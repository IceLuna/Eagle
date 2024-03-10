#include "egpch.h"
#include "AnimationGraphEditor.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Animation/AnimationGraphNodes.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
    static const char* s_VarDragDropTag = "ANIMATION_GRAPH_VAR";
    static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
    {
        using namespace ImGui;
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        ImGuiID id = window->GetID("##Splitter");
        ImRect bb;
        bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
        bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
        return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
    }

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

    static Ref<AnimationGraphVariable> CreateAnimGraphVariableFromType(GraphVariableType type)
    {
        switch (type)
        {
        case Eagle::GraphVariableType::Bool: return MakeRef<AnimationGraphVariableBool>();
        case Eagle::GraphVariableType::Float: return MakeRef<AnimationGraphVariableFloat>();
        case Eagle::GraphVariableType::Animation: return MakeRef<AnimationGraphVariableAnimation>();
        }
        EG_CORE_ASSERT(false);
        return nullptr;
    }

    static PinType GetPinType(GraphVariableType type)
    {
        switch (type)
        {
        case GraphVariableType::Bool: return PinType::Bool;
        case GraphVariableType::Float: return PinType::Float;
        case GraphVariableType::Animation: return PinType::Object;
        }
        EG_CORE_ASSERT(false);
        return PinType::Object;
    }

    static GraphVariableType GetVariableType(PinType type)
    {
        switch (type)
        {
        case PinType::Bool : return GraphVariableType::Bool;
        case PinType::Float: return GraphVariableType::Float;
        case PinType::Object: return GraphVariableType::Animation;
        }
        EG_CORE_ASSERT(false);
        return GraphVariableType::Bool;
    }

    static bool IsVariableType(PinType type)
    {
        switch (type)
        {
        case PinType::Flow:
        case PinType::Pose:
        case PinType::Function:
        case PinType::Delegate: return false;
        }

        return true;
    }

    static void UpdateNodeSize(const ed::Detail::Settings& settings, Node& node)
    {
        const ed::Detail::NodeSettings* nodeSettings = settings.FindNode(node.ID);
        if (nodeSettings)
            node.Size = nodeSettings->m_Size;
    }

	AnimationGraphEditor::AnimationGraphEditor(const Ref<AssetAnimationGraph>& graph)
        : m_Graph(graph)
	{
        m_HeaderTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/BlueprintBackground.png");
        m_SaveTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/ic_save_white_24dp.png");
        m_RestoreTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/ic_restore_white_24dp.png");
        OnStart();
	}

	AnimationGraphEditor::~AnimationGraphEditor()
	{
        OnStop();
	}

    void AnimationGraphEditor::OnStart()
    {
        const bool bDirty = m_Graph->IsDirty();

        ed::Config config;
        config.UserPointer = this;

        config.SaveNodeSettings = [](ed::NodeId nodeId, const char* data, size_t size, ed::SaveReasonFlags reason, void* userPointer) -> bool
        {
            auto self = static_cast<AnimationGraphEditor*>(userPointer);
            if (self->IgnoreChangedEvent() == false)
                self->GetGraphAsset()->SetDirty(true);

            return true;
        };
        // Required to prevent "node editor" from creating a json file
        config.SaveSettings = [](const char* data, size_t size, ed::SaveReasonFlags reason, void* userPointer) -> bool
        {
            return true;
        };

        m_Editor = ed::CreateEditor(&config);
        ed::SetCurrentEditor(m_Editor);

        SetupNodeFactory();
        m_OutputNodeId = SpawnOutputPoseNode()->ID;

        Deserialize();

        // If deserialization has failed, focus
        if (m_Nodes.size() == 1)
            ed::NavigateToContent();

        BuildNodes();

        if (!bDirty) // Creation of nodes makes it dirty, so reset it to false if it wasn't dirty
            m_Graph->SetDirty(false);
    }

    void AnimationGraphEditor::OnStop()
    {
        if (m_Editor)
        {
            ed::DestroyEditor(m_Editor);
            m_Editor = nullptr;
        }
    }

    void AnimationGraphEditor::OnImGuiRender(bool* pOpen)
    {
        m_HeaderBackground = UI::GetTextureID(m_HeaderTexture);
        m_SaveIcon = UI::GetTextureID(m_SaveTexture);
        m_RestoreIcon = UI::GetTextureID(m_RestoreTexture);
        m_StartMousePos = ImGui::GetMousePos();

        for (auto& pendingNodeId : m_NodesPendingDeletion)
        {
            auto id = std::find_if(m_Nodes.begin(), m_Nodes.end(), [nodeId = pendingNodeId](auto& node) { return node.ID == nodeId; });
            if (id != m_Nodes.end())
            {
                auto& node = *id;
                if (node.Type == NodeType::Variable)
                {
                    auto it = m_VarToNodesMapping.find(node.Name);
                    if (it != m_VarToNodesMapping.end())
                    {
                        auto& varNodes = it->second;
                        auto nodeIt = std::find(varNodes.begin(), varNodes.end(), node.ID);
                        if (nodeIt != varNodes.end())
                            varNodes.erase(nodeIt);
                    }
                }
                
                m_Nodes.erase(id);
            }
        }
        if (!m_NodesPendingDeletion.empty())
        {
            BuildNodes();
            m_NodesPendingDeletion.clear();
        }

        if (ImGui::Begin("Animation Editor", pOpen))
        {
            UpdateTouch();

            ed::SetCurrentEditor(m_Editor);

            static ed::NodeId contextNodeId = 0;
            static ed::LinkId contextLinkId = 0;
            static ed::PinId  contextPinId = 0;
            static bool createNewNode = false;
            static Pin* newNodeLinkPin = nullptr;
            static Pin* newLinkPin = nullptr;

            static float leftPaneWidth = 400.0f;
            static float rightPaneWidth = 800.0f;
            Splitter(true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f);

            ShowLeftPane(leftPaneWidth - 4.0f);

            ImGui::SameLine(0.0f, 12.0f);

            ed::Begin("Node editor");

            //Drop event
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_VarDragDropTag))
                {
                    const std::string varName = (const char*)payload->Data;
                    auto var = GetVariable(varName);
                    if (var)
                    {
                        Node* node = SpawnVarNode(varName, GetPinType(var->GetType()));
                        createNewNode = false;
                        HandleNodeCreation(node, ImGui::GetMousePos(), nullptr);
                    }
                }

                ImGui::EndDragDropTarget();
            }

            {
                auto cursorTopLeft = ImGui::GetCursorScreenPos();

                util::BlueprintNodeBuilder builder(m_HeaderBackground, (int)m_HeaderTexture->GetWidth(), (int)m_HeaderTexture->GetHeight());

                // BP
                for (auto& node : m_Nodes)
                {
                    if (node.Type != NodeType::Blueprint && node.Type != NodeType::Simple && node.Type != NodeType::Variable)
                        continue;

                    HandleBPNode(builder, node, newLinkPin);
                }

                // Tree
                for (auto& node : m_Nodes)
                {
                    if (node.Type != NodeType::Tree)
                        continue;

                    HandleTreeNode(node, newLinkPin);
                }

                // Houdini
                for (auto& node : m_Nodes)
                {
                    if (node.Type != NodeType::Houdini)
                        continue;

                    HandleHoudiniNode(node, newLinkPin);
                }

                // Comment
                for (auto& node : m_Nodes)
                {
                    if (node.Type != NodeType::Comment)
                        continue;

                    HandleCommentNode(node, newLinkPin);
                }

                for (auto& link : m_Links)
                    ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);

                if (!createNewNode)
                {
                    if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f))
                    {
                        auto showLabel = [](const char* label, ImColor color)
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
                        };

                        ed::PinId startPinId = 0, endPinId = 0;
                        if (ed::QueryNewLink(&startPinId, &endPinId))
                        {
                            auto startPin = FindPin(startPinId);
                            auto endPin = FindPin(endPinId);

                            newLinkPin = startPin ? startPin : endPin;

                            if (startPin->Kind == PinKind::Input)
                            {
                                std::swap(startPin, endPin);
                                std::swap(startPinId, endPinId);
                            }

                            if (startPin && endPin)
                            {
                                if (endPin == startPin)
                                {
                                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                                }
                                else if (endPin->Kind == startPin->Kind)
                                {
                                    showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
                                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                                }
                                else if (endPin->NodeID == startPin->NodeID)
                                {
                                    showLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
                                    ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
                                }
                                else if (endPin->Type != startPin->Type)
                                {
                                    showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
                                    ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                                }
                                else
                                {
                                    showLabel("+ Create Link", ImColor(32, 45, 32, 180));
                                    if (ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
                                    {
                                        // Disconnect existing link
                                        auto id = std::find_if(m_Links.begin(), m_Links.end(), [endPinId](auto& link) { return link.EndPinID == endPinId; });
                                        if (id != m_Links.end())
                                        {
                                            OnLinkDeleted(*id);
                                            m_Links.erase(id);
                                        }

                                        m_Links.emplace_back(Link(GetNextId(), startPinId, endPinId));
                                        m_Links.back().Color = GetIconColor(startPin->Type);
                                        OnLinkCreated(m_Links.back());
                                    }
                                }
                            }
                        }

                        ed::PinId pinId = 0;
                        if (ed::QueryNewNode(&pinId))
                        {
                            newLinkPin = FindPin(pinId);
                            if (newLinkPin)
                                showLabel("+ Create Node", ImColor(32, 45, 32, 180));

                            if (ed::AcceptNewItem())
                            {
                                createNewNode = true;
                                newNodeLinkPin = FindPin(pinId);
                                newLinkPin = nullptr;
                                ed::Suspend();
                                ImGui::OpenPopup("Create New Node");
                                ed::Resume();
                                m_bGetPopupPos = true;
                                m_bShowCreateNewVarInPopup = true;
                            }
                        }
                    }
                    else
                        newLinkPin = nullptr;

                    ed::EndCreate();

                    if (ed::BeginDelete())
                    {
                        ed::NodeId nodeId = 0;
                        while (ed::QueryDeletedNode(&nodeId))
                        {
                            Node* node = FindNode(nodeId);
                            if (node && node->bDeletable && ed::AcceptDeletedItem())
                            {
                                auto id = std::find_if(m_Nodes.begin(), m_Nodes.end(), [nodeId](auto& node) { return node.ID == nodeId; });
                                if (id != m_Nodes.end())
                                    DeleteNode(&(*id));
                            }
                        }

                        ed::LinkId linkId = 0;
                        while (ed::QueryDeletedLink(&linkId))
                        {
                            if (ed::AcceptDeletedItem())
                            {
                                auto id = std::find_if(m_Links.begin(), m_Links.end(), [linkId](auto& link) { return link.ID == linkId; });
                                if (id != m_Links.end())
                                {
                                    OnLinkDeleted(*id);
                                    m_Links.erase(id);
                                }
                            }
                        }
                    }
                    ed::EndDelete();
                }

                ImGui::SetCursorScreenPos(cursorTopLeft);
            }

