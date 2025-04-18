#include "egpch.h"
#include "UIGraph.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Editors/GraphEditor.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/UI/Graphs/UIAnimationStateMachineGraph.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"

namespace Eagle
{
    static inline ImRect ImGui_GetItemRect()
    {
        return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }

    static inline ImRect ImRect_Expanded(const ImRect& rect, float x, float y)
    {
        auto result = rect;
        result.Min.x -= x;
        result.Min.y -= y;
        result.Max.x += x;
        result.Max.y += y;
        return result;
    }

    static bool DragVec4(glm::vec4& value)
    {
        bool bChanged = false;

        constexpr float s_ItemWidth = 50.f;
        const auto& style = ImGui::GetStyle();
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImVec2 pos = start;

        const char* labels[4] = { "##vX", "##vY", "##vZ", "##vW" };

        ImGui::PushItemWidth(s_ItemWidth);
        for (uint32_t i = 0; i < 4; ++i)
        {
            ImGui::SetCursorScreenPos(pos);

            bChanged |= ImGui::DragFloat(labels[i], &value[i], 0.01f);

            pos.x = start.x + (style.ItemInnerSpacing.x + s_ItemWidth) * (i + 1);
        }
        ImGui::PopItemWidth();

        return bChanged;
    }

    static void UpdateNodeSize(const ed::Detail::Settings& settings, Node& node)
    {
        const ed::Detail::NodeSettings* nodeSettings = settings.FindNode(node.ID);
        if (nodeSettings)
            node.Size = nodeSettings->m_Size;
    }

    static bool IsVariableType(PinType type)
    {
        switch (type)
        {
        case PinType::Flow:
        case PinType::StateFlow:
        case PinType::Pose:
        case PinType::Function:
        case PinType::Delegate: return false;
        }

        return true;
    }

    static Ref<GraphVariable> ProcessVariable(const Ref<GraphVariable>& var, const std::string& varName, bool bCloneVars, VariablesMap& outVariables)
    {
        Ref<GraphVariable> resultVar;
        if (bCloneVars)
        {
            if (auto it = outVariables.find(varName); it != outVariables.end())
                resultVar = it->second;
            else
                resultVar = CopyVarByType(var);
        }
        else
            resultVar = var;

        if (resultVar)
            outVariables[varName] = resultVar;

        return resultVar;
    }

    static bool AreFlowAndStateFlowPinTypes(PinType a, PinType b)
    {
        const bool bAIsFlow = a == PinType::Flow || a == PinType::StateFlow;
        const bool bBIsFlow = b == PinType::Flow || b == PinType::StateFlow;
        return bAIsFlow && bBIsFlow;
    }

    static PinType GetPinType(GraphVariableType type)
    {
        switch (type)
        {
        case GraphVariableType::Bool: return PinType::Bool;
        case GraphVariableType::Float: return PinType::Float;
        case GraphVariableType::Animation: return PinType::Object;
        case GraphVariableType::String: return PinType::String;
        case GraphVariableType::Vec4: return PinType::Vec4;
        }
        EG_CORE_ASSERT(false);
        return PinType::Object;
    }

    static GraphNodeType NodeTypeToGraphNodeType(NodeType type)
    {
        switch (type)
        {
            case NodeType::Variable: return GraphNodeType::Variable;
            case NodeType::PoseCache: return GraphNodeType::PoseCache;
            case NodeType::PoseCacheGetter: return GraphNodeType::PoseCacheGetter;
            default: return GraphNodeType::Node;
        }
    }

    UIGraph::UIGraph(GraphEditor& editor, const std::string_view name)
        : m_Editor(editor)
	{
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();

        m_GraphData.Editor = ed::CreateEditor(&editor.GetConfig());
        m_GraphData.Name = name;

        ed::SetCurrentEditor(m_GraphData.Editor);

        // If deserialization has failed, focus
        if (m_GraphData.Nodes.size() == 1)
            ed::NavigateToContent();

        BuildNodes();

        ed::SetCurrentEditor(editorBefore);
	}

    UIGraph::~UIGraph()
    {
        if (m_GraphData.Editor)
            ed::DestroyEditor(m_GraphData.Editor);
    }

    void UIGraph::OnEvent(Event& e)
    {
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        if (e.GetEventType() == EventType::KeyPressed)
        {
            KeyPressedEvent& keyPressed = (KeyPressedEvent&)e;
            if (keyPressed.GetKey() == Key::F2)
            {
                int selectedCount = ed::GetSelectedObjectCount();
                if (selectedCount == 1)
                {
                    ed::NodeId selectedNodeID;
                    ed::GetSelectedNodes(&selectedNodeID, selectedCount);
                    if (Node* node = FindNode(selectedNodeID))
                    {
                        OnStartedRenamingNode(node);
                        e.Handled = true;
                    }
                }

            }
        }

        ed::SetCurrentEditor(editorBefore);
    }

    void UIGraph::OnImGuiRender(bool* pOpen)
    {
        ed::SetCurrentEditor(m_GraphData.Editor);

        // Select variable in the list
        if (ed::HasSelectionChanged())
        {
            std::vector<ed::NodeId> selectedNodes;
            selectedNodes.resize(ed::GetSelectedObjectCount());
            int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
            if (selectedNodes.size() > 0)
            {
                Node* selected = FindNode(selectedNodes[0]);
                if (selected && selected->Type == NodeType::Variable)
                    m_Editor.SelectVariable(selected->GetName());
            }
        }

        ed::Begin(m_GraphData.Name.c_str());

        ProcessPendingDeletion();
        HandleDragDrop();
        UpdateTouch();

        m_CursorTopLeft = ImGui::GetCursorScreenPos();
        DrawNodes();
        DrawLinks();
        HandleCreatingDeletion();
        HandleIfPopupShouldOpen();

        // Draw popups
        {
            ed::Suspend();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

            DrawNodeContextPopup();
            DrawPinContextPopup();
            DrawLinkContextPopup();
            DrawCreateNewNodePopup();

            ImGui::PopStyleVar();
            ed::Resume();
        }

        ed::End();
    }

    void UIGraph::Parse(Node* node, bool bCloneVars, VariablesMap& outVariables, std::unordered_set<UIGraph*>& compiledGraphs)
    {
        if (node->GraphNode)
            node->GraphNode->ResetInputs();

        const size_t baseInputsCount = node->InputPins.size();
        for (size_t baseNodeInputIdx = 0; baseNodeInputIdx < baseInputsCount; ++baseNodeInputIdx)
        {
            // Process input pins
            auto& pinInputs = node->InputsPerPin[baseNodeInputIdx];
            if (pinInputs.empty()) // No inputs connected, try to set default value
            {
                const Pin& pin = node->InputPins[baseNodeInputIdx];
                if (pin.DefaultValue)
                    node->GraphNode->SetInput(pin.DefaultValue, baseNodeInputIdx);
                continue;
            }

            for (auto& pinInput : pinInputs)
            {
                auto& input = pinInput.NodeID;
                if (!input) // Safety check again
                {
                    const Pin& pin = node->InputPins[baseNodeInputIdx];
                    if (pin.DefaultValue)
                        node->GraphNode->SetInput(pin.DefaultValue, baseNodeInputIdx);
                    continue;
                }

                Node* connectedNode = FindNode(input);
                if (connectedNode->Graph) // Compile graph and set its result as an input
                {
                    compiledGraphs.emplace(connectedNode->Graph.get());
                    node->GraphNode->SetInput(connectedNode->Graph->Compile_Internal(connectedNode->Graph->GetOutputNode(), outVariables, compiledGraphs), baseNodeInputIdx);
                    continue;
                }
                else if (connectedNode->Type == NodeType::Variable)
                {
                    const auto& varName = connectedNode->GetName();
                    auto var = ProcessVariable(m_Editor.GetVariable(varName), varName, bCloneVars, outVariables);
                    node->GraphNode->SetInput(var, baseNodeInputIdx);
                    continue;
                }
                else if (connectedNode->Type == NodeType::PoseCacheGetter)
                {
                    auto& cachedNode = connectedNode->CachedNode;
                    if (cachedNode.Owner)
                    {
                        Node* cached = cachedNode.Owner->FindNode(cachedNode.NodeID);
                        if (cached && cached->GraphNode)
                        {
                            node->GraphNode->SetInput(cached->GraphNode, baseNodeInputIdx);
                            // We need to compile graph that owns the cache, otherwise cache will remain empty.
                            if (compiledGraphs.count(cachedNode.Owner) == 0u)
                            {
                                compiledGraphs.emplace(connectedNode->Owner);
                                cachedNode.Owner->Compile_Internal(cached, outVariables, compiledGraphs);
                            }
                        }
                    }

                    continue;
                }
                else if (!connectedNode->GraphNode)
                    continue;

                auto& graphNode = connectedNode->GraphNode;
                EG_CORE_ASSERT(graphNode);
                node->GraphNode->SetInput(graphNode, baseNodeInputIdx);

                graphNode->ResetInputs();
                for (auto& inputs : connectedNode->InputsPerPin)
                {
                    const size_t inputsCount = inputs.size();
                    for (size_t i = 0; i < inputsCount; ++i)
                    {
                        Node* inputNode = FindNode(inputs[i].NodeID);
                        if (inputNode)
                        {
                            if (inputNode->Graph)
                            {
                                compiledGraphs.emplace(inputNode->Graph.get());
                                auto graph = inputNode->Graph->Compile_Internal(inputNode->Graph->GetOutputNode(), outVariables, compiledGraphs);
                                graphNode->SetInput(graph, i);
                            }
                            else if (inputNode->GraphNode)
                                graphNode->SetInput(inputNode->GraphNode, i);
                            else if (inputNode->Type == NodeType::Variable)
                            {
                                const auto& varName = inputNode->GetName();
                                auto var = ProcessVariable(m_Editor.GetVariable(varName), varName, bCloneVars, outVariables);
                                graphNode->SetInput(var, i);
                            }
                        }
                        else
                        {
                            const auto& inputPin = connectedNode->InputPins[i];
                            if (inputPin.DefaultValue)
                                graphNode->SetInput(inputPin.DefaultValue, i);
                        }
                    }
                }

                Parse(connectedNode, bCloneVars, outVariables, compiledGraphs);
            }
        }
    }

