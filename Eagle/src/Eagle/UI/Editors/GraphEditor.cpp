#include "egpch.h"
#include "GraphEditor.h"

#include "Eagle/Core/Serializer.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Input/Input.h"

namespace Eagle
{
    static Ref<GraphVariable> CreateAnimGraphVariableFromType(GraphVariableType type)
    {
        switch (type)
        {
        case Eagle::GraphVariableType::Bool: return MakeRef<GraphVariableBool>();
        case Eagle::GraphVariableType::Int: return MakeRef<GraphVariableInt>();
        case Eagle::GraphVariableType::Float: return MakeRef<GraphVariableFloat>();
        case Eagle::GraphVariableType::Animation: return MakeRef<GraphVariableAnimation>();
        case Eagle::GraphVariableType::String: return MakeRef<GraphVariableString>();
        case Eagle::GraphVariableType::Vec4: return MakeRef<GraphVariableVec4>();
        }
        EG_CORE_ASSERT(false);
        return nullptr;
    }

    // Copy from ImGui::SplitterBehavior, but adjusted handling of X/Y offsets to fix the bug here.
    static bool SplitterBehavior(const ImRect& bb, ImGuiID id, ImGuiAxis axis, float* size1, float* size2, float min_size1, float min_size2, float hover_extend = 0.0f, float hover_visibility_delay = 0.0f, ImU32 bg_col = 0)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;

        if (!ImGui::ItemAdd(bb, id, NULL, ImGuiItemFlags_NoNav))
            return false;

        // FIXME: AFAIK the only leftover reason for passing ImGuiButtonFlags_AllowOverlap here is
        // to allow caller of SplitterBehavior() to call SetItemAllowOverlap() after the item.
        // Nowadays we would instead want to use SetNextItemAllowOverlap() before the item.
        ImGuiButtonFlags button_flags = ImGuiButtonFlags_FlattenChildren;
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        button_flags |= ImGuiButtonFlags_AllowOverlap;
#endif

        bool hovered, held;
        ImRect bb_interact = bb;
        bb_interact.Expand(axis == ImGuiAxis_Y ? ImVec2(0.0f, hover_extend) : ImVec2(hover_extend, 0.0f));
        ImGui::ButtonBehavior(bb_interact, id, &hovered, &held, button_flags);
        if (hovered)
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredRect; // for IsItemHovered(), because bb_interact is larger than bb

        if (held || (hovered && g.HoveredIdPreviousFrame == id && g.HoveredIdTimer >= hover_visibility_delay))
            ImGui::SetMouseCursor(axis == ImGuiAxis_Y ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);

        ImRect bb_render = bb;
        if (held)
        {
            float mouse_delta = (g.IO.MousePos - g.ActiveIdClickOffset - bb_interact.Min)[axis];
            float* size = axis == ImGuiAxis_X ? size1 : size2;
            const float min_size = axis == ImGuiAxis_X ? min_size1 : min_size2;

            // Minimum pane size
            float size_maximum_delta = ImMax(0.0f, *size - min_size);
            if (mouse_delta < -size_maximum_delta)
                mouse_delta = -size_maximum_delta;

            // Apply resize
            if (mouse_delta != 0.0f)
            {
                *size = ImMax(*size + mouse_delta, min_size);
                bb_render.Translate((axis == ImGuiAxis_X) ? ImVec2(mouse_delta, 0.0f) : ImVec2(0.0f, mouse_delta));
                ImGui::MarkItemEdited(id);
            }
        }