# if 1
            auto openPopupPosition = ImGui::GetMousePos();
            ed::Suspend();
            if (ed::ShowNodeContextMenu(&contextNodeId))
                ImGui::OpenPopup("Node Context Menu");
            else if (ed::ShowPinContextMenu(&contextPinId))
                ImGui::OpenPopup("Pin Context Menu");
            else if (ed::ShowLinkContextMenu(&contextLinkId))
                ImGui::OpenPopup("Link Context Menu");
            else if (ed::ShowBackgroundContextMenu())
            {
                ImGui::OpenPopup("Create New Node");
                newNodeLinkPin = nullptr;
                m_bGetPopupPos = true;
                m_bShowCreateNewVarInPopup = false;
            }
            if (m_bGetPopupPos)
            {
                m_CreateNodeOpenPopupPos = openPopupPosition;
                m_bGetPopupPos = false;
            }
            ed::Resume();

            ed::Suspend();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            if (ImGui::BeginPopup("Node Context Menu"))
            {
                auto node = FindNode(contextNodeId);

                ImGui::TextUnformatted("Node Context Menu");
                ImGui::Separator();
                if (node)
                {
                    ImGui::Text("ID: %p", node->ID.AsPointer());
                    ImGui::Text("Type: %s", node->Type == NodeType::Blueprint ? "Blueprint" : (node->Type == NodeType::Tree ? "Tree" : "Comment"));
                    ImGui::Text("Input Pins: %d", (int)node->InputPins.size());
                    ImGui::Text("Output Pins: %d", (int)node->OutputPins.size());
                }
                else
                    ImGui::Text("Unknown node: %p", contextNodeId.AsPointer());
                ImGui::Separator();

                // Delete node
                {
                    Node* node = FindNode(contextNodeId);
                    if (node && node->bDeletable && ImGui::MenuItem("Delete"))
                        DeleteNode(node);
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Pin Context Menu"))
            {
                auto pin = FindPin(contextPinId);

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
                    ImGui::Text("Unknown pin: %p", contextPinId.AsPointer());

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Link Context Menu"))
            {
                auto link = FindLink(contextLinkId);

                ImGui::TextUnformatted("Link Context Menu");
                ImGui::Separator();
                if (link)
                {
                    ImGui::Text("ID: %p", link->ID.AsPointer());
                    ImGui::Text("From: %p", link->StartPinID.AsPointer());
                    ImGui::Text("To: %p", link->EndPinID.AsPointer());
                }
                else
                    ImGui::Text("Unknown link: %p", contextLinkId.AsPointer());
                ImGui::Separator();
                if (ImGui::MenuItem("Delete"))
                    ed::DeleteLink(contextLinkId);
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Create New Node"))
            {
                auto newNodePostion = m_CreateNodeOpenPopupPos;

                Node* node = nullptr;
#if 0
                {
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
                    if (ImGui::MenuItem("Less"))
                        node = SpawnLessNode();
                    if (ImGui::MenuItem("Weird"))
                        node = SpawnWeirdNode();
                    if (ImGui::MenuItem("Trace by Channel"))
                        node = SpawnTraceByChannelNode();
                    if (ImGui::MenuItem("Print String"))
                        node = SpawnPrintStringNode();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Comment"))
                        node = SpawnComment();
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

                    ImGui::Separator();
                    if (ImGui::MenuItem("Entry"))
                        node = SpawnEntryNode();
                    if (ImGui::MenuItem("Int to String"))
                        node = SpawnIntToStringNode();
                    ImGui::Separator();
                }
#endif
                if (m_bShowCreateNewVarInPopup)
                {
                    if (newNodeLinkPin && IsVariableType(newNodeLinkPin->Type) && ImGui::MenuItem("Create new variable"))
                    {
                        std::string varName;
                        switch (newNodeLinkPin->Type)
                        {
                        case PinType::Bool:
                            varName = CreateNewVar<AnimationGraphVariableBool>(newNodeLinkPin->DefaultValue);
                            break;
                        case PinType::Float:
                            varName = CreateNewVar<AnimationGraphVariableFloat>(newNodeLinkPin->DefaultValue);
                            break;
                        case PinType::Object:
                            varName = CreateNewVar<AnimationGraphVariableAnimation>(newNodeLinkPin->DefaultValue);
                            break;
                        }
                        if (!varName.empty())
                        {
                            node = SpawnVarNode(varName, newNodeLinkPin->Type);
                            SelectVariable(varName);
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
                            node = (this->*func)(name);
                    }
                }

                // Variables
                {
                    const auto& variables = GetVariables();
                    if (variables.size())
                        UI::TextWithSeparator("Variables");

                    for (auto& [name, var] : variables)
                    {
                        if (ImGui::MenuItem(name.c_str()))
                        {
                            node = SpawnVarNode(name, GetPinType(var->GetType()));
                        }
                    }
                }

                if (node)
                {
                    createNewNode = false;
                    HandleNodeCreation(node, newNodePostion, newNodeLinkPin);
                }

                ImGui::EndPopup();
            }
            else
                createNewNode = false;
            ImGui::PopStyleVar();
            ed::Resume();
# endif

            ed::End();
        }

        //ImGui::ShowTestWindow();
        //ImGui::ShowMetricsWindow();
        ImGui::End();

        m_bIgnoreChangedEvent = false;
    }
    
    void AnimationGraphEditor::ShowLeftPane(float paneWidth)
    {
        auto& io = ImGui::GetIO();

        ImGui::BeginChild("Selection", ImVec2(paneWidth, 0));

        paneWidth = ImGui::GetContentRegionAvail().x;

        ImGui::BeginHorizontal("Style Editor", ImVec2(paneWidth, 0));
        ImGui::Spring(0.0f, 0.0f);
        if (ImGui::Button("Zoom to Content"))
            ed::NavigateToContent();
        ImGui::Spring(0.0f);
        if (ImGui::Button("Show Flow"))
        {
            for (auto& link : m_Links)
                ed::Flow(link.ID);
        }
        ImGui::Spring();
        ImGui::EndHorizontal();

        std::vector<ed::NodeId> selectedNodes;
        std::vector<ed::LinkId> selectedLinks;
        selectedNodes.resize(ed::GetSelectedObjectCount());
        selectedLinks.resize(ed::GetSelectedObjectCount());

        int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
        int linkCount = ed::GetSelectedLinks(selectedLinks.data(), static_cast<int>(selectedLinks.size()));

        selectedNodes.resize(nodeCount);
        selectedLinks.resize(linkCount);

        int saveIconWidth = (int)m_SaveTexture->GetWidth();
        int saveIconHeight = (int)m_SaveTexture->GetHeight();
        int restoreIconWidth = (int)m_RestoreTexture->GetWidth();
        int restoreIconHeight = (int)m_RestoreTexture->GetHeight();

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
            ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
        ImGui::Spacing(); ImGui::SameLine();
        ImGui::TextUnformatted("Nodes");
        ImGui::Indent();
        for (auto& node : m_Nodes)
        {
            ImGui::PushID(node.ID.AsPointer());
            auto start = ImGui::GetCursorScreenPos();

            if (const auto progress = GetTouchProgress(node.ID))
            {
                ImGui::GetWindowDrawList()->AddLine(
                    start + ImVec2(-8, 0),
                    start + ImVec2(-8, ImGui::GetTextLineHeight()),
                    IM_COL32(255, 0, 0, 255 - (int)(255 * progress)), 4.0f);
            }

            bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), node.ID) != selectedNodes.end();
# if IMGUI_VERSION_NUM >= 18967
            ImGui::SetNextItemAllowOverlap();
# endif
            if (ImGui::Selectable((node.Name + "##" + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer()))).c_str(), &isSelected))
            {
                if (io.KeyCtrl)
                {
                    if (isSelected)
                        ed::SelectNode(node.ID, true);
                    else
                        ed::DeselectNode(node.ID);
                }
                else
                    ed::SelectNode(node.ID, false);

                ed::NavigateToSelection();
            }
            if (ImGui::IsItemHovered() && !node.State.empty())
                ImGui::SetTooltip("State: %s", node.State.c_str());

            auto id = std::string("(") + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer())) + ")";
            auto textSize = ImGui::CalcTextSize(id.c_str(), nullptr);
            auto iconPanelPos = start + ImVec2(
                paneWidth - ImGui::GetStyle().FramePadding.x - ImGui::GetStyle().IndentSpacing - saveIconWidth - restoreIconWidth - ImGui::GetStyle().ItemInnerSpacing.x * 1,
                (ImGui::GetTextLineHeight() - saveIconHeight) / 2);
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(iconPanelPos.x - textSize.x - ImGui::GetStyle().ItemInnerSpacing.x, start.y),
                IM_COL32(255, 255, 255, 255), id.c_str(), nullptr);

            auto drawList = ImGui::GetWindowDrawList();
            ImGui::SetCursorScreenPos(iconPanelPos);