    void UIGraph::OnStartedRenamingNode(Node* node)
    {
        UpdateNodeSize(m_GraphData.Editor->GetSettings(), *node);
        m_RenamingNodeTemp = node->GetName();
        node->bEditing = true;
    }

    Ref<GraphNode> UIGraph::Compile_Internal(Node* outputNode, VariablesMap& outUsedVars, std::unordered_set<UIGraph*>& compiledGraphs)
    {
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        Parse(outputNode, true, outUsedVars, compiledGraphs);
        Ref<GraphNode> compiledNode = outputNode->GraphNode;

        ed::SetCurrentEditor(editorBefore);

        return compiledNode;
    }

    Ref<GraphNode> UIGraph::Compile(VariablesMap& outUsedVars)
    {
        Node* outputNode = GetOutputNode();
        std::unordered_set<UIGraph*> compiledGraphs;
        return Compile_Internal(outputNode, outUsedVars, compiledGraphs);
    }
    
    void UIGraph::SetupNodeFactory()
    {
        GraphNodeFactory::FillCommonNodes(m_NodeFactory);
    }
    
    void UIGraph::ProcessPendingDeletion()
    {
        for (auto& pendingNodeId : m_GraphData.NodesPendingDeletion)
        {
            auto it = m_GraphData.Nodes.find(pendingNodeId);
            if (it != m_GraphData.Nodes.end())
            {
                auto& node = it->second;
                if (node.Type == NodeType::Variable)
                {
                    auto it = m_VarToNodesMapping.find(node.GetName());
                    if (it != m_VarToNodesMapping.end())
                    {
                        auto& varNodes = it->second;
                        auto nodeIt = std::find(varNodes.begin(), varNodes.end(), node.ID);
                        if (nodeIt != varNodes.end())
                            varNodes.erase(nodeIt);
                    }
                }

                m_GraphData.Nodes.erase(it);
            }
        }
        if (!m_GraphData.NodesPendingDeletion.empty())
        {
            BuildNodes();
            m_GraphData.NodesPendingDeletion.clear();
        }
    }

    void UIGraph::DrawNodes()
    {
        const auto& headerTexture = m_Editor.GetHeaderTexture();
        util::BlueprintNodeBuilder builder(m_Editor.GetHeaderTextureID(), (int)headerTexture->GetWidth(), (int)headerTexture->GetHeight());

        // BP
        for (auto& [_, node] : m_GraphData.Nodes)
        {
            if (node.Type != NodeType::Blueprint && node.Type != NodeType::Simple && node.Type != NodeType::Variable &&
                node.Type != NodeType::PoseCache && node.Type != NodeType::PoseCacheGetter && node.Type != NodeType::StateMachine)
                continue;

            HandleBPNode(builder, node, m_NewLinkPin);
        }

        // State machine states
        for (auto& [_, node] : m_GraphData.Nodes)
        {
            if (node.Type != NodeType::StateMachineState)
                continue;

            HandleStateNode(node, m_NewLinkPin);
        }

        // Comment
        for (auto& [_, node] : m_GraphData.Nodes)
        {
            if (node.Type != NodeType::Comment)
                continue;

            HandleCommentNode(node, m_NewLinkPin);
        }
    }

    void UIGraph::DrawLinks()
    {
        for (auto& [_, link] : m_GraphData.Links)
            ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
    }

    void UIGraph::DrawNodeContextPopup()
    {
        if (ImGui::BeginPopup("Node Context Menu"))
        {
            auto node = FindNode(m_ContextNodeId);

            ImGui::TextUnformatted(node->GetName().c_str());
            ImGui::Separator();
            if (node)
            {
                //ImGui::Text("ID: %p", node->ID.AsPointer());
                //ImGui::Text("Type: %s", Utils::GetEnumName(node->Type));
                //ImGui::Text("Input Pins: %d", (int)node->InputPins.size());
                //ImGui::Text("Output Pins: %d", (int)node->OutputPins.size());
                //ImGui::Separator();

                if (node->Type == NodeType::Variable || node->Type == NodeType::PoseCache || node->Graph)
                {
                    if (ImGui::MenuItem("Rename", "F2"))
                        OnStartedRenamingNode(node);
                    ImGui::Separator();
                }
            }
            else
                ImGui::Text("Unknown node: %p", m_ContextNodeId.AsPointer());

            // Delete node
            {
                ImGui::Separator();
                Node* node = FindNode(m_ContextNodeId);
                if (node && node->bDeletable && ImGui::MenuItem("Delete", "Del"))
                    DeleteNode(node);
            }

            ImGui::EndPopup();
        }
    }

    void UIGraph::DrawPinContextPopup()
    {
        if (ImGui::BeginPopup("Pin Context Menu"))
        {
            auto pin = FindPin(m_ContextPinId);

            ImGui::TextUnformatted("Pin Context Menu");
            ImGui::Separator();
            if (pin)
            {
                ImGui::Text("ID: %p", pin->ID.AsPointer());
                if (const Node* node = FindNode(pin->NodeID))
                    ImGui::Text("Node: %p", node->ID.AsPointer());
                else
                    ImGui::Text("Node: %s", "<none>");
            }
            else
                ImGui::Text("Unknown pin: %p", m_ContextPinId.AsPointer());

            ImGui::EndPopup();
        }
    }

    void UIGraph::DrawLinkContextPopup()
    {
        if (ImGui::BeginPopup("Link Context Menu"))
        {
            auto link = FindLink(m_ContextLinkId);

            ImGui::TextUnformatted("Link Context Menu");
            ImGui::Separator();
            if (link)
            {
                ImGui::Text("ID: %p", link->ID.AsPointer());
                ImGui::Text("From: %p", link->StartPinID.AsPointer());
                ImGui::Text("To: %p", link->EndPinID.AsPointer());
            }
            else
                ImGui::Text("Unknown link: %p", m_ContextLinkId.AsPointer());
            ImGui::Separator();
            if (ImGui::MenuItem("Delete"))
                ed::DeleteLink(m_ContextLinkId);
            ImGui::EndPopup();
        }
    }

