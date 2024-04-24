#include "egpch.h"
#include "GraphEditor.h"

#include "Eagle/Core/Serializer.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
    static Ref<GraphVariable> CreateAnimGraphVariableFromType(GraphVariableType type)
    {
        switch (type)
        {
        case Eagle::GraphVariableType::Bool: return MakeRef<GraphVariableBool>();
        case Eagle::GraphVariableType::Float: return MakeRef<GraphVariableFloat>();
        case Eagle::GraphVariableType::Animation: return MakeRef<GraphVariableAnimation>();
        }
        EG_CORE_ASSERT(false);
        return nullptr;
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

    GraphEditor::GraphEditor(const std::string_view name)
        : m_Name(name)
    {
        // Init textures
        m_HeaderTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/BlueprintBackground.png");
        m_SaveTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/ic_save_white_24dp.png");
        m_RestoreTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/ic_restore_white_24dp.png");

        // Init editor
        m_Config.UserPointer = this;
        m_Config.SaveNodeSettings = [](ed::NodeId nodeId, const char* data, size_t size, ed::SaveReasonFlags reason, void* userPointer) -> bool
        {
            auto self = static_cast<GraphEditor*>(userPointer);
            self->OnGraphChanged();

            return true;
        };
        // Required to prevent "node editor" from creating a json file
        m_Config.SaveSettings = [](const char* data, size_t size, ed::SaveReasonFlags reason, void* userPointer) -> bool
        {
            return true;
        };
    }

    GraphEditor::~GraphEditor()
    {
        m_Graphs.clear();
    }

    void GraphEditor::OnImGuiRender(bool* pOpen)
    {
        m_HeaderBackground = UI::GetTextureID(m_HeaderTexture);
        m_SaveIcon = UI::GetTextureID(m_SaveTexture);
        m_RestoreIcon = UI::GetTextureID(m_RestoreTexture);

        // Editor
        if (ImGui::Begin(m_Name.c_str(), pOpen))
        {
            static float leftPaneWidth = 400.0f;
            static float rightPaneWidth = 800.0f;
            Splitter(true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f);

            ShowLeftPane(leftPaneWidth - 4.0f);

            ImGui::SameLine(0.0f, 12.0f);

            auto& currentGraph = m_Graphs.back();
            currentGraph->OnImGuiRender();
        }
        ImGui::End();

        m_bIgnoreChangedEvent = false;
    }
	
	void GraphEditor::ShowLeftPane(float paneWidth)
	{
        auto& io = ImGui::GetIO();

        ImGui::BeginChild("Selection", ImVec2(paneWidth, 0));

        paneWidth = ImGui::GetContentRegionAvail().x;

        ImGui::BeginHorizontal("Style Editor", ImVec2(paneWidth, 0));
        ImGui::Spring(0.0f, 0.0f);
        if (ImGui::Button("Zoom to Content"))
            ed::NavigateToContent();
        ImGui::Spring();
        ImGui::EndHorizontal();

        int saveIconWidth = (int)m_SaveTexture->GetWidth();
        int saveIconHeight = (int)m_SaveTexture->GetHeight();
        int restoreIconWidth = (int)m_RestoreTexture->GetWidth();
        int restoreIconHeight = (int)m_RestoreTexture->GetHeight();

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight() * 1.35f),
            ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
        ImGui::Spacing(); ImGui::SameLine();

        const size_t graphsCount = m_Graphs.size();
        for (size_t i = 0; i < graphsCount; ++i)
        {
            const auto& graph = m_Graphs[i];
            if (ImGui::Button(graph->GetName().c_str()))
            {
                // Remove other graphs
                m_Graphs.resize(i + 1);
                break;
            }

            // Don't display " / " if it's last one
            if (i != (graphsCount - 1))
            {
                ImGui::SameLine();
                ImGui::Text(" / ");
                ImGui::SameLine();
            }
        }

        if (ImGui::Button("Compile"))
            Compile();
        ImGui::SameLine();
        if (ImGui::Button("Save"))
            Save();

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
                    CreateNewVar<GraphVariableBool>(nullptr);
                    OnGraphChanged();
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
                            {
                                if (RemoveVariable(name))
                                {
                                    for (auto& graph : m_Graphs)
                                        graph->OnVariableDeleted(name);
                                }
                            }

                            ImGui::EndPopup();
                        }

                        //Handling Drag Event.
                        {
                            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                            {
                                ImGui::SetDragDropPayload(GetVarDragDropTag(), name.c_str(), (name.size() + 1) * sizeof(char));
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
                        OnGraphChanged();
                    }

                    // Value
                    {
                        switch (type)
                        {
                        case GraphVariableType::Bool:
                        {
                            auto valueVar = Cast<GraphVariableBool>(var);
                            if (UI::Property("Value", valueVar->Value))
                                OnGraphChanged();
                            break;
                        }
                        case GraphVariableType::Float:
                        {
                            auto valueVar = Cast<GraphVariableFloat>(var);
                            if (UI::PropertyDrag("Value", valueVar->Value))
                                OnGraphChanged();
                            break;
                        }
                        case GraphVariableType::Animation:
                        {
                            auto valueVar = Cast<GraphVariableAnimation>(var);
                            if (UI::DrawAssetSelection("Value", valueVar->Value))
                                OnGraphChanged();
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
                    OnGraphChanged();
                }
                else
                {
                    Application::Get().GetImGuiLayer()->AddMessage("Failed to rename a variable. A variable with that name already exist!");
                    m_RenamingVarTemp = m_SelectedVar;
                }
            }
        }

        ImGui::EndChild();
	}

    bool GraphEditor::ChangeVariableType(const std::string& varName, GraphVariableType newType)
    {
        if (RemoveVariable(varName))
        {
            bool bSuccess = CreateVariable(CreateAnimGraphVariableFromType(newType), varName);
            return bSuccess;
        }
        return false;
    }

    bool GraphEditor::RenameVariable(const std::string& varName, const std::string& newName)
    {
        auto it = m_Variables.find(varName);
        if (it == m_Variables.end() || GetVariable(newName))
            return false;

        auto var = it->second;
        RemoveVariable(varName);
        CreateVariable(var, newName);

        for (auto& graph : m_Graphs)
            graph->OnVariableRenamed(varName, newName);

        return true;
    }

    void GraphEditor::SelectVariable(const std::string& var)
    {
        m_SelectedVar = var;
        m_RenamingVarTemp = var;
    }

    GraphSerializationData GraphEditor::Save()
    {
        // TODO: Make sure every graph is saved
        const auto& graphToSave = m_Graphs[0];
        const auto& graphData = graphToSave->GetGraphData();
        const auto& settings = graphData.Editor->GetSettings();

        GraphSerializationData result;
        result.ScrollOffset = glm::vec2(settings.m_ViewScroll.x, settings.m_ViewScroll.y);
        result.Zoom = settings.m_ViewZoom;

        for (const auto& [name, value] : GetVariables())
        {
            auto& var = result.Variables.emplace_back();
            var.Name = name;
            var.Value = value;
        }

        for (const auto& node : graphData.Nodes)
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

                        const Node* connectedTo = graphToSave->FindNode(outputData.NodeID);
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

        return result;
    }
}