# if IMGUI_VERSION_NUM < 18967
            ImGui::SetItemAllowOverlap();
# else
            ImGui::SetNextItemAllowOverlap();
# endif
            if (node.SavedState.empty())
            {
                if (ImGui::InvisibleButton("save", ImVec2((float)saveIconWidth, (float)saveIconHeight)))
                    node.SavedState = node.State;

                if (ImGui::IsItemActive())
                    drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 96));
                else if (ImGui::IsItemHovered())
                    drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
                else
                    drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 160));
            }
            else
            {
                ImGui::Dummy(ImVec2((float)saveIconWidth, (float)saveIconHeight));
                drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 32));
            }

            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
# if IMGUI_VERSION_NUM < 18967
            ImGui::SetItemAllowOverlap();
# else
            ImGui::SetNextItemAllowOverlap();
# endif
            if (!node.SavedState.empty())
            {
                if (ImGui::InvisibleButton("restore", ImVec2((float)restoreIconWidth, (float)restoreIconHeight)))
                {
                    node.State = node.SavedState;
                    ed::RestoreNodeState(node.ID);
                    node.SavedState.clear();
                }

                if (ImGui::IsItemActive())
                    drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 96));
                else if (ImGui::IsItemHovered())
                    drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
                else
                    drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 160));
            }
            else
            {
                ImGui::Dummy(ImVec2((float)restoreIconWidth, (float)restoreIconHeight));
                drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 32));
            }

            ImGui::SameLine(0, 0);
