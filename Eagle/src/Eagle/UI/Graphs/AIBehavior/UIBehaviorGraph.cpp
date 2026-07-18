#include "egpch.h"
#include "UIBehaviorGraph.h"

#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Input/Input.h"

namespace Eagle
{
    static const char* s_MoveUpText = "Move Up";
    static const char* s_MoveDownText = "Move Down";

    template<typename Func>
    static void DrawClassNodeItems(UIGraph& graph, std::string_view label, const std::vector<AIBehaviorClassData>& classes, Func&& func, Node** node)
    {
        constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (ImGui::TreeNodeEx(label.data(), treeFlags))
        {
            for (const auto& data : classes)
            {
                ImGui::PushID(data.ClassData.FullName.c_str());

                if (ImGui::MenuItem(data.ClassData.UIName.c_str()))
                {
                    AIBehaviorNode nodeData;
                    nodeData.Data = data;
                    *node = &func(graph, nodeData);
                }
                if (!data.ClassData.Tooltip.empty())
                {
                    ImGui::SameLine();
                    UI::HelpMarker(data.ClassData.Tooltip);
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    static bool DrawDecoratorItems(const std::vector<AIBehaviorClassData>& decorators, std::vector<AIBehaviorClassData>& attachedDecorators)
    {
        bool bChanged = false;

        for (const auto& decorator : decorators)
        {
            const bool bAlreadyAdded = std::find(attachedDecorators.begin(), attachedDecorators.end(), decorator) != attachedDecorators.end();

            if (bAlreadyAdded)
                UI::PushItemDisabled();

            ImGui::PushID(decorator.ClassData.FullName.c_str());

            if (ImGui::MenuItem(decorator.ClassData.UIName.c_str()))
            {
                attachedDecorators.push_back(decorator);
                bChanged = true;
            }
            if (!decorator.ClassData.Tooltip.empty())
            {
                ImGui::SameLine();
                UI::HelpMarker(decorator.ClassData.Tooltip);
            }

            ImGui::PopID();

            if (bAlreadyAdded)
                UI::PopItemDisabled();
        }

        return bChanged;
    }

    // Returns true if valid
    static bool IsNodeValid(const AIBehaviorClasses& coreClasses, const AIBehaviorClasses& userClasses, const Eagle::Node& node, bool bShowMessage)
    {
        const auto& classes = node.BehaviorNodeData.Data.ClassData.bUserClass ? userClasses : coreClasses;
        const std::vector<AIBehaviorClassData>& classesData = node.Type == NodeType::BehaviorTask ? classes.Tasks : classes.Composites;

        bool bValid = true;
        if (std::find(classesData.begin(), classesData.end(), node.BehaviorNodeData.Data) == classesData.end())
        {
            bValid = false;
        }
        else
        {
            for (const auto& decorator : node.BehaviorNodeData.AttachedDecorators)
            {
                const auto& decoratorClasses = decorator.ClassData.bUserClass ? userClasses : coreClasses;
                const auto& decorators = decoratorClasses.Decorators;

                if (std::find(decorators.begin(), decorators.end(), decorator) == decorators.end())
                {
                    bValid = false;
                    break;
                }
            }
        }

        if (!bValid && bShowMessage)
            Application::Get().GetImGuiLayer()->AddMessage("At least one of the behavior nodes or decorators is invalid. Please, verify correct classes are set");

        return bValid;
    }

    static void UpdateNodeData(Eagle::Node& node)
    {
        ScriptEngine::UpdateAIClassPublicFields(node.BehaviorNodeData.Data);
        for (auto& decorator : node.BehaviorNodeData.AttachedDecorators)
        {
            ScriptEngine::UpdateAIClassPublicFields(decorator);
        }
    }

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

    UIBehaviorGraph::UIBehaviorGraph(BehaviorGraphEditor& editor, const std::string_view name)
        : UIGraph(editor, name), m_Asset(editor.GetBehaviorGraphAsset())
    {
        SetupInitialNodes();
        SetupNodeFactory();

        m_MoveUpTextSize = ImGui::CalcTextSize(s_MoveUpText);
        m_MoveDownTextSize = ImGui::CalcTextSize(s_MoveDownText);

        m_AppAssemblyReloadedCallback = ScriptEngine::AddOnAppAssemblyReloadedCallback([this]()
        {
            OnAppAssemblyReloaded();
        });
    }

    UIBehaviorGraph::~UIBehaviorGraph()
    {
        ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_AppAssemblyReloadedCallback);
    }

    void UIBehaviorGraph::DrawCreateNewNodePopup()
    {
        if (ImGui::BeginPopup("Create New Node"))
        {
            auto newNodePostion = m_CreateNodeOpenPopupPos;
            Node* node = nullptr;

            if (ImGui::MenuItem("Comment"))
            {
                node = &GraphNodeFactory::SpawnComment(*this, "Comment");
            }

            // Core classes
            {
                const auto& classes = ScriptEngine::GetCoreAIClasses();
                DrawClassNodeItems(*this, "Core Tasks", classes.Tasks, GraphNodeFactory::SpawnBehaviorTaskNode, &node);
                DrawClassNodeItems(*this, "Core Composite Nodes", classes.Composites, GraphNodeFactory::SpawnBehaviorCompositeNode, &node);
            }

            // User classes
            {
                const auto& classes = ScriptEngine::GetUserAIClasses();
                if (!classes.Tasks.empty())
                {
                    DrawClassNodeItems(*this, "User Tasks", classes.Tasks, GraphNodeFactory::SpawnBehaviorTaskNode, &node);
                }

                if (!classes.Composites.empty())
                {
                    DrawClassNodeItems(*this, "User Composite Nodes", classes.Composites, GraphNodeFactory::SpawnBehaviorCompositeNode, &node);
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

    void UIBehaviorGraph::SetupInitialNodes()
    {
        m_EntryNodeId = GraphNodeFactory::SpawnBehaviorRootNode(*this).ID;
    }

    bool UIBehaviorGraph::AreConnected_DeepSearch(const Node* startNode, const Node* endNode) const
    {
        if (!startNode || !endNode)
            return false;

        if (startNode == endNode)
            return false;

        // Check if a user tries to connect nodes again but using other input-output pins
        if (!endNode->OutputsPerPin.empty())
        {
            const auto& outputs = endNode->OutputsPerPin[0];
            for (const auto& output : outputs)
            {
                if (output.NodeID == startNode->ID)
                {
                    return true;
                }
            }
        }

        // Check if a user tries to connect nodes again using same input-output pins
        if (!startNode->OutputsPerPin.empty())
        {
            const auto& outputs = startNode->OutputsPerPin[0];
            for (const auto& output : outputs)
            {
                if (output.NodeID == endNode->ID)
                {
                    return true;
                }
            }
        }

        // Avoid graph cycles
        if (!endNode->OutputsPerPin.empty())
        {
            const auto& outputs = endNode->OutputsPerPin[0];
            for (const auto& output : outputs)
            {
                if (AreConnected_DeepSearch(startNode, FindNode(output.NodeID)))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool UIBehaviorGraph::ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin)
    {
        // Default reject behavior
        if (UIGraph::ProcessNewLinkRejection(startPin, endPin))
            return true;

        // Check if nodes are already connected. If so, reject the link
        // Since it doesn't make sense because the existing link already has a back transition that can be used
        Node* startNode = FindNode(startPin.NodeID);
        Node* endNode = FindNode(endPin.NodeID);

        if (AreConnected_DeepSearch(startNode, endNode))
        {
            ShowLabel("x Connection between the states already exist!", ImColor(45, 32, 32, 180));
            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
            return true;
        }

        return false;
    }

    void UIBehaviorGraph::DrawNodeContextPopup()
    {
        if (ImGui::BeginPopup("Node Context Menu"))
        {
            auto node = FindNode(m_ContextNodeId);
            if (node)
            {
                ImGui::TextUnformatted(node->GetName().c_str());
                if (m_ContextNodeId != m_EntryNodeId)
                {
                    ImGui::Separator();

                    const auto& coreDecorators = ScriptEngine::GetCoreAIClasses().Decorators;
                    const auto& userDecorators = ScriptEngine::GetUserAIClasses().Decorators;
                    auto& attachedDecorators = node->BehaviorNodeData.AttachedDecorators;

                    if (ImGui::BeginMenu("Add decorator"))
                    {
                        UI::TextWithSeparator("Core decorators");
                        m_bRebuild |= DrawDecoratorItems(coreDecorators, attachedDecorators);
                        if (!userDecorators.empty())
                        {
                            UI::TextWithSeparator("User decorators");
                            m_bRebuild |= DrawDecoratorItems(userDecorators, attachedDecorators);
                        }
                        ImGui::EndMenu();
                    }

                    const bool bDisableRemove = attachedDecorators.empty();
                    if (bDisableRemove)
                    {
                        UI::PushItemDisabled();
                        ImGui::MenuItem("Remove decorator");
                        UI::PopItemDisabled();
                    }
                    else
                    {
                        if (ImGui::BeginMenu("Remove decorator"))
                        {
                            for (auto it = attachedDecorators.begin(); it != attachedDecorators.end();)
                            {
                                const char* label = it->ClassData.UIName.empty() ? "Invalid" : it->ClassData.UIName.c_str();
                                ImGui::PushID(&(*it));

                                if (ImGui::MenuItem(label))
                                {
                                    it = attachedDecorators.erase(it);
                                    m_bRebuild = true;
                                }
                                else
                                {
                                    ++it;
                                }

                                ImGui::PopID();
                            }
                            ImGui::EndMenu();
                        }
                    }
                }
            }
            else
            {
                ImGui::Text("Unknown node: %p", m_ContextNodeId.AsPointer());
            }

            ImGui::EndPopup();
        }
    }

    void UIBehaviorGraph::OnAppAssemblyReloaded()
    {
        ValidateNodes();
    }

    void UIBehaviorGraph::DrawLinks()
    {
        for (auto& [_, link] : m_GraphData.Links)
        {
            ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
            
            // One directional arrow
            if (auto internalLink = m_GraphData.Editor->FindLink(link.ID))
            {
                if (internalLink->m_StartPin)
                {
                    internalLink->m_StartPin->m_ArrowSize = 0.f;
                    internalLink->m_StartPin->m_ArrowWidth = 0.f;
                }
            }
        }
    }

    void UIBehaviorGraph::OnImGuiRender(bool* pOpen)
    {
        ed::SetCurrentEditor(m_GraphData.Editor);

        RenderLeftPanel();
        UIGraph::OnImGuiRender(pOpen);

        for (auto& [id, node] : m_GraphData.Nodes)
        {
            ImVec2 currentPos = ed::GetNodePosition(id);
            if (currentPos.x != node.PrevPosition.x || currentPos.y != node.PrevPosition.y)
            {
                m_bRebuild = true;
                break;
            }
        }

        if (m_bRebuild)
        {
            RebuildBehaviorTree();
            m_bRebuild = false;

            // Check if invalid nodes became valid
            if (!m_bNodesValid)
            {
                ValidateNodes();
            }
        }
    }

    void UIBehaviorGraph::OnEvent(Event& e)
    {
        UIGraph::OnEvent(e);
        Event::Dispatch<KeyPressedEvent>(e, EG_BIND_FN(UIBehaviorGraph::OnKeyPressedEvent));
    }

    bool UIBehaviorGraph::OnKeyPressedEvent(KeyPressedEvent& e)
    {
        if (e.GetKey() == Key::S && Input::IsKeyPressed(Key::LeftControl))
        {
            m_Editor.Save();
            return true;
        }
        return false;
    }

    void UIBehaviorGraph::RenderLeftPanel()
    {
        Splitter(true, 4.0f, &m_LeftPanelWidth, &m_RightPanelWidth, 50.0f, 50.0f);
        float panelWidth = m_LeftPanelWidth - 4.0f;

        auto& io = ImGui::GetIO();

        ImGui::BeginChild("Selection", ImVec2(panelWidth, 0));

        panelWidth = ImGui::GetContentRegionAvail().x;

        ImGui::BeginHorizontal("Style Editor", ImVec2(panelWidth, 0));
        ImGui::Spring(0.0f, 0.0f);
        if (ImGui::Button("Zoom to Content"))
            ed::NavigateToContent();
        if (ImGui::Button("Save"))
        {
            m_Editor.Save();
        }
        ImGui::Spring();
        ImGui::EndHorizontal();

        ImGui::Separator();
        HandleSelectedNode();

        ImGui::EndChild();

        ImGui::SameLine(0.0f, 12.0f);
    }

    void UIBehaviorGraph::HandleSelectedNode()
    {
        // Select variable in the list
        if (ed::HasSelectionChanged())
        {
            std::vector<ed::NodeId> selectedNodes;
            selectedNodes.resize(ed::GetSelectedObjectCount());
            if (selectedNodes.empty())
            {
                m_Selected = nullptr;
            }
            else
            {
                ed::GetSelectedNodes(selectedNodes.data(), int(selectedNodes.size()));
                m_Selected = FindNode(selectedNodes[0]);
            }
        }

        if (!m_Selected)
            return;

        if (m_Selected->Type != NodeType::BehaviorTask && m_Selected->Type != NodeType::BehaviorComposite)
            return;

        auto& nodeData = m_Selected->BehaviorNodeData;
        {
            UI::TextWithSeparator("Node Properties");
            UI::BeginPropertyGrid("BehaviorGraphNodes");

            UI::Text("Full name", nodeData.Data.ClassData.FullName, nodeData.Data.ClassData.Tooltip);
            if (UI::PropertyText("UI Name", m_Selected->GetName()))
                m_Asset->SetDirty(true);
            if (!nodeData.Data.ClassData.Fields.empty())
            {
                ImGui::Separator();
                for (auto& field : nodeData.Data.ClassData.Fields)
                {
                    m_bRebuild |= UI::Property(field);

                    // TODO:
                    //int index = 0;
                    //UI::ComboWithNone("Blackboard key", -1, {}, index, {}, "You can select a key from a blackboard instead.");
                    //ImGui::Separator();
                }
            }
            UI::EndPropertyGrid();
            ImGui::Separator();
        }

        UI::TextWithSeparator("Attached Decorators");
        if (nodeData.AttachedDecorators.empty())
            ImGui::TextDisabled("No decorators");

        const float paddingX = ImGui::GetStyle().FramePadding.x;
        constexpr size_t s_InvalidIdx = size_t(-1);
        size_t indexToMoveUp = s_InvalidIdx;
        size_t indexToMoveDown = s_InvalidIdx;

        const size_t decoratorsCount = nodeData.AttachedDecorators.size();
        for (size_t i = 0; i < decoratorsCount; ++i)
        {
            auto& decorator = nodeData.AttachedDecorators[i];
            if (decorator.ClassData.UIName.empty())
                continue;

            ImGui::PushID(decorator.ClassData.FullName.c_str());

            const bool bTreeOpened = UI::PushTreeNode(decorator.ClassData.UIName, true);

            const float offset = ImGui::GetContentRegionAvail().x - m_MoveUpTextSize.x - m_MoveDownTextSize.x - paddingX;

            ImGui::SameLine(offset);
            {
                const bool bCanMoveUp = i > 0;
                const bool bCanMoveDown = i < (decoratorsCount - 1);

                if (!bCanMoveUp)
                    UI::PushItemDisabled();
                if (ImGui::Button(s_MoveUpText))
                    indexToMoveUp = i;
                if (!bCanMoveUp)
                    UI::PopItemDisabled();

                ImGui::SameLine();

                if (!bCanMoveDown)
                    UI::PushItemDisabled();
                if (ImGui::Button(s_MoveDownText))
                    indexToMoveDown = i;
                if (!bCanMoveDown)
                    UI::PopItemDisabled();
            }

            if (bTreeOpened)
            {
                UI::BeginPropertyGrid("DecoratorFields");
                for (auto& field : decorator.ClassData.Fields)
                {
                    m_bRebuild |= UI::Property(field);
                }
                UI::EndPropertyGrid();
                UI::PopTreeNode();
            }

            ImGui::PopID();
        }

        if (indexToMoveUp != s_InvalidIdx)
        {
            std::swap(nodeData.AttachedDecorators[indexToMoveUp], nodeData.AttachedDecorators[indexToMoveUp - 1]);
            m_bRebuild = true;
        }
        else if (indexToMoveDown != s_InvalidIdx)
        {
            std::swap(nodeData.AttachedDecorators[indexToMoveDown], nodeData.AttachedDecorators[indexToMoveDown + 1]);
            m_bRebuild = true;
        }
    }

    void UIBehaviorGraph::OnNodeRenamingFinished(Node& node, const std::string& newName)
    {
        node.SetName(newName);
        m_Asset->SetDirty(true);
    }

    void UIBehaviorGraph::ValidateNodes()
    {
        constexpr ImColor defaultColor = ImColor(128, 128, 128, 200);
        constexpr ImColor invalidColor = ImColor(128, 20, 20);

        const auto& coreClasses = ScriptEngine::GetCoreAIClasses();
        const auto& userClasses = ScriptEngine::GetUserAIClasses();
        // Don't show the message if they weren't valid before (avoid spamming)
        bool bShowMessage = m_bNodesValid;
        m_bNodesValid = true;

        for (auto& [id, node] : m_GraphData.Nodes)
        {
            if (node.Type != NodeType::BehaviorTask && node.Type != NodeType::BehaviorComposite)
                continue;

            if (!IsNodeValid(coreClasses, userClasses, node, bShowMessage))
            {
                // Don't spam with messages for each invalid node
                bShowMessage = false;
                m_bNodesValid = false;
                node.Color = invalidColor;
                continue;
            }

            UpdateNodeData(node);

            node.Color = defaultColor;
        }
    }

    void UIBehaviorGraph::OnLinkCreated(const Link& link)
    {
        UIGraph::OnLinkCreated(link);
        RebuildBehaviorTree();
    }

    void UIBehaviorGraph::OnLinkDeleted(const Link& link)
    {
        UIGraph::OnLinkDeleted(link);
        RebuildBehaviorTree();
    }

    void UIBehaviorGraph::RebuildBehaviorTree_Internal(Node* root, AIBehaviorNode& nodeData, size_t& index)
    {
        if (!root)
            return;

        struct NodeState_Internal
        {
            ed::NodeId ID = {};
            float PositionX = {}; // We're sorting along X
        };

        nodeData.Data = root->BehaviorNodeData.Data;
        nodeData.AttachedDecorators = root->BehaviorNodeData.AttachedDecorators;
        nodeData.OrderIndex = index++;
        nodeData.Children.clear();
        root->BehaviorNodeIndex = " (Num: " + std::to_string(nodeData.OrderIndex) + ')';

        if (root->OutputsPerPin.empty())
            return;

        const size_t outputsCount = root->OutputsPerPin[0].size(); // We only have a single pin
        std::vector<NodeState_Internal> internalData;
        internalData.reserve(outputsCount);
        nodeData.Children.reserve(outputsCount);

        for (const auto& outputData : root->OutputsPerPin[0])
        {
            auto& data = internalData.emplace_back();
            data.ID = outputData.NodeID;
            data.PositionX = ed::GetNodePosition(outputData.NodeID).x;
        }

        std::sort(internalData.begin(), internalData.end(), [](const NodeState_Internal& a, const NodeState_Internal& b)
        {
            return a.PositionX < b.PositionX;
        });

        for (const auto& data : internalData)
        {
            Node* childNode = FindNode(data.ID);
            if (!childNode)
                continue;

            auto& child = nodeData.Children.emplace_back();
            RebuildBehaviorTree_Internal(childNode, child, index);
        }
    }

    static void Print(const AIBehaviorNode& node)
    {
        EG_CORE_INFO("{} : {}", node.Data.ClassData.FullName, node.OrderIndex);
        for (const auto& child : node.Children)
            Print(child);
    }

    void UIBehaviorGraph::RebuildBehaviorTree()
    {
        if (m_bDisableBuilds)
            return;

        for (auto& [id, node] : m_GraphData.Nodes)
        {
            node.BehaviorNodeIndex.clear();
        }

        Node* root = FindNode(m_EntryNodeId);
        if (!root)
        {
            // Should never happen
            EG_CORE_ASSERT(false);
            EG_CORE_ERROR("Failed to build behavior root. There's no root!");
            return;
        }

        if (root->OutputsPerPin.empty() || root->OutputsPerPin[0].empty())
        {
            m_Asset->SetRoot({});
            return;
        }

        auto oldEditor = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        // It's pointless to parse an actual root from UI since it doesn't represent an AI node
        Node* startNode = FindNode(root->OutputsPerPin[0][0].NodeID);
        if (!startNode)
        {
            // Should never happen
            EG_CORE_ASSERT(false);
            EG_CORE_ERROR("Failed to build behavior root. There's no root!");
            return;
        }

        size_t startIndex = 1;
        AIBehaviorNode nodeData;
        RebuildBehaviorTree_Internal(startNode, nodeData, startIndex);
        m_Asset->SetRoot(std::move(nodeData));

        if (oldEditor != m_GraphData.Editor)
            ed::SetCurrentEditor(oldEditor);

        //Print(nodeData);
    }
}