    void UIGraph::DrawCreateNewNodePopup()
    {
        if (ImGui::BeginPopup("Create New Node"))
        {
            auto newNodePostion = m_CreateNodeOpenPopupPos;

            Node* node = nullptr;
            if (m_bShowCreateNewVarInPopup)
            {
                if (m_NewNodeLinkPin && IsVariableType(m_NewNodeLinkPin->Type) && ImGui::MenuItem("Create new variable"))
                {
                    std::string varName;
                    switch (m_NewNodeLinkPin->Type)
                    {
                    case PinType::Bool:
                        varName = m_Editor.CreateNewVar<GraphVariableBool>(m_NewNodeLinkPin->DefaultValue);
                        break;
                    case PinType::Float:
                        varName = m_Editor.CreateNewVar<GraphVariableFloat>(m_NewNodeLinkPin->DefaultValue);
                        break;
                    case PinType::Vec4:
                        varName = m_Editor.CreateNewVar<GraphVariableVec4>(m_NewNodeLinkPin->DefaultValue);
                        break;
                    case PinType::String:
                        varName = m_Editor.CreateNewVar<GraphVariableString>(m_NewNodeLinkPin->DefaultValue);
                        break;
                    case PinType::Object:
                        varName = m_Editor.CreateNewVar<GraphVariableAnimation>(m_NewNodeLinkPin->DefaultValue);
                        break;
                    }
                    if (!varName.empty())
                    {
                        node = &GraphNodeFactory::SpawnVarNode(*this, varName, m_NewNodeLinkPin->Type);
                        m_Editor.SelectVariable(varName);
                    }

                    ImGui::Separator();
                }
            }

            for (const auto& [category, factory] : m_NodeFactory)
            {
                UI::TextWithSeparator(category);
                for (const auto& [name, func] : factory)
                {
                    if (ImGui::MenuItem(name.c_str()))
                    {
                        Node& createdNode = (*func)(*this, name);
                        node = &createdNode;
                    }
                }
            }

#if 0
            UI::TextWithSeparator("Test nodes");
            if (ImGui::MenuItem("Input Action"))
                node = SpawnInputActionNode();
            if (ImGui::MenuItem("Output Action"))
                node = SpawnOutputActionNode();
            if (ImGui::MenuItem("Branch"))
                node = SpawnBranchNode();
            if (ImGui::MenuItem("Do N"))
                node = SpawnDoNNode();
            if (ImGui::MenuItem("Set Timer"))
                node = SpawnSetTimerNode();
            if (ImGui::MenuItem("Weird"))
                node = SpawnWeirdNode();
            if (ImGui::MenuItem("Trace by Channel"))
                node = SpawnTraceByChannelNode();
            if (ImGui::MenuItem("Print String"))
                node = SpawnPrintStringNode();
            ImGui::Separator();
            if (ImGui::MenuItem("Sequence"))
                node = SpawnTreeSequenceNode();
            if (ImGui::MenuItem("Move To"))
                node = SpawnTreeTaskNode();
            if (ImGui::MenuItem("Random Wait"))
                node = SpawnTreeTask2Node();
            ImGui::Separator();
            if (ImGui::MenuItem("Message"))
                node = SpawnMessageNode();
            ImGui::Separator();
            if (ImGui::MenuItem("Transform"))
                node = SpawnHoudiniTransformNode();
            if (ImGui::MenuItem("Group"))
                node = SpawnHoudiniGroupNode();
#endif

            // Variables
            if (CanSpawnVariables())
            {
                const auto& variables = m_Editor.GetVariables();
                if (variables.size())
                {
                    UI::TextWithSeparator("Variables");

                    for (const auto& [name, var] : variables)
                    {
                        if (ImGui::MenuItem(name.c_str()))
                        {
                            node = &GraphNodeFactory::SpawnVarNode(*this, name, GetPinType(var->GetType()));
                        }
                    }
                }

                const auto& poseCacheNodes = m_Editor.GetPoseCacheNodes();
                if (!poseCacheNodes.empty())
                {
                    UI::TextWithSeparator("Caches");

                    for (const auto& [owner, nodeID] : poseCacheNodes)
                    {
                        Node* cacheNode = owner->FindNode(nodeID);
                        if (!cacheNode)
                            continue;

                        ImGui::PushID(cacheNode);
                        if (ImGui::MenuItem(cacheNode->Name.c_str()))
                        {
                            node = &GraphNodeFactory::SpawnCachePoseGetterNode(*this, cacheNode);
                        }
                        ImGui::PopID();
                    }
                }
            }

            if (node)
            {
                m_CreateNewNode = false;
                HandleNodeCreation(*node, newNodePostion, m_NewNodeLinkPin);
            }

            ImGui::EndPopup();
        }
        else
            m_CreateNewNode = false;
    }

    GraphSerializationData UIGraph::Serialize() const
    {
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        const auto& settings = m_GraphData.Editor->GetSettings();

        GraphSerializationData result;
        result.Name = m_GraphData.Name;
        result.ScrollOffset = glm::vec2(settings.m_ViewScroll.x, settings.m_ViewScroll.y);
        result.Zoom = settings.m_ViewZoom;
        result.ID = m_ID;

        for (const auto& [_, node] : m_GraphData.Nodes)
        {
            if (node.Graph)
                result.Subgraphs.emplace_back(node.Graph->Serialize());

            ImVec2 pos = ed::GetNodePosition(node.ID);
            ImVec2 size = ed::GetNodeSize(node.ID);
            GraphNodeSerializationData nodeData;
            nodeData.OwnerID = m_ID;
            nodeData.Name = node.Name;
            nodeData.Type = NodeTypeToGraphNodeType(node.Type);
            nodeData.Position = glm::vec2(pos.x, pos.y);
            nodeData.Size = glm::vec2(size.x, size.y);
            nodeData.NodeID = (uint32_t)node.ID.Get();
            nodeData.CachedOwnerID = node.CachedNode.Owner ? node.CachedNode.Owner->m_ID : GUID(0, 0);
            nodeData.CachedNodeID = (uint32_t)node.CachedNode.NodeID.Get();
            nodeData.UserData = node.UserData;

            // Inputs default values
            for (const auto& inputPin : node.InputPins)
                nodeData.DefaultValues.emplace_back(inputPin.DefaultValue);

            // Outputs
            {
                size_t i = 1;
                for (const auto& pinOutputs : node.OutputsPerPin)
                {
                    for (const auto& outputData : pinOutputs)
                    {
                        if (!outputData.NodeID)
                            continue;

                        const Node* connectedTo = FindNode(outputData.NodeID);
                        if (!connectedTo)
                            continue;

                        GraphConnectionData connectionData;
                        connectionData.NodeID = (uint32_t)connectedTo->ID.Get();
                        connectionData.PinIndex = outputData.PinIndex;
                        nodeData.OutputConnections.push_back(connectionData);
                    }
                }
            }

            result.Nodes.push_back(nodeData);
        }
        ed::SetCurrentEditor(editorBefore);

        return result;
    }