# if IMGUI_VERSION_NUM < 18967
            ImGui::SetItemAllowOverlap();
# endif
            ImGui::Dummy(ImVec2(0, (float)restoreIconHeight));

            ImGui::PopID();
        }
        ImGui::Unindent();

        if (ImGui::Button("Compile"))
            Compile();
        ImGui::SameLine();
        if (ImGui::Button("Save"))
            Save();

        if (ed::HasSelectionChanged())
        {
            std::vector<ed::NodeId> selectedNodes;
            selectedNodes.resize(ed::GetSelectedObjectCount());
            int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
            if (selectedNodes.size() > 0)
            {
                Node* selected = FindNode(selectedNodes[0]);
                if (selected && selected->Type == NodeType::Variable)
                    SelectVariable(selected->Name);
            }
        }

        // Variables
        {
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
                | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

            constexpr uint64_t treeID1 = 95392191ull;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
            ImGui::Separator();
            bool treeOpened = ImGui::TreeNodeEx((void*)treeID1, flags, "Variables");
            ImGui::PopStyleVar();
            if (treeOpened)
            {
                if (ImGui::Button("Create variable"))
                {
                    CreateNewVar<AnimationGraphVariableBool>(nullptr);
                    m_Graph->SetDirty(true);
                }
                ImGui::Separator();
                ImGui::Separator();

                const auto variables = GetVariables();
                if (!variables.empty())
                {
                    UI::BeginPropertyGrid("AnimationGraphVars");

                    UI::Text("Name", "Type");
                    ImGui::Separator();

                    for (const auto& [name, var] : variables)
                    {
                        ImGui::PushID(name.c_str());
                        GraphVariableType type = var->GetType();

                        ImGui::SetNextItemAllowOverlap();
                        ImVec2 curPos = ImGui::GetCursorPos();
                        if (ImGui::Selectable("##var_select", m_SelectedVar == name, 0, ImVec2(ImGui::GetColumnWidth(), 25)))
                            SelectVariable(name);

                        if (ImGui::BeginPopupContextItem(name.c_str(), ImGuiPopupFlags_MouseButtonRight))
                        {
                            SelectVariable(name);

                            if (ImGui::MenuItem("Delete"))
                                DeleteVariable(name);

                            ImGui::EndPopup();
                        }

                        //Handling Drag Event.
                        {
                            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                            {
                                ImGui::SetDragDropPayload(s_VarDragDropTag, name.c_str(), (name.size() + 1) * sizeof(char));
                                ImGui::Text(name.c_str());

                                ImGui::EndDragDropSource();
                            }
                        }

                        ImGui::SetCursorPos(curPos);

                        if (UI::ComboEnum(name, type))
                            ChangeVariableType(name, type);

                        ImGui::PopID();
                    }

                    UI::EndPropertyGrid();
                }
                ImGui::TreePop();
            }
        }

        // Selected var
        {
            auto var = GetVariable(m_SelectedVar);
            bool bRename = false;
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
                | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

            constexpr uint64_t treeID1 = 95392151ull;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
            ImGui::Separator();
            bool treeOpened = ImGui::TreeNodeEx((void*)treeID1, flags, "Details");
            ImGui::PopStyleVar();
            if (treeOpened)
            {
                if (var)
                {
                    GraphVariableType type = var->GetType();
                    UI::BeginPropertyGrid("AnimationGraphVars");

                    if (UI::PropertyText("Name", m_RenamingVarTemp, "", ImGuiInputTextFlags_EnterReturnsTrue))
                        bRename = true;

                    // Lost focus, accept input
                    if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (m_SelectedVar != m_RenamingVarTemp))
                        bRename = true;

                    if (UI::ComboEnum("Type", type))
                    {
                        ChangeVariableType(m_SelectedVar, type);
                        var = GetVariable(m_SelectedVar);
                        m_Graph->SetDirty(true);
                    }

                    // Value
                    {
                        switch (type)
                        {
                            case GraphVariableType::Bool:
                            {
                                auto valueVar = Cast<AnimationGraphVariableBool>(var);
                                if (UI::Property("Value", valueVar->Value))
                                    m_Graph->SetDirty(true);
                                break;
                            }
                            case GraphVariableType::Float:
                            {
                                auto valueVar = Cast<AnimationGraphVariableFloat>(var);
                                if (UI::PropertyDrag("Value", valueVar->Value))
                                    m_Graph->SetDirty(true);
                                break;
                            }
                            case GraphVariableType::Animation:
                            {
                                auto valueVar = Cast<AnimationGraphVariableAnimation>(var);
                                if (UI::DrawAssetSelection("Value", valueVar->Value))
                                    m_Graph->SetDirty(true);
                                break;
                            }
                        }
                    }

                    UI::EndPropertyGrid();
                }
                ImGui::TreePop();
            }

            if (bRename)
            {
                if (RenameVariable(m_SelectedVar, m_RenamingVarTemp))
                {
                    m_SelectedVar = m_RenamingVarTemp;
                    m_Graph->SetDirty(true);
                }
                else
                {
                    Application::Get().GetImGuiLayer()->AddMessage("Failed to rename a variable. A variable with that name already exist!");
                    m_RenamingVarTemp = m_SelectedVar;
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
            for (auto& link : m_Links)
                ed::Flow(link.ID);

        ImGui::EndChild();
    }

    void AnimationGraphEditor::HandleBPNode(util::BlueprintNodeBuilder& builder, Node& node, Pin* newLinkPin)
    {
        const auto isSimple = node.Type == NodeType::Simple || node.Type == NodeType::Variable;

        bool hasOutputDelegates = false;
        for (auto& output : node.OutputPins)
            if (output.Type == PinType::Delegate)
                hasOutputDelegates = true;

        builder.Begin(node.ID);
        if (!isSimple)
        {
            builder.Header(node.Color);
            ImGui::Spring(0);
            ImGui::TextUnformatted(node.Name.c_str());
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
        float nodeStartWidth = 0.f; // These are needed to limit the width of additional widgets
        float nodeEndWidth = 0.f;
        ImVec2 cursorPosUnderInputs{}; // This is needed to draw at the correct pos
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

            const bool bHasConnection = FindNode(input.NodeID)->Inputs[input.Index].operator bool();
            if (!bHasConnection && input.DefaultValue)
            {
                if (input.Type == PinType::Bool)
                {
                    Ref<AnimationGraphVariableBool> value = Cast<AnimationGraphVariableBool>(input.DefaultValue);
                    if (ImGui::Checkbox("##cond", &value->Value))
                        m_Graph->SetDirty(true);
                    ImGui::Spring(0);
                }
                else if (input.Type == PinType::Float)
                {
                    Ref<AnimationGraphVariableFloat> value = Cast<AnimationGraphVariableFloat>(input.DefaultValue);
                    ImGui::PushItemWidth(50.f);
                    if (ImGui::DragFloat("##v", &value->Value, 0.01f))
                        m_Graph->SetDirty(true);
                    ImGui::PopItemWidth();
                    ImGui::Spring(0);
                }
                else if (input.Type == PinType::Object)
                {
                    Ref<AnimationGraphVariableAnimation> value = Cast<AnimationGraphVariableAnimation>(input.DefaultValue);
                    float maxWidth = 75.f;
                    if (value->Value)
                        maxWidth = glm::max(150.f, ImGui::CalcTextSize(value->Value->GetPath().stem().u8string().c_str(), NULL, true).x);
                    if (UI::DrawAssetSelection("", value->Value, "", maxWidth))
                        m_Graph->SetDirty(true);
                    ImGui::Spring(0);
                }
            }

            ImGui::PopStyleVar();
            builder.EndInput();
            cursorPosUnderInputs = ImGui::GetCursorPos();
        }

        if (isSimple)
        {
            builder.Middle();

            ImGui::Spring(1, 0);
            ImGui::TextUnformatted(node.Name.c_str());
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
            if (output.Type == PinType::String)
            {
                static char buffer[128] = "Edit Me\nMultiline!";
                static bool wasActive = false;

                ImGui::PushItemWidth(100.0f);
                ImGui::InputText("##edit", node.UserData.data(), node.UserData.length() + 1, ImGuiInputTextFlags_CallbackResize, UI::TextResizeCallback, &node.UserData);
                ImGui::PopItemWidth();
                if (ImGui::IsItemActive() && !wasActive)
                {
                    ed::EnableShortcuts(false);
                    wasActive = true;
                }
                else if (!ImGui::IsItemActive() && wasActive)
                {
                    ed::EnableShortcuts(true);
                    wasActive = false;
                }
                ImGui::Spring(0);
            }
            if (!output.Name.empty())
            {
                ImGui::Spring(0);
                ImGui::TextUnformatted(output.Name.c_str());
            }
            ImGui::Spring(0);
            DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
            nodeEndWidth = ImGui::GetCursorScreenPos().x + float(m_PinIconSize);
            ImGui::PopStyleVar();
            builder.EndOutput();
        }

        if (auto clipNode = Cast<AnimationGraphNodeClip>(node.GraphNode))
        {
            float currentTime = 0.f;
            if (const auto& inputVar = clipNode->GetInputVariables()[0])
            {
                Ref<AnimationGraphVariableAnimation> inputAnim = Cast<AnimationGraphVariableAnimation>(inputVar);
                if (!inputAnim)
                    inputAnim = Cast<AnimationGraphVariableAnimation>(node.InputPins[0].DefaultValue);

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

    void AnimationGraphEditor::HandleTreeNode(Node& node, Pin* newLinkPin)
    {
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
        ImGui::TextUnformatted(node.Name.c_str());
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

#if IMGUI_VERSION_NUM > 18101
            ed::PushStyleVar(ed::StyleVar_PinCorners, ImDrawFlags_RoundCornersTop);
#else
            ed::PushStyleVar(ed::StyleVar_PinCorners, 3);
#endif
            ed::BeginPin(pin.ID, ed::PinKind::Output);
            ed::PinPivotRect(outputsRect.GetTL(), outputsRect.GetBR());
            ed::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
            ed::EndPin();
            ed::PopStyleVar();

            if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
                outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        }
        else
            ImGui::Dummy(ImVec2(0, padding));

        ImGui::Spring(0, padding * 2);
        ImGui::EndHorizontal();

        ImGui::EndVertical();

        ed::EndNode();
        ed::PopStyleVar(7);
        ed::PopStyleColor(4);

        auto drawList = ed::GetNodeBackgroundDrawList(node.ID);

        //const auto fringeScale = ImGui::GetStyle().AntiAliasFringeScale;
        //const auto unitSize    = 1.0f / fringeScale;

        //const auto ImDrawList_AddRect = [](ImDrawList* drawList, const ImVec2& a, const ImVec2& b, ImU32 col, float rounding, int rounding_corners, float thickness)
        //{
        //    if ((col >> 24) == 0)
        //        return;
        //    drawList->PathRect(a, b, rounding, rounding_corners);
        //    drawList->PathStroke(col, true, thickness);
        //};

#if IMGUI_VERSION_NUM > 18101
        const auto    topRoundCornersFlags = ImDrawFlags_RoundCornersTop;
        const auto bottomRoundCornersFlags = ImDrawFlags_RoundCornersBottom;
#else
        const auto    topRoundCornersFlags = 1 | 2;
        const auto bottomRoundCornersFlags = 4 | 8;
#endif

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

    void AnimationGraphEditor::HandleHoudiniNode(Node& node, Pin* newLinkPin)
    {
        const float rounding = 10.0f;
        const float padding = 12.0f;


        ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(229, 229, 229, 200));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(125, 125, 125, 200));
        ed::PushStyleColor(ed::StyleColor_PinRect, ImColor(229, 229, 229, 60));
        ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(125, 125, 125, 60));

        const auto pinBackground = ed::GetStyle().Colors[ed::StyleColor_NodeBg];

        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
        ed::PushStyleVar(ed::StyleVar_NodeRounding, rounding);
        ed::PushStyleVar(ed::StyleVar_SourceDirection, ImVec2(0.0f, 1.0f));
        ed::PushStyleVar(ed::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
        ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
        ed::PushStyleVar(ed::StyleVar_PinBorderWidth, 1.0f);
        ed::PushStyleVar(ed::StyleVar_PinRadius, 6.0f);
        ed::BeginNode(node.ID);

        ImGui::BeginVertical(node.ID.AsPointer());
        if (!node.InputPins.empty())
        {
            ImGui::BeginHorizontal("inputs");
            ImGui::Spring(1, 0);

            ImRect inputsRect;
            int inputAlpha = 200;
            for (auto& pin : node.InputPins)
            {
                ImGui::Dummy(ImVec2(padding, padding));
                inputsRect = ImGui_GetItemRect();
                ImGui::Spring(1, 0);
                inputsRect.Min.y -= padding;
                inputsRect.Max.y -= padding;

#if IMGUI_VERSION_NUM > 18101
                const auto allRoundCornersFlags = ImDrawFlags_RoundCornersAll;
#else
                const auto allRoundCornersFlags = 15;
#endif
                //ed::PushStyleVar(ed::StyleVar_PinArrowSize, 10.0f);
                //ed::PushStyleVar(ed::StyleVar_PinArrowWidth, 10.0f);
                ed::PushStyleVar(ed::StyleVar_PinCorners, allRoundCornersFlags);

                ed::BeginPin(pin.ID, ed::PinKind::Input);
                ed::PinPivotRect(inputsRect.GetCenter(), inputsRect.GetCenter());
                ed::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
                ed::EndPin();
                //ed::PopStyleVar(3);
                ed::PopStyleVar(1);

                auto drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(inputsRect.GetTL(), inputsRect.GetBR(),
                    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, allRoundCornersFlags);
                drawList->AddRect(inputsRect.GetTL(), inputsRect.GetBR(),
                    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, allRoundCornersFlags);

                if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
                    inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
            }

            //ImGui::Spring(1, 0);
            ImGui::EndHorizontal();
        }

        ImGui::BeginHorizontal("content_frame");
        ImGui::Spring(1, padding);

        ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
        ImGui::Dummy(ImVec2(160, 0));
        ImGui::Spring(1);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted(node.Name.c_str());
        ImGui::PopStyleColor();
        ImGui::Spring(1);
        ImGui::EndVertical();
        auto contentRect = ImGui_GetItemRect();

        ImGui::Spring(1, padding);
        ImGui::EndHorizontal();

        if (!node.OutputPins.empty())
        {
            ImGui::BeginHorizontal("outputs");
            ImGui::Spring(1, 0);

            ImRect outputsRect;
            int outputAlpha = 200;
            for (auto& pin : node.OutputPins)
            {
                ImGui::Dummy(ImVec2(padding, padding));
                outputsRect = ImGui_GetItemRect();
                ImGui::Spring(1, 0);
                outputsRect.Min.y += padding;
                outputsRect.Max.y += padding;

#if IMGUI_VERSION_NUM > 18101
                const auto allRoundCornersFlags = ImDrawFlags_RoundCornersAll;
                const auto topRoundCornersFlags = ImDrawFlags_RoundCornersTop;
#else
                const auto allRoundCornersFlags = 15;
                const auto topRoundCornersFlags = 3;
#endif

                ed::PushStyleVar(ed::StyleVar_PinCorners, topRoundCornersFlags);
                ed::BeginPin(pin.ID, ed::PinKind::Output);
                ed::PinPivotRect(outputsRect.GetCenter(), outputsRect.GetCenter());
                ed::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
                ed::EndPin();
                ed::PopStyleVar();


                auto drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR(),
                    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, allRoundCornersFlags);
                drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR(),
                    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, allRoundCornersFlags);


                if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
                    outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
            }

            ImGui::EndHorizontal();
        }

        ImGui::EndVertical();

        ed::EndNode();
        ed::PopStyleVar(7);
        ed::PopStyleColor(4);
    }

    void AnimationGraphEditor::HandleCommentNode(Node& node, Pin* newLinkPin)
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
                UpdateNodeSize(m_Editor->GetSettings(), node);
        }

        ImGui::PushItemWidth(node.Size.x - 25.f);
        if (node.bEditing)
        {
            constexpr ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;

            ImGui::SetKeyboardFocusHere(0);
            if (ImGui::InputText("##graph_comment", node.UserData.data(), node.UserData.length() + 1, inputFlags, UI::TextResizeCallback, &node.UserData))
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
                    UpdateNodeSize(m_Editor->GetSettings(), node);
            }
        }
        ImGui::PopItemWidth();

        ImGui::Spring(1);
        if (bDoubleClicked && ImGui::IsItemClicked())
        {
            node.bEditing = true;
            if (!bInitialState)
                UpdateNodeSize(m_Editor->GetSettings(), node);
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

    void AnimationGraphEditor::DeleteNode(const Node* node)
    {
        if (node)
            m_NodesPendingDeletion.push_back(node->ID);
    }

    void AnimationGraphEditor::ChangeVariableType(const std::string& varName, GraphVariableType newType)
    {
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
        auto& graph = m_Graph->GetGraph();
        RemoveVariable(varName);
        CreateVariable(CreateAnimGraphVariableFromType(newType), varName);
    }

    bool AnimationGraphEditor::RenameVariable(const std::string& varName, const std::string& newName)
    {
        auto it = m_Variables.find(varName);
        if (it == m_Variables.end() || GetVariable(newName))
            return false;

        auto var = it->second;
        RemoveVariable(varName);
        CreateVariable(var, newName);

        auto varNodes = std::move(m_VarToNodesMapping[varName]);
        for (auto& id : varNodes)
        {
            Node* node = FindNode(id);
            if (node)
                node->Name = newName;
        }
        m_VarToNodesMapping.erase(varName);
        m_VarToNodesMapping[newName] = std::move(varNodes);

        return true;
    }

    void AnimationGraphEditor::OnLinkCreated(const Link& link)
    {
        Pin* startPin = FindPin(link.StartPinID);
        Pin* endPin = FindPin(link.EndPinID);

        if (startPin && endPin)
        {
            OutputConnectionData outputData;
            outputData.NodeID = endPin->NodeID;
            outputData.PinIndex = endPin->Index;

            FindNode(startPin->NodeID)->OutputsPerPin[startPin->Index].push_back(outputData);
            FindNode(endPin->NodeID)->Inputs[endPin->Index] = startPin->NodeID;
        }
    }

    void AnimationGraphEditor::OnLinkDeleted(const Link& link)
    {
        Pin* startPin = FindPin(link.StartPinID);
        Pin* endPin = FindPin(link.EndPinID);

        if (startPin && endPin)
        {
            auto& outputs = FindNode(startPin->NodeID)->OutputsPerPin[startPin->Index];
            auto it = std::find_if(outputs.begin(), outputs.end(), [id = endPin->NodeID](const OutputConnectionData& data) { return data.NodeID == id; });
            if (it != outputs.end())
                outputs.erase(it);
            FindNode(endPin->NodeID)->Inputs[endPin->Index] = {};
        }
    }

    void AnimationGraphEditor::HandleNodeCreation(Node* node, ImVec2 pos, Pin* newNodeLinkPin)
    {
        BuildNodes();

        ed::SetNodePosition(node->ID, pos);

        if (auto startPin = newNodeLinkPin)
        {
            auto& pins = startPin->Kind == PinKind::Input ? node->OutputPins : node->InputPins;

            for (auto& pin : pins)
            {
                if (CanCreateLink(startPin, &pin))
                {
                    auto endPin = &pin;
                    if (startPin->Kind == PinKind::Input)
                        std::swap(startPin, endPin);

                    // Disconnect existing link
                    auto id = std::find_if(m_Links.begin(), m_Links.end(), [endPinId = endPin->ID](auto& link) { return link.EndPinID == endPinId; });
                    if (id != m_Links.end())
                    {
                        OnLinkDeleted(*id);
                        m_Links.erase(id);
                    }

                    m_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
                    m_Links.back().Color = GetIconColor(startPin->Type);
                    OnLinkCreated(m_Links.back());

                    break;
                }
            }
        }
    }

    void AnimationGraphEditor::Deserialize()
    {
        const auto& data = m_Graph->GetSerializationData();
        m_Editor->SetViewScroll(ImVec2(data.ScrollOffset.x, data.ScrollOffset.y));
        m_Editor->SetViewZoom(data.Zoom);

        // Create variables
        for (const auto& var : data.Variables)
        {
            // Create var if doesn't exist
            if (!GetVariable(var.Name))
                CreateNewVarFromType(var.Value->GetType(), var.Value, var.Name);
        }

        // Create nodes
        int maxNodeID = m_NextId;
        for (const auto& nodeData : data.Nodes)
        {
            m_NextId = int(nodeData.NodeID); // So that the node is created with the required ID

            if (nodeData.bVariable)
            {
                if (const auto& var = GetVariable(nodeData.Name))
                {
                    Node* createdNode = SpawnVarNode(nodeData.Name, GetPinType(var->GetType()));
                    ed::SetNodePosition(createdNode->ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                }
            }
            else
            {
                for (const auto& [unused, factory] : m_NodeFactory)
                {
                    auto it = factory.find(nodeData.Name);
                    if (it != factory.end())
                    {
                        auto func = it->second;
                        Node* createdNode = (this->*func)(nodeData.Name);
                        createdNode->Size = ImVec2(nodeData.Size.x, nodeData.Size.y);
                        ed::SetNodePosition(createdNode->ID, ImVec2(nodeData.Position.x, nodeData.Position.y));
                        ed::SetGroupSize(createdNode->ID, createdNode->Size);
                        createdNode->UserData = nodeData.UserData;

                        // Set default values
                        const size_t inputPinsCount = createdNode->InputPins.size();
                        if (inputPinsCount == nodeData.DefaultValues.size()) // Should always match, but this check is here just in case
                        {
                            for (size_t i = 0; i < inputPinsCount; ++i)
                                createdNode->InputPins[i].DefaultValue = nodeData.DefaultValues[i];
                        }
                        break;
                    }
                }
            }

            if (m_NextId > maxNodeID)
                maxNodeID = m_NextId; // Save the max node ID so that we can set `m_NextId` to it after all nodes are created
        }
        m_NextId = maxNodeID;

        // Link nodes
        for (const auto& nodeData : data.Nodes)
        {
            for (const auto& connection : nodeData.OutputConnections)
            {
                if (Node* connectToNode = FindNode(connection.NodeID))
                {
                    Node* currentNode = FindNode(nodeData.NodeID);
                    const Pin* startPin = &currentNode->OutputPins[0];
                    const Pin* endPin = &connectToNode->InputPins[connection.PinIndex];

                    m_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
                    m_Links.back().Color = GetIconColor(startPin->Type);
                    OnLinkCreated(m_Links.back());
                }
            }
        }
    }

    void AnimationGraphEditor::SetupNodeFactory()
    {
        // Animations category
        {
            auto& animationsCategory = m_NodeFactory["Animations"];
            animationsCategory["Animation Clip"] = &AnimationGraphEditor::SpawnAnimClipNode;
            animationsCategory["Blend Poses"] = &AnimationGraphEditor::SpawnAnimBlendNode;
            animationsCategory["Additive Blend"] = &AnimationGraphEditor::SpawnAnimAdditiveBlendNode;
            animationsCategory["Calculate Additive"] = &AnimationGraphEditor::SpawnAnimCalculateAdditiveNode;
            animationsCategory["Select Pose by Bool"] = &AnimationGraphEditor::SpawnSelectPoseByBoolNode;
        }

        // Logical catergory
        {
            auto& logicalCategory = m_NodeFactory["Logical"];
            logicalCategory["AND"] = &AnimationGraphEditor::SpawnAndNode;
            logicalCategory["OR"] = &AnimationGraphEditor::SpawnOrNode;
            logicalCategory["XOR"] = &AnimationGraphEditor::SpawnXorNode;
            logicalCategory["NOT"] = &AnimationGraphEditor::SpawnNotNode;
            logicalCategory["<"] = &AnimationGraphEditor::SpawnLessNode;
            logicalCategory["<="] = &AnimationGraphEditor::SpawnLessEqNode;
            logicalCategory[">"] = &AnimationGraphEditor::SpawnGreaterNode;
            logicalCategory[">="] = &AnimationGraphEditor::SpawnGreaterEqNode;
            logicalCategory["=="] = &AnimationGraphEditor::SpawnEqualNode;
            logicalCategory["!="] = &AnimationGraphEditor::SpawnNotEqualNode;
        }

        // Math catergory
        {
            auto& mathCategory = m_NodeFactory["Math"];
            mathCategory["Add"] = &AnimationGraphEditor::SpawnAddNode;
            mathCategory["Subtract"] = &AnimationGraphEditor::SpawnSubNode;
            mathCategory["Multiply"] = &AnimationGraphEditor::SpawnMulNode;
            mathCategory["Divide"] = &AnimationGraphEditor::SpawnDivNode;
            mathCategory["Sin (rad)"] = &AnimationGraphEditor::SpawnSinNode;
            mathCategory["Cos (rad)"] = &AnimationGraphEditor::SpawnCosNode;
            mathCategory["To Radians"] = &AnimationGraphEditor::SpawnToRadNode;
            mathCategory["To Degrees"] = &AnimationGraphEditor::SpawnToDegNode;
        }

        // Other
        {
            auto& commentCategory = m_NodeFactory["Other"];
            commentCategory["Comment"] = &AnimationGraphEditor::SpawnComment;
        }
    }

    void AnimationGraphEditor::SelectVariable(const std::string& var)
    {
        m_SelectedVar = var;
        m_RenamingVarTemp = var;
    }

    void AnimationGraphEditor::DeleteVariable(const std::string& var)
    {
        if (m_SelectedVar == var)
            SelectVariable("");

        if (RemoveVariable(var))
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
                        auto id = std::find_if(m_Links.begin(), m_Links.end(), [pinID = node->OutputPins[0].ID](auto& link) { return link.StartPinID == pinID; });
                        if (id == m_Links.end())
                            break;

                        OnLinkDeleted(*id);
                        m_Links.erase(id);
                    }

                    DeleteNode(node);
                }

                m_VarToNodesMapping.erase(it);
            }
        }
    }

    void AnimationGraphEditor::Parse(Node* node, bool bCloneVars, VariablesMap& outVariables)
    {
        for (auto& input : node->Inputs)
        {
            if (!input)
                continue;

            Node* connectedNode = FindNode(input);
            auto& graphNode = connectedNode->GraphNode;
            if (!graphNode)
                continue;

            graphNode->Reset();
            const size_t inputsCount = connectedNode->Inputs.size();
            for (size_t i = 0; i < inputsCount; ++i)
            {
                const auto& input = connectedNode->Inputs[i];
                Node* inputNode = FindNode(input);
                if (inputNode)
                {
                    if (inputNode->GraphNode)
                        graphNode->SetInput(inputNode->GraphNode, i);
                    else if (inputNode->Type == NodeType::Variable)
                    {
                        const auto& var = GetVariable(inputNode->Name);
                        Ref<AnimationGraphVariable> resultVar;
                        if (bCloneVars)
                            resultVar = CopyVarByType(var);
                        else
                            resultVar = var;

                        if (resultVar)
                            outVariables[inputNode->Name] = resultVar;
                        graphNode->SetInput(resultVar, i);
                    }
                }
                else
                {
                    const auto& inputPin = connectedNode->InputPins[i];
                    if (inputPin.DefaultValue)
                        graphNode->SetInput(inputPin.DefaultValue, i);
                }
            }
        
            Parse(connectedNode, bCloneVars, outVariables);
        }
    }

    void AnimationGraphEditor::Compile()
    {
        auto& graph = m_Graph->GetGraph();
        Node* result = FindNode(m_OutputNodeId);
        graph->Reset();

        if (result->Inputs[0])
        {
            VariablesMap usedVars;
            Parse(result, true, usedVars);
            graph->SetVariables(std::move(usedVars));

            // We clone it so that changing node in the editor doesn't affect the final component without compilation
            const auto& resultNode = FindNode(result->Inputs[0])->GraphNode;
            graph->SetOutput(resultNode ? resultNode->Clone() : nullptr);
        }
        else
        {
            graph->SetOutput(nullptr);
        }
    }

    void AnimationGraphEditor::Save()
    {
        const auto& settings = m_Editor->GetSettings();

        GraphSerializationData result;
        result.ScrollOffset = glm::vec2(settings.m_ViewScroll.x, settings.m_ViewScroll.y);
        result.Zoom = settings.m_ViewZoom;

        for (const auto& [name, value] : GetVariables())
        {
            auto& var = result.Variables.emplace_back();
            var.Name = name;
            var.Value = value;
        }

        for (const auto& node : m_Nodes)
        {
            const auto& nodeSetting = settings.FindNode(node.ID);
            if (!nodeSetting)
                continue;

            GraphNodeSerializationData nodeData;
            nodeData.Name = node.Name;
            nodeData.bVariable = node.Type == NodeType::Variable;
            nodeData.Position = glm::vec2(nodeSetting->m_Location.x, nodeSetting->m_Location.y);
            nodeData.Size = glm::vec2(nodeSetting->m_Size.x, nodeSetting->m_Size.y);
            nodeData.NodeID = (uint32_t)node.ID.Get();
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
    
        m_Graph->SetSerializationData(result);
        Asset::Save(m_Graph);
    }

    Node* AnimationGraphEditor::SpawnInputActionNode()
    {
        m_Nodes.emplace_back(GetNextId(), "InputAction Fire", ImColor(255, 128, 128));
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Delegate);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Pressed", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Released", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnBranchNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Branch");
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Condition", PinType::Bool);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "True", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "False", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnDoNNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Do N");
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Enter", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "N", PinType::Int);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Reset", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Exit", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Counter", PinType::Int);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnOutputActionNode()
    {
        m_Nodes.emplace_back(GetNextId(), "OutputAction");
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Sample", PinType::Float);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Condition", PinType::Bool);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Event", PinType::Delegate);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnPrintStringNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Print String");
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "In String", PinType::String);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnMessageNode()
    {
        m_Nodes.emplace_back(GetNextId(), "", ImColor(128, 195, 248));
        m_Nodes.back().Type = NodeType::Simple;
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Message", PinType::String);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnSetTimerNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Set Timer", ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Object", PinType::Object);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Function Name", PinType::Function);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Time", PinType::Float);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Looping", PinType::Bool);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnWeirdNode()
    {
        m_Nodes.emplace_back(GetNextId(), "o.O", ImColor(128, 195, 248));
        m_Nodes.back().Type = NodeType::Simple;
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnTraceByChannelNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Single Line Trace by Channel", ImColor(255, 128, 64));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Start", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "End", PinType::Int);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Trace Channel", PinType::Float);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Trace Complex", PinType::Bool);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Actors to Ignore", PinType::Int);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Draw Debug Type", PinType::Bool);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Ignore Self", PinType::Bool);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Out Hit", PinType::Float);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Return Value", PinType::Bool);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnTreeSequenceNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Sequence");
        m_Nodes.back().Type = NodeType::Tree;
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnTreeTaskNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Move To");
        m_Nodes.back().Type = NodeType::Tree;
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnTreeTask2Node()
    {
        m_Nodes.emplace_back(GetNextId(), "Random Wait");
        m_Nodes.back().Type = NodeType::Tree;
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnHoudiniTransformNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Transform");
        m_Nodes.back().Type = NodeType::Houdini;
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnHoudiniGroupNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Group");
        m_Nodes.back().Type = NodeType::Houdini;
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Flow);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnEntryNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Entry", ImColor(128, 195, 248));
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Flow);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnIntToStringNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Int", ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Int);
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::String);
        m_Nodes.back().Type = NodeType::Blueprint;

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnVarNode(const std::string& name, PinType type)
    {
        m_Nodes.emplace_back(GetNextId(), name.c_str(), ImColor(128, 195, 248));
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", type);
        m_Nodes.back().Type = NodeType::Variable;

        m_VarToNodesMapping[name].push_back(m_Nodes.back().ID);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnOutputPoseNode()
    {
        m_Nodes.emplace_back(GetNextId(), "Output Pose", ImColor(128, 195, 248), false);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Pose);

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnAnimBlendNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Pose 1", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Pose 2", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Blend weight", PinType::Float, MakeRef<AnimationGraphVariableFloat>(0.5f));

        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Output pose", PinType::Pose);
        m_Nodes.back().Type = NodeType::Blueprint;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeBlend>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnAnimCalculateAdditiveNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Reference pose", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Source pose", PinType::Pose);

        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Additive pose", PinType::Pose);
        m_Nodes.back().Type = NodeType::Blueprint;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeCalculateAdditive>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnAnimAdditiveBlendNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Target pose", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Additive pose", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Blend weight", PinType::Float, MakeRef<AnimationGraphVariableFloat>(1.f));

        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Output pose", PinType::Pose);
        m_Nodes.back().Type = NodeType::Blueprint;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeAdditiveBlend>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnAnimClipNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Animation", PinType::Object, MakeRef<AnimationGraphVariableAnimation>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Playback speed", PinType::Float, MakeRef<AnimationGraphVariableFloat>(1.f));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Loop", PinType::Bool, MakeRef<AnimationGraphVariableBool>(true));

        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Output pose", PinType::Pose);
        m_Nodes.back().Type = NodeType::Blueprint;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeClip>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnSelectPoseByBoolNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "False pose", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "True pose", PinType::Pose);
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "Condition", PinType::Bool, MakeRef<AnimationGraphVariableBool>(true));

        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "Output pose", PinType::Pose);
        m_Nodes.back().Type = NodeType::Blueprint;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeSelectPoseByBool>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnAndNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeAnd>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnOrNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeOr>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnXorNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeXor>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnNotNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Bool, MakeRef<AnimationGraphVariableBool>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeNot>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnLessNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeLess>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnLessEqNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeLessEqual>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnGreaterNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeGreater>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnGreaterEqNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeGreaterEqual>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnEqualNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeEqual>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnNotEqualNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Bool);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeNotEqual>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnAddNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeAdd>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnSubNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeSub>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnMulNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeMul>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnDivNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeDiv>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnSinNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeSin>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnCosNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeCos>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnToRadNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeToRad>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }
    
    Node* AnimationGraphEditor::SpawnToDegNode(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name, ImColor(128, 195, 248));
        m_Nodes.back().InputPins.emplace_back(GetNextId(), "", PinType::Float, MakeRef<AnimationGraphVariableFloat>());
        m_Nodes.back().OutputPins.emplace_back(GetNextId(), "", PinType::Float);
        m_Nodes.back().Type = NodeType::Simple;

        m_Nodes.back().GraphNode = MakeRef<AnimationGraphNodeToDeg>(m_Graph->GetGraph());

        BuildNode(&m_Nodes.back());

        return &m_Nodes.back();
    }

    Node* AnimationGraphEditor::SpawnComment(const std::string_view name)
    {
        m_Nodes.emplace_back(GetNextId(), name);
        m_Nodes.back().Type = NodeType::Comment;
        m_Nodes.back().Size = ImVec2(300, 200);

        return &m_Nodes.back();
    }
}