        // Render at new position
        if (bg_col & IM_COL32_A_MASK)
            window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, bg_col, 0.0f);
        const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_SeparatorActive : (hovered && g.HoveredIdTimer >= hover_visibility_delay) ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
        window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, col, 0.0f);

        return held;
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
        m_HeaderTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/BlueprintBackground.png");

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

    bool GraphEditor::OnImGuiRender(bool* pOpen)
    {
        m_HeaderBackground = UI::GetTextureID(m_HeaderTexture);

        // Required to not mark asset as dirty.
        // Because when drawing a graph for the first time, save events will be triggered
        bool bIgnoreChanges = false;

        bool bRecompiled = false;

        for (auto& graph : m_GraphsToAdd)
        {
            m_Graphs.emplace_back(std::move(graph));
            bIgnoreChanges = true;
        }
        m_GraphsToAdd.clear();

        // Editor
        m_bGraphFocused = false;
        ImGui::SetNextWindowSize(ImVec2(920.f, 760.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(m_Name.c_str(), pOpen))
        {
            m_bGraphFocused = ImGui::IsWindowFocused();
            bRecompiled = RenderLeftPanel();

            auto& currentGraph = m_Graphs.back();
            if (bIgnoreChanges)
                OnAddGraphPre();

            currentGraph->OnImGuiRender();

            if (bIgnoreChanges)
                OnAddGraphPost();
        }
        ImGui::End();

        m_bIgnoreChangedEvent = false;

        return bRecompiled;
    }
	
	bool GraphEditor::RenderLeftPanel()
	{
        bool bRecompiled = false;

        Splitter(true, 4.0f, &m_LeftPanelWidth, &m_RightPanelWidth, 50.0f, 50.0f);
        float panelWidth = m_LeftPanelWidth - 4.0f;

        auto& io = ImGui::GetIO();

        ImGui::BeginChild("Selection", ImVec2(panelWidth, 0));

        panelWidth = ImGui::GetContentRegionAvail().x;

        ImGui::BeginHorizontal("Style Editor", ImVec2(panelWidth, 0));
        ImGui::Spring(0.0f, 0.0f);
        if (ImGui::Button("Compile"))
        {
            Compile();
            bRecompiled = true;
        }
        if (ImGui::Button("Save"))
            Save();
        if (ImGui::Button("Zoom to Content"))
            ed::NavigateToContent();
        ImGui::Spring();
        ImGui::EndHorizontal();
        ImGui::Separator();

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImGui::GetCursorScreenPos() + ImVec2(panelWidth, ImGui::GetTextLineHeight() * 1.35f),
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
                m_History.clear();
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

        if (m_RenderPreviewPanelCallback)
        {
            float unused = 0;

            ImGui::Separator();
            m_RenderPreviewPanelCallback(panelWidth, m_PreviewPanelHeight);

            Splitter(false, 4.0f, &unused, &m_PreviewPanelHeight, 50.0f, 50.0f);
            ImGui::Spacing();
            ImGui::Spacing();
        }

        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Variables"))
            {
                RenderVariablesUI();
                RenderSelectedVariableDetails();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Preview Variables"))
            {
                if (m_RenderPreviewVarsCallback)
                    m_RenderPreviewVarsCallback();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::EndChild();

        ImGui::SameLine(0.0f, 12.0f);

        return bRecompiled;
	}

    void GraphEditor::OnEvent(Event& e)
    {
        if (e.Handled || !m_bGraphFocused)
            return;

        m_Graphs.back()->OnEvent(e); // Pass the event to a graph that's opened

        Event::Dispatch<MouseButtonPressedEvent>(e, EG_BIND_FN(GraphEditor::OnMousePressedEvent));
        Event::Dispatch<KeyPressedEvent>(e, EG_BIND_FN(GraphEditor::OnKeyPressedEvent));
    }

    bool GraphEditor::OnKeyPressedEvent(KeyPressedEvent& e)
    {
        if (e.GetKey() == Key::S && Input::IsKeyPressed(Key::LeftControl))
        {
            Save();
            return true;
        }
        return false;
    }

    bool GraphEditor::OnMousePressedEvent(MouseButtonPressedEvent& e)
    {
        Mouse button = e.GetMouseCode();
        if (button == Mouse::Button3)
        {
            if (m_Graphs.size() > 1)
            {
                m_History.push_back(m_Graphs.back());
                m_Graphs.pop_back();
                return true;
            }
        }
        else if (button == Mouse::Button4)
        {
            if (m_History.size())
            {
                m_Graphs.push_back(m_History.back());
                m_History.pop_back();
                return true;
            }
        }

        return false;
    }

    void GraphEditor::RenderVariablesUI()
    {
        if (ImGui::Button("Create variable"))
        {
            const std::string name = CreateNewVar<GraphVariableBool>(nullptr);
            SelectVariable(name);
            OnGraphChanged();
        }
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
                            // It's ok to update only the first one since it'll pass the event to its subgraphs as well
                            m_Graphs[0]->OnVariableDeleted(name);
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
    }

    void GraphEditor::RenderSelectedVariableDetails()
    {
        // Selected var
        auto var = GetVariable(m_SelectedVar);
        if (!var)
            return;

        bool bRename = false;
        UI::TextWithSeparator("Details");
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
                const bool bShowInUI = var->bShowInUI;
                ChangeVariableType(m_SelectedVar, type);
                var = GetVariable(m_SelectedVar);
                var->bShowInUI = bShowInUI;
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
                case GraphVariableType::Int:
                {
                    auto valueVar = Cast<GraphVariableInt>(var);
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
                case GraphVariableType::String:
                {
                    auto valueVar = Cast<GraphVariableString>(var);
                    if (UI::PropertyText("Value", valueVar->Value))
                        OnGraphChanged();
                    break;
                }
                case GraphVariableType::Vec4:
                {
                    auto valueVar = Cast<GraphVariableVec4>(var);
                    if (UI::PropertyDrag("Value", valueVar->Value, 0.05f))
                        OnGraphChanged();
                    break;
                }
                }
            }

            if (UI::Property("Show in UI", var->bShowInUI))
            {
                OnGraphChanged();
            }

            UI::EndPropertyGrid();
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

    bool GraphEditor::ChangeVariableType(const std::string& varName, GraphVariableType newType)
    {
        if (RemoveVariable(varName))
        {
            bool bSuccess = CreateVariable(CreateAnimGraphVariableFromType(newType), varName);
            if (bSuccess)
            {
                for (auto& graph : m_Graphs)
                {
                    graph->OnVariableTypeChanged(varName, newType);
                }
            }
            return bSuccess;
        }
        return false;
    }

    bool GraphEditor::RenameVariable(std::string varName, const std::string& newName)
    {
        auto it = m_Variables.find(varName);
        if (it == m_Variables.end() || GetVariable(newName))
            return false;

        auto var = it->second;
        RemoveVariable(varName);
        CreateVariable(var, newName);

        // It's ok to update only the first one since it'll pass the event to its subgraphs as well
        m_Graphs[0]->OnVariableRenamed(varName, newName);

        return true;
    }

    void GraphEditor::SelectVariable(const std::string& var)
    {
        m_SelectedVar = var;
        m_RenamingVarTemp = var;
    }

    GraphEditorSerializationData GraphEditor::Save()
    {
        GraphEditorSerializationData result;

        const auto& graphToSave = m_Graphs[0];
        result.Graph = graphToSave->Serialize();

        const auto& variables = GetVariables();
        // Vars
        for (const auto& [name, value] : variables)
        {
            auto& var = result.Variables.emplace_back();
            var.Name = name;
            var.Value = value;
        }

        return result;
    }
    
    void GraphEditor::SetInFocus()
    {
        if (ImGuiWindow* window = ImGui::FindWindowByName(m_Name.c_str()))
            ImGui::FocusWindow(window);
    }
}