    void UIGraph::Deserialize_Internal(const GraphEditorSerializationData& editorData, const GraphSerializationData& data, std::vector<UIGraph*>& deserializedGraphs, std::vector<PoseCacheGetterDeserializationData>& poseCacheGetterData)
    {
        deserializedGraphs.push_back(this);

        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        m_GraphData.Name = data.Name;
        m_GraphData.Editor->SetViewScroll(ImVec2(data.ScrollOffset.x, data.ScrollOffset.y));
        m_GraphData.Editor->SetViewZoom(data.Zoom);
        m_ID = data.ID;

        // Create nodes
        int maxNodeID = m_GraphData.NextId;
        for (const auto& nodeData : data.Nodes)
        {
            if (auto nodeID = GetOutputNodeID(); nodeID.Get() == nodeData.NodeID) // Special case for the base node
            {
                ed::SetNodePosition(nodeID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                Node* node = GetOutputNode();
                const size_t defaultValuesCount = nodeData.DefaultValues.size();
                const size_t inputPinsCount = node->InputPins.size();
                if (inputPinsCount == nodeData.DefaultValues.size()) // Should always match, but this check is here just in case
                {
                    for (size_t i = 0; i < inputPinsCount; ++i)
                        node->InputPins[i].DefaultValue = nodeData.DefaultValues[i];
                }
                continue;
            }

            m_GraphData.NextId = int(nodeData.NodeID); // So that the node is created with the required ID

            if (nodeData.Type == GraphNodeType::Variable)
            {
                if (const auto& var = m_Editor.GetVariable(nodeData.Name))
                {
                    Node& createdNode = GraphNodeFactory::SpawnVarNode(*this, nodeData.Name, GetPinType(var->GetType()));
                    ed::SetNodePosition(createdNode.ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                }
            }
            else if (nodeData.Type == GraphNodeType::PoseCache)
            {
                Node& createdNode = GraphNodeFactory::SpawnCachePoseNode(*this, nodeData.Name);
                ed::SetNodePosition(createdNode.ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
            }
            else if (nodeData.Type == GraphNodeType::PoseCacheGetter)
            {
                // It's nullptr because we might not have all `CachePose` nodes created,
                // so we temporarily set it to nullptr, and at the end of deserialization, assign correct values
                const Node* cached = nullptr;
                Node& createdNode = GraphNodeFactory::SpawnCachePoseGetterNode(*this, cached);
                ed::SetNodePosition(createdNode.ID, ImVec2(nodeData.Position.x, nodeData.Position.y));

                auto& data = poseCacheGetterData.emplace_back();
                data.Owner = m_ID;
                data.ID = createdNode.ID;
                data.CacheNodeID = nodeData.CachedNodeID;
                data.CacheNodeOwner = nodeData.CachedOwnerID;
            }
            else if (nodeData.Type == GraphNodeType::Node)
            {
                for (const auto& [unused, factory] : m_NodeFactory)
                {
                    auto it = factory.find(nodeData.Name);
                    if (it != factory.end())
                    {
                        auto func = it->second;
                        Node& createdNode = (*func)(*this, nodeData.Name);
                        createdNode.Size = ImVec2(nodeData.Size.x, nodeData.Size.y);
                        ed::SetNodePosition(createdNode.ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                        ed::SetGroupSize(createdNode.ID, createdNode.Size);
                        createdNode.UserData = nodeData.UserData;

                        // Set default values
                        const size_t inputPinsCount = createdNode.InputPins.size();
                        if (inputPinsCount == nodeData.DefaultValues.size()) // Should always match, but this check is here just in case
                        {
                            for (size_t i = 0; i < inputPinsCount; ++i)
                                createdNode.InputPins[i].DefaultValue = nodeData.DefaultValues[i];
                        }

                        if (createdNode.Graph)
                        {
                            // It's a graph, deserialize it as well
                            const GraphSerializationData* createdNodeData = nullptr;
                            for (const auto& graphData : data.Subgraphs)
                            {
                                if (graphData.Name == createdNode.GetName())
                                {
                                    createdNodeData = &graphData;
                                    break;
                                }
                            }
                            if (createdNodeData)
                                createdNode.Graph->Deserialize_Internal(editorData, *createdNodeData, deserializedGraphs, poseCacheGetterData);
                        }
                        break;
                    }
                }
            }

            if (m_GraphData.NextId > maxNodeID)
                maxNodeID = m_GraphData.NextId; // Save the max node ID so that we can set `m_NextId` to it after all nodes are created
        }

        m_GraphData.NextId = maxNodeID;

        // Link nodes
        for (const auto& nodeData : data.Nodes)
        {
            for (const auto& connection : nodeData.OutputConnections)
            {
                if (Node* connectToNode = FindNode(connection.NodeID))
                {
                    Node* currentNode = FindNode(nodeData.NodeID);
                    if (currentNode)
                    {
                        const Pin* startPin = &currentNode->OutputPins[0];
                        const Pin* endPin = &connectToNode->InputPins[connection.PinIndex];

                        AddLink(startPin, endPin);
                    }
                }
            }
        }

        ed::SetCurrentEditor(editorBefore);
    }

    void UIGraph::Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data)
    {
        std::vector<PoseCacheGetterDeserializationData> poseCacheGetterData; // Creation is delayed since we need to wait till all `PoseCache` nodes are created
        std::vector<UIGraph*> deserializedGraphs;
        poseCacheGetterData.reserve(4);
        deserializedGraphs.reserve(4);

        auto GetGraphByID = [&deserializedGraphs](GUID id) -> UIGraph*
        {
            for (auto& graph : deserializedGraphs)
            {
                if (graph->GetID() == id)
                    return graph;
            }

            return nullptr;
        };

        Deserialize_Internal(editorData, data, deserializedGraphs, poseCacheGetterData);

        for (const auto& data : poseCacheGetterData)
        {
            UIGraph* getterOwner = GetGraphByID(data.Owner);
            if (!getterOwner)
                continue;

            UIGraph* cacheOwner = GetGraphByID(data.CacheNodeOwner);
            if (!cacheOwner)
                continue;

            const Node* cacheNode = cacheOwner->FindNode(data.CacheNodeID);
            if (!cacheNode)
                continue;

            Node* cacheGetterNode = getterOwner->FindNode(data.ID);
            if (!cacheGetterNode)
                continue;

            cacheGetterNode->SetName(cacheNode->GetName());
            cacheGetterNode->CachedNode.Owner = cacheNode->Owner;
            cacheGetterNode->CachedNode.NodeID = cacheNode->ID;
        }
    }

    void UIGraph::DrawPinIcon(const Pin& pin, bool connected, int alpha)
    {
        using ax::Widgets::IconType;

        IconType iconType;
        ImColor  color = GetIconColor(pin.Type);
        color.Value.w = alpha / 255.0f;
        switch (pin.Type)
        {
        case PinType::Flow:      iconType = IconType::Flow;   break;
        case PinType::StateFlow: iconType = IconType::Flow;   break;
        case PinType::Bool:      iconType = IconType::Circle; break;
        case PinType::Int:       iconType = IconType::Circle; break;
        case PinType::Float:     iconType = IconType::Circle; break;
        case PinType::Vec4:      iconType = IconType::Circle; break;
        case PinType::String:    iconType = IconType::Circle; break;
        case PinType::Object:    iconType = IconType::Circle; break;
        case PinType::Pose:      iconType = IconType::Circle; break;
        case PinType::Function:  iconType = IconType::Circle; break;
        case PinType::Delegate:  iconType = IconType::Square; break;
        default:
            EG_CORE_ASSERT(false);
            return;
        }

        ax::Widgets::Icon(ImVec2(static_cast<float>(m_GraphData.PinIconSize), static_cast<float>(m_GraphData.PinIconSize)), iconType, connected, color, ImColor(32, 32, 32, alpha));
    }

    void UIGraph::HandleDragDrop()
    {
        // Drop event
        if (ImGui::BeginDragDropTarget())
        {
            if (CanSpawnVariables())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GraphEditor::GetVarDragDropTag()))
                {
                    const std::string varName = (const char*)payload->Data;
                    auto var = m_Editor.GetVariable(varName);
                    if (var)
                    {
                        Node& node = GraphNodeFactory::SpawnVarNode(*this, varName, GetPinType(var->GetType()));
                        m_CreateNewNode = false;
                        HandleNodeCreation(node, ImGui::GetMousePos(), nullptr);
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }
    }

    void UIGraph::HandleIfPopupShouldOpen()
    {
        auto openPopupPosition = ImGui::GetMousePos();
        ed::Suspend();
        if (ed::ShowNodeContextMenu(&m_ContextNodeId))
            ImGui::OpenPopup("Node Context Menu");
        else if (ed::ShowPinContextMenu(&m_ContextPinId))
            ImGui::OpenPopup("Pin Context Menu");
        else if (ed::ShowLinkContextMenu(&m_ContextLinkId))
            ImGui::OpenPopup("Link Context Menu");
        else if (ed::ShowBackgroundContextMenu())
        {
            ImGui::OpenPopup("Create New Node");
            m_NewNodeLinkPin = nullptr;
            m_bGetPopupPos = true;
            m_bShowCreateNewVarInPopup = false;
        }
        if (m_bGetPopupPos)
        {
            m_CreateNodeOpenPopupPos = openPopupPosition;
            m_bGetPopupPos = false;
        }
        ed::Resume();
    }

    void UIGraph::HandleCreatingDeletion()
    {
        if (m_CreateNewNode)
        {
            ImGui::SetCursorScreenPos(m_CursorTopLeft);
            return;
        }

        if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f))
        {
            ed::PinId startPinId = 0, endPinId = 0;
            if (ed::QueryNewLink(&startPinId, &endPinId))
            {
                auto startPin = FindPin(startPinId);
                auto endPin = FindPin(endPinId);

                m_NewLinkPin = startPin ? startPin : endPin;

                if (startPin && endPin)
                {
                    if (startPin->Kind == PinKind::Input)
                    {
                        std::swap(startPin, endPin);
                        std::swap(startPinId, endPinId);
                    }

                    if (!ProcessNewLinkRejection(*startPin, *endPin))
                    {
                        ShowLabel("+ Create Link", ImColor(32, 45, 32, 180));
                        if (ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
                        {
                            // Disconnect existing link
                            const bool bDisconnectStart = startPin->Type == PinType::Flow;
                            // Flow pin should always have one link
                            if ((AllowMultipleLinksToInput() == false) || bDisconnectStart)
                            {
                                auto pinIdToDisconnect = bDisconnectStart ? startPinId : endPinId;
                                auto it = std::find_if(m_GraphData.Links.begin(), m_GraphData.Links.end(), [bDisconnectStart, pinIdToDisconnect](const auto& link)
                                {
                                    return (bDisconnectStart ? link.second.StartPinID : link.second.EndPinID) == pinIdToDisconnect;
                                });
                                if (it != m_GraphData.Links.end())
                                {
                                    OnLinkDeleted(it->second);
                                    m_GraphData.Links.erase(it);
                                }
                            }

                            AddLink(startPin, endPin);
                        }
                    }
                }
            }

            ed::PinId pinId = 0;
            if (ed::QueryNewNode(&pinId))
            {
                m_NewLinkPin = FindPin(pinId);
                if (m_NewLinkPin)
                    ShowLabel("+ Create Node", ImColor(32, 45, 32, 180));

                if (ed::AcceptNewItem())
                {
                    m_CreateNewNode = true;
                    m_NewNodeLinkPin = FindPin(pinId);
                    m_NewLinkPin = nullptr;
                    ed::Suspend();
                    ImGui::OpenPopup("Create New Node");
                    ed::Resume();
                    m_bGetPopupPos = true;
                    m_bShowCreateNewVarInPopup = true;
                }
            }
        }
        else
            m_NewLinkPin = nullptr;

        ed::EndCreate();

        if (ed::BeginDelete())
        {
            ed::NodeId nodeId = 0;
            while (ed::QueryDeletedNode(&nodeId))
            {
                Node* node = FindNode(nodeId);
                if (node && node->bDeletable && ed::AcceptDeletedItem())
                {
                    auto it = m_GraphData.Nodes.find(nodeId);
                    if (it != m_GraphData.Nodes.end())
                        DeleteNode(&(it->second));
                }
            }

            ed::LinkId linkId = 0;
            while (ed::QueryDeletedLink(&linkId))
            {
                if (ed::AcceptDeletedItem())
                {
                    auto it = std::find_if(m_GraphData.Links.begin(), m_GraphData.Links.end(), [linkId](const auto& link) { return link.second.ID == linkId; });
                    if (it != m_GraphData.Links.end())
                    {
                        OnLinkDeleted(it->second);
                        m_GraphData.Links.erase(it);
                    }
                }
            }
        }
        ed::EndDelete();

        ImGui::SetCursorScreenPos(m_CursorTopLeft);
    }

    Node* UIGraph::FindNode(ed::NodeId id)
    {
        auto it = m_GraphData.Nodes.find(id);
        return it != m_GraphData.Nodes.end() ? &(it->second) : nullptr;
    }

    const Node* UIGraph::FindNode(ed::NodeId id) const
    {
        auto it = m_GraphData.Nodes.find(id);
        return it != m_GraphData.Nodes.end() ? &(it->second) : nullptr;
    }

    Link* UIGraph::FindLink(ed::LinkId id)
    {
        for (auto& [_, link] : m_GraphData.Links)
            if (link.ID == id)
                return &link;

        return nullptr;
    }

    Pin* UIGraph::FindPin(ed::PinId id)
    {
        if (!id)
            return nullptr;

        for (auto& [_, node] : m_GraphData.Nodes)
        {
            for (auto& pin : node.InputPins)
                if (pin.ID == id)
                    return &pin;

            for (auto& pin : node.OutputPins)
                if (pin.ID == id)
                    return &pin;
        }

        return nullptr;
    }

    bool UIGraph::IsPinLinked(ed::PinId id)
    {
        if (!id)
            return false;

        for (auto& [_, link] : m_GraphData.Links)
            if (link.StartPinID == id || link.EndPinID == id)
                return true;

        return false;
    }

    bool UIGraph::CanCreateLink(Pin* a, Pin* b)
    {
        if (!a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->NodeID == b->NodeID)
            return false;

        return true;
    }

    void UIGraph::BuildNode(Node& node)
    {
        uint32_t idx = 0;
        for (auto& input : node.InputPins)
        {
            input.NodeID = node.ID;
            input.Kind = PinKind::Input;
            input.Index = idx++;
        }

        idx = 0;
        for (auto& output : node.OutputPins)
        {
            output.NodeID = node.ID;
            output.Kind = PinKind::Output;
            output.Index = idx++;
        }

        node.InputsPerPin.resize(node.InputPins.size());
        node.OutputsPerPin.resize(node.OutputPins.size());
    }

    void UIGraph::BuildNodes()
    {
        for (auto& [_, node] : m_GraphData.Nodes)
            BuildNode(node);
    }

    void UIGraph::OnVariableDeleted(const std::string& var)
    {
        auto it = m_VarToNodesMapping.find(var);
        if (it != m_VarToNodesMapping.end())
        {
            const auto& nodes = it->second;
            for (const auto& nodeID : nodes)
            {
                Node* node = FindNode(nodeID);
                if (!node)
                    continue;

                // Disconnect existing links
                while (true)
                {
                    auto it = std::find_if(m_GraphData.Links.begin(), m_GraphData.Links.end(), [pinID = node->OutputPins[0].ID](const auto& link) { return link.second.StartPinID == pinID; });
                    if (it == m_GraphData.Links.end())
                        break;

                    OnLinkDeleted(it->second);
                    m_GraphData.Links.erase(it);
                }

                DeleteNode(node);
            }

            m_VarToNodesMapping.erase(it);
        }

        for (auto& nodeID : m_NodesWithGraph)
        {
            if (Node* node = FindNode(nodeID))
                node->Graph->OnVariableDeleted(var);
        }
    }

    void UIGraph::OnVariableRenamed(const std::string& varName, const std::string& newName)
    {
        auto varNodes = std::move(m_VarToNodesMapping[varName]);
        for (auto& id : varNodes)
        {
            Node* node = FindNode(id);
            if (node)
                node->SetName(newName);
        }
        m_VarToNodesMapping.erase(varName);
        m_VarToNodesMapping[newName] = std::move(varNodes);

        for (auto& nodeID : m_NodesWithGraph)
        {
            if (Node* node = FindNode(nodeID))
                node->Graph->OnVariableRenamed(varName, newName);
        }
    }

    void UIGraph::OnNodeAdded(Node& node)
    {
        if (node.Type == NodeType::Variable)
            m_VarToNodesMapping[node.GetName()].push_back(node.ID);
        else if (node.Type == NodeType::PoseCache)
            m_Editor.GetPoseCacheNodes().emplace_back(this, node.ID);

        if (node.Graph)
            m_NodesWithGraph.push_back(node.ID);
    }

    void UIGraph::OnNodeDeleted(const Node& node)
    {
        if (node.Graph)
        {
            auto it = std::find_if(m_NodesWithGraph.begin(), m_NodesWithGraph.end(), [node](const ed::NodeId& a)
            {
                return a == node.ID;
            });
            if (it != m_NodesWithGraph.end())
                m_NodesWithGraph.erase(it);
        }
        else if (node.Type == NodeType::PoseCache)
        {
            auto& poseCacheNodes = m_Editor.GetPoseCacheNodes();
            CachedNodeData nodeToFind{ this, node.ID };
            auto it = std::find_if(poseCacheNodes.begin(), poseCacheNodes.end(), [nodeToFind](const CachedNodeData& a)
            {
                return a == nodeToFind;
            });
            if (it != poseCacheNodes.end())
            {
                poseCacheNodes.erase(it);
            }
        }
    }

    void UIGraph::HandleBPNode(util::BlueprintNodeBuilder& builder, Node& node, Pin* newLinkPin)
    {
        const auto isSimple = node.Type == NodeType::Simple || node.Type == NodeType::Variable ||
            node.Type == NodeType::PoseCache || node.Type == NodeType::PoseCacheGetter || node.Type == NodeType::StateMachine;

        bool hasOutputDelegates = false;
        for (auto& output : node.OutputPins)
            if (output.Type == PinType::Delegate)
                hasOutputDelegates = true;

        builder.Begin(node.ID);
        if (!isSimple)
        {
            builder.Header(node.Color);
            ImGui::Spring(0);
            ImGui::TextUnformatted(node.GetName().c_str());
            ImGui::Spring(1);
            ImGui::Dummy(ImVec2(0, 28));
            if (hasOutputDelegates)
            {
                ImGui::BeginVertical("delegates", ImVec2(0, 28));
                ImGui::Spring(1, 0);
                for (auto& output : node.OutputPins)
                {
                    if (output.Type != PinType::Delegate)
                        continue;

                    auto alpha = ImGui::GetStyle().Alpha;
                    if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
                        alpha = alpha * (48.0f / 255.0f);

                    ed::BeginPin(output.ID, ed::PinKind::Output);
                    ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                    ed::PinPivotSize(ImVec2(0, 0));
                    ImGui::BeginHorizontal(output.ID.AsPointer());
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                    if (!output.Name.empty())
                    {
                        ImGui::TextUnformatted(output.Name.c_str());
                        ImGui::Spring(0);
                    }
                    DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
                    ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
                    ImGui::EndHorizontal();
                    ImGui::PopStyleVar();
                    ed::EndPin();

                    //DrawItemRect(ImColor(255, 0, 0));
                }
                ImGui::Spring(1, 0);
                ImGui::EndVertical();
                ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
            }
            else
                ImGui::Spring(0);
            builder.EndHeader();
        }

        // Saving some data to draw additional widgets at the bottom
        float nodeStartWidth = ImGui::GetCursorScreenPos().x; // These are needed to limit the width of additional widgets
        float nodeEndWidth = ImGui::GetCursorScreenPos().x + float(m_GraphData.PinIconSize);
        ImVec2 cursorPosUnderInputs{}; // This is needed to draw at the correct pos

        if (node.Type == NodeType::StateMachine)
        {
            if (ImGui::Button("Edit..."))
            {
                m_Editor.AddGraph(node.Graph);
            }
        }

        for (auto& input : node.InputPins)
        {
            auto alpha = ImGui::GetStyle().Alpha;
            if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
                alpha = alpha * (48.0f / 255.0f);

            builder.Input(input.ID);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            DrawPinIcon(input, IsPinLinked(input.ID), (int)(alpha * 255));
            nodeStartWidth = ImGui::GetCursorScreenPos().x;
            ImGui::Spring(0);
            if (!input.Name.empty())
            {
                ImGui::TextUnformatted(input.Name.c_str());
                ImGui::Spring(0);
            }

            bool bHasConnection = false;
            {
                Node* pinNode = FindNode(input.NodeID);
                EG_CORE_ASSERT(pinNode);
                for (const auto& inputs : pinNode->InputsPerPin[input.Index])
                {
                    if (inputs.NodeID)
                    {
                        bHasConnection = true;
                        break;
                    }
                }
            }
            if (!bHasConnection && input.DefaultValue)
            {
                if (input.Type == PinType::Bool)
                {
                    Ref<GraphVariableBool> value = Cast<GraphVariableBool>(input.DefaultValue);
                    if (ImGui::Checkbox("##cond", &value->Value))
                        m_Editor.OnGraphChanged();
                    ImGui::Spring(0);
                }
                else if (input.Type == PinType::Float)
                {
                    Ref<GraphVariableFloat> value = Cast<GraphVariableFloat>(input.DefaultValue);
                    ImGui::PushItemWidth(50.f);
                    if (ImGui::DragFloat("##v", &value->Value, 0.01f))
                        m_Editor.OnGraphChanged();
                    ImGui::PopItemWidth();
                    ImGui::Spring(0);
                }
                else if (input.Type == PinType::Vec4)
                {
                    Ref<GraphVariableVec4> value = Cast<GraphVariableVec4>(input.DefaultValue);
                    if (DragVec4(value->Value))
                        m_Editor.OnGraphChanged();
                    ImGui::Spring(0);
                }
                else if (input.Type == PinType::String)
                {
                    Ref<GraphVariableString> value = Cast<GraphVariableString>(input.DefaultValue);
                    
                    const float maxWidth = glm::max(50.f, ImGui::CalcTextSize(value->Value.c_str(), NULL, true).x + 7.5f);
                    ImGui::PushItemWidth(maxWidth);
                    if (UI::InputText("##Anim_StringVar", value->Value))
                        m_Editor.OnGraphChanged();
                    ImGui::PopItemWidth();

                    ImGui::Spring(0);
                }
                else if (input.Type == PinType::Object)
                {
                    // TODO: Fix drop-menu
                    Ref<GraphVariableAnimation> value = Cast<GraphVariableAnimation>(input.DefaultValue);
                    float maxWidth = 75.f;
                    if (value->Value)
                        maxWidth = glm::max(150.f, ImGui::CalcTextSize(value->Value->GetPath().stem().u8string().c_str(), NULL, true).x);
                    if (UI::DrawAssetSelection("", value->Value, "", maxWidth))
                        m_Editor.OnGraphChanged();
                    ImGui::Spring(0);
                }
            }

            if (input.HelpMessage.empty() == false)
            {
                UI::HelpMarker(input.HelpMessage);
            }

            ImGui::PopStyleVar();
            builder.EndInput();
            cursorPosUnderInputs = ImGui::GetCursorPos();
        }

        if (isSimple)
        {
            const std::string& name = node.GetName();

            builder.Middle();

            ImGui::Spring(1, 0);

            ImGui::PushItemWidth(node.Size.x - 25.f);
            if (node.bEditing)
            {
                constexpr ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
                bool bStoppedEditing = false;
                auto& name = m_RenamingNodeTemp;

                ImGui::SetKeyboardFocusHere(0);
                std::string inputTextFieldID = "##" + node.GetName();
                if (UI::InputText(inputTextFieldID.c_str(), name, inputFlags))
                    bStoppedEditing = true;

                // Lost focus, stop editing
                if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    bStoppedEditing = true;

                if (bStoppedEditing)
                {
                    node.bEditing = false;
                    if (m_RenamingNodeTemp != node.GetName())
                    {
                        if (node.Type == NodeType::Variable)
                        {
                            if (RenameVariable(node.GetName(), m_RenamingNodeTemp) == false)
                                Application::Get().GetImGuiLayer()->AddMessage("Failed to rename the variable! This name already exists!");
                        }
                        else if (node.Type == NodeType::PoseCache)
                        {
                            node.SetName(m_RenamingNodeTemp);
                        }
                        else if (node.Graph)
                        {
                            if (RenameGraph(node.GetName(), m_RenamingNodeTemp) == false)
                                Application::Get().GetImGuiLayer()->AddMessage("Failed to rename the graph! This name already exists!");
                        }
                    }
                }
            }
            else
            {
                if (node.Type == NodeType::PoseCacheGetter)
                {
                    bool bFound = false;
                    if (node.CachedNode.Owner)
                    {
                        if (const Node* cached = node.CachedNode.Owner->FindNode(node.CachedNode.NodeID))
                        {
                            ImGui::TextUnformatted(cached->GetName().c_str());
                            bFound = true;
                        }
                    }

                    if (!bFound)
                        ImGui::TextUnformatted("Unknown cache");
                }
                else
                {
                    ImGui::TextUnformatted(name.c_str());
                }
            }

            ImGui::Spring(1, 0);
        }

        for (auto& output : node.OutputPins)
        {
            if (!isSimple && output.Type == PinType::Delegate)
                continue;

            auto alpha = ImGui::GetStyle().Alpha;
            if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
                alpha = alpha * (48.0f / 255.0f);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            builder.Output(output.ID);
            if (!output.Name.empty())
            {
                ImGui::Spring(0);
                ImGui::TextUnformatted(output.Name.c_str());
            }
            ImGui::Spring(0);
            DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
            nodeEndWidth = ImGui::GetCursorScreenPos().x + float(m_GraphData.PinIconSize);
            ImGui::PopStyleVar();
            builder.EndOutput();
        }

        if (auto clipNode = Cast<AnimationGraphNodeClip>(node.GraphNode))
        {
            float currentTime = 0.f;
            if (const auto& inputVar = clipNode->GetInputVariables()[0])
            {
                Ref<GraphVariableAnimation> inputAnim = Cast<GraphVariableAnimation>(inputVar);
                if (!inputAnim)
                    inputAnim = Cast<GraphVariableAnimation>(node.InputPins[0].DefaultValue);

                if (inputAnim && inputAnim->Value)
                {
                    const float duration = inputAnim->Value->GetAnimation()->Duration;
                    currentTime = clipNode->CurrentTime / duration;
                }
            }

            ImGui::SetCursorPos(cursorPosUnderInputs);
            ImGui::PushItemWidth(nodeEndWidth - nodeStartWidth);
            UI::PushItemDisabled();
            ImGui::SliderFloat("##", &currentTime, 0.f, 1.f);
            UI::PopItemDisabled();
            ImGui::PopItemWidth();
        }

        builder.End();
    }

    void UIGraph::HandleStateNode(Node& node, Pin* newLinkPin)
    {
        const bool bDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        const float rounding = 5.0f;
        const float padding = 12.0f;

        const auto pinBackground = ed::GetStyle().Colors[ed::StyleColor_NodeBg];

        ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(128, 128, 128, 200));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(32, 32, 32, 200));
        ed::PushStyleColor(ed::StyleColor_PinRect, ImColor(60, 180, 255, 150));
        ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(60, 180, 255, 150));

        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
        ed::PushStyleVar(ed::StyleVar_NodeRounding, rounding);
        ed::PushStyleVar(ed::StyleVar_SourceDirection, ImVec2(0.0f, 1.0f));
        ed::PushStyleVar(ed::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
        ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
        ed::PushStyleVar(ed::StyleVar_PinBorderWidth, 1.0f);
        ed::PushStyleVar(ed::StyleVar_PinRadius, 5.0f);
        ed::BeginNode(node.ID);

        ImGui::BeginVertical(node.ID.AsPointer());
        ImGui::BeginHorizontal("inputs");
        ImGui::Spring(0, padding * 2);

        ImRect inputsRect;
        int inputAlpha = 200;
        if (!node.InputPins.empty())
        {
            auto& pin = node.InputPins[0];
            ImGui::Dummy(ImVec2(0, padding));
            ImGui::Spring(1, 0);
            inputsRect = ImGui_GetItemRect();

            ed::PushStyleVar(ed::StyleVar_PinArrowSize, 10.0f);
            ed::PushStyleVar(ed::StyleVar_PinArrowWidth, 10.0f);
#if IMGUI_VERSION_NUM > 18101
            ed::PushStyleVar(ed::StyleVar_PinCorners, ImDrawFlags_RoundCornersBottom);
#else
            ed::PushStyleVar(ed::StyleVar_PinCorners, 12);
#endif
            ed::BeginPin(pin.ID, ed::PinKind::Input);
            ed::PinPivotRect(inputsRect.GetTL(), inputsRect.GetBR());
            ed::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
            ed::EndPin();
            ed::PopStyleVar(3);

            if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
                inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        }
        else
            ImGui::Dummy(ImVec2(0, padding));

        ImGui::Spring(0, padding * 2);
        ImGui::EndHorizontal();

        ImGui::BeginHorizontal("content_frame");
        ImGui::Spring(1, padding);

        ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
        ImGui::Dummy(ImVec2(160, 0));
        ImGui::Spring(1);

        ImGui::PushItemWidth(node.Size.x - 25.f);
        if (node.bEditing)
        {
            constexpr ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
            bool bStoppedEditing = false;
            auto& name = m_RenamingNodeTemp;

            ImGui::SetKeyboardFocusHere(0);
            std::string inputTextFieldID = "##" + node.GetName();
            if (UI::InputText(inputTextFieldID.c_str(), name, inputFlags))
                bStoppedEditing = true;

            // Lost focus, stop editing
            if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                bStoppedEditing = true;

            if (bStoppedEditing)
            {
                node.bEditing = false;
                if (m_RenamingNodeTemp != node.GetName())
                {
                    if (node.Graph)
                    {
                        if (RenameGraph(node.GetName(), m_RenamingNodeTemp) == false)
                            Application::Get().GetImGuiLayer()->AddMessage("Failed to rename the graph! This name already exists!");
                    }
                }
            }
        }
        else
            ImGui::TextUnformatted(node.GetName().c_str());

        ImGui::Spring(1);
        ImGui::EndVertical();
        auto contentRect = ImGui_GetItemRect();

        ImGui::Spring(1, padding);
        ImGui::EndHorizontal();

        ImGui::BeginHorizontal("outputs");
        ImGui::Spring(0, padding * 2);

        ImRect outputsRect;
        int outputAlpha = 200;
        if (!node.OutputPins.empty())
        {
            auto& pin = node.OutputPins[0];
            ImGui::Dummy(ImVec2(0, padding));
            ImGui::Spring(1, 0);
            outputsRect = ImGui_GetItemRect();

            ed::PushStyleVar(ed::StyleVar_PinArrowSize, 10.0f);
            ed::PushStyleVar(ed::StyleVar_PinArrowWidth, 10.0f);
#if IMGUI_VERSION_NUM > 18101
            ed::PushStyleVar(ed::StyleVar_PinCorners, ImDrawFlags_RoundCornersTop);
#else
            ed::PushStyleVar(ed::StyleVar_PinCorners, 3);
#endif
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ed::PinPivotRect(outputsRect.GetTL(), outputsRect.GetBR());
            ed::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
            ed::EndPin();
            ed::PopStyleVar(3);

            if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
                outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        }
        else
            ImGui::Dummy(ImVec2(0, padding));

        ImGui::Spring(0, padding * 2);
        ImGui::EndHorizontal();

        ImGui::EndVertical();

        if (bDoubleClicked && ImGui::IsItemClicked())
        {
            m_Editor.AddGraph(node.Graph);
        }

        ed::EndNode();
        ed::PopStyleVar(7);
        ed::PopStyleColor(4);

        auto drawList = ed::GetNodeBackgroundDrawList(node.ID);

        const auto    topRoundCornersFlags = ImDrawFlags_RoundCornersTop;
        const auto bottomRoundCornersFlags = ImDrawFlags_RoundCornersBottom;

        drawList->AddRectFilled(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
            IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, bottomRoundCornersFlags);
        //ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
        drawList->AddRect(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
            IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, bottomRoundCornersFlags);
        //ImGui::PopStyleVar();
        drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
            IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, topRoundCornersFlags);
        //ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
        drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
            IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, topRoundCornersFlags);
        //ImGui::PopStyleVar();
        drawList->AddRectFilled(contentRect.GetTL(), contentRect.GetBR(), IM_COL32(24, 64, 128, 200), 0.0f);
        //ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
        drawList->AddRect(
            contentRect.GetTL(),
            contentRect.GetBR(),
            IM_COL32(48, 128, 255, 100), 0.0f);
        //ImGui::PopStyleVar();
    }
    
    void UIGraph::HandleCommentNode(Node& node, Pin* newLinkPin)
    {
        const float commentAlpha = 0.75f;
        const bool bDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool bInitialState = node.bEditing;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha);
        ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(255, 255, 255, 64));
        ed::BeginNode(node.ID);
        ImGui::PushID(node.ID.AsPointer());
        ImGui::BeginVertical("content");
        ImGui::BeginHorizontal("horizontal");

        ImGui::Spring(1);
        if (bDoubleClicked && ImGui::IsItemClicked())
        {
            node.bEditing = true;
            if (!bInitialState)
                UpdateNodeSize(m_GraphData.Editor->GetSettings(), node);
        }

        ImGui::PushItemWidth(node.Size.x - 25.f);
        if (node.bEditing)
        {
            constexpr ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;

            ImGui::SetKeyboardFocusHere(0);
            if (UI::InputText("##graph_comment", node.UserData, inputFlags))
                node.bEditing = false;

            // Lost focus, stop editing
            if (bInitialState && !ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                node.bEditing = false;
        }
        else
        {
            ImGui::TextUnformatted(node.UserData.c_str());
            if (bDoubleClicked && ImGui::IsItemClicked())
            {
                node.bEditing = true;
                if (!bInitialState)
                    UpdateNodeSize(m_GraphData.Editor->GetSettings(), node);
            }
        }
        ImGui::PopItemWidth();

        ImGui::Spring(1);
        if (bDoubleClicked && ImGui::IsItemClicked())
        {
            node.bEditing = true;
            if (!bInitialState)
                UpdateNodeSize(m_GraphData.Editor->GetSettings(), node);
        }

        ImGui::EndHorizontal();
        ed::Group(node.Size);
        ImGui::EndVertical();
        ImGui::PopID();
        ed::EndNode();
        ed::PopStyleColor(2);
        ImGui::PopStyleVar();

        if (ed::BeginGroupHint(node.ID))
        {
            //auto alpha   = static_cast<int>(commentAlpha * ImGui::GetStyle().Alpha * 255);
            auto bgAlpha = static_cast<int>(ImGui::GetStyle().Alpha * 255);

            //ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha * ImGui::GetStyle().Alpha);

            auto min = ed::GetGroupMin();
            //auto max = ed::GetGroupMax();

            ImGui::SetCursorScreenPos(min - ImVec2(-8, ImGui::GetTextLineHeightWithSpacing() + 4));
            ImGui::BeginGroup();
            ImGui::TextUnformatted(node.UserData.c_str());
            ImGui::EndGroup();

            auto drawList = ed::GetHintBackgroundDrawList();

            auto hintBounds = ImGui_GetItemRect();
            auto hintFrameBounds = ImRect_Expanded(hintBounds, 8, 4);

            drawList->AddRectFilled(
                hintFrameBounds.GetTL(),
                hintFrameBounds.GetBR(),
                IM_COL32(255, 255, 255, 64 * bgAlpha / 255), 4.0f);

            drawList->AddRect(
                hintFrameBounds.GetTL(),
                hintFrameBounds.GetBR(),
                IM_COL32(255, 255, 255, 128 * bgAlpha / 255), 4.0f);

            //ImGui::PopStyleVar();
        }
        ed::EndGroupHint();
    }

    void UIGraph::HandleNodeCreation(Node& node, ImVec2 pos, Pin* newNodeLinkPin)
    {
        BuildNodes();

        ed::SetNodePosition(node.ID, pos);

        if (auto startPin = newNodeLinkPin)
        {
            auto& pins = startPin->Kind == PinKind::Input ? node.OutputPins : node.InputPins;

            for (auto& pin : pins)
            {
                if (CanCreateLink(startPin, &pin))
                {
                    auto endPin = &pin;
                    if (startPin->Kind == PinKind::Input)
                        std::swap(startPin, endPin);

                    // Disconnect existing link
                    auto it = std::find_if(m_GraphData.Links.begin(), m_GraphData.Links.end(), [endPinId = endPin->ID](auto& link) { return link.second.EndPinID == endPinId; });
                    if (it != m_GraphData.Links.end())
                    {
                        OnLinkDeleted(it->second);
                        m_GraphData.Links.erase(it);
                    }

                    AddLink(startPin, endPin);

                    break;
                }
            }
        }
    }

    void UIGraph::DeleteNode(const Node* node)
    {
        if (node)
        {
            m_GraphData.NodesPendingDeletion.push_back(node->ID);
            OnNodeDeleted(*node);
        }
    }

    bool UIGraph::ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin)
    {
        bool bReject = true;
        if (endPin.ID == startPin.ID)
        {
            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
        }
        else if (endPin.Kind == startPin.Kind)
        {
            ShowLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
        }
        else if (endPin.NodeID == startPin.NodeID)
        {
            ShowLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
            ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
        }
        else if (endPin.Type != startPin.Type && (AreFlowAndStateFlowPinTypes(startPin.Type, endPin.Type) == false)) // Flow pins are compatible
        {
            ShowLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
            ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
        }
        else
            bReject = false;

        return bReject;
    }

    void UIGraph::ChangeVariableType(const std::string& varName, GraphVariableType newType)
    {
        if (!m_Editor.ChangeVariableType(varName, newType))
            return; // Failed

        PinType pinType = GetPinType(newType);
        const auto& varNodes = m_VarToNodesMapping[varName];
        for (auto& id : varNodes)
        {
            Node* node = FindNode(id);
            if (node)
            {
                node->GraphNode.reset();
                node->OutputPins[0].Type = pinType;
            }
        }
    }

    bool UIGraph::RenameVariable(const std::string& varName, const std::string& newName)
    {
        return m_Editor.RenameVariable(varName, newName);
    }

    void UIGraph::DeleteVariable(const std::string& var)
    {
        if (m_Editor.RemoveVariable(var))
            OnVariableDeleted(var);
    }

    bool UIGraph::RenameGraph(std::string graphName, const std::string& newName)
    {
        for (const auto& graphNodeID : m_NodesWithGraph)
        {
            Node* node = FindNode(graphNodeID);
            if (node)
            {
                if (node->GetName() == newName)
                    return false; // Name is taken
            }
        }
        
        // Find our graph
        for (auto& graphNodeID : m_NodesWithGraph)
        {
            Node* node = FindNode(graphNodeID);
            if (node && node->GetName() == graphName)
            {
                node->SetName(newName);
                node->Graph->SetName(newName);
                break;
            }
        }

        return true;
    }

    void UIGraph::OnLinkCreated(const Link& link)
    {
        Pin* startPin = FindPin(link.StartPinID);
        Pin* endPin = FindPin(link.EndPinID);

        if (startPin && endPin)
        {
            PinConnectionData inputData;
            inputData.NodeID = startPin->NodeID;
            inputData.PinIndex = startPin->Index;

            PinConnectionData outputData;
            outputData.NodeID = endPin->NodeID;
            outputData.PinIndex = endPin->Index;

            FindNode(startPin->NodeID)->OutputsPerPin[startPin->Index].push_back(outputData);
            FindNode(endPin->NodeID)->InputsPerPin[endPin->Index].push_back(inputData);

            m_Editor.OnGraphChanged();
        }
    }

    void UIGraph::OnLinkDeleted(const Link& link)
    {
        Pin* startPin = FindPin(link.StartPinID);
        Pin* endPin = FindPin(link.EndPinID);

        if (startPin && endPin)
        {
            auto& inputs = FindNode(endPin->NodeID)->InputsPerPin[endPin->Index];
            auto& outputs = FindNode(startPin->NodeID)->OutputsPerPin[startPin->Index];

            // Erase input
            {
                auto it = std::find_if(inputs.begin(), inputs.end(), [id = startPin->NodeID](const PinConnectionData& data) { return data.NodeID == id; });
                if (it != inputs.end())
                    inputs.erase(it);
            }

            // Erase output
            {
                auto it = std::find_if(outputs.begin(), outputs.end(), [id = endPin->NodeID](const PinConnectionData& data) { return data.NodeID == id; });
                if (it != outputs.end())
                    outputs.erase(it);
            }

            m_Editor.OnGraphChanged();
        }
    }
    
    void UIGraph::ShowLabel(const char* label, ImColor color)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
        auto size = ImGui::CalcTextSize(label);

        auto padding = ImGui::GetStyle().FramePadding;
        auto spacing = ImGui::GetStyle().ItemSpacing;

        ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

        auto rectMin = ImGui::GetCursorScreenPos() - padding;
        auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

        auto drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
        ImGui::TextUnformatted(label);
    }
}
