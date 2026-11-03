#include "egpch.h"
#include "GraphEditor.h"

#include "Eagle/Core/Application.h"
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

    static void CloneDefaultValues(GraphSerializationData& graph)
    {
        for (auto& node : graph.Nodes)
        {
            for (auto& pin : node.InputPins)
            {
                if (pin.DefaultValue)
                    pin.DefaultValue = pin.DefaultValue->Clone();
            }
        }

        for (auto& subgraph : graph.Subgraphs)
            CloneDefaultValues(subgraph);
    }

    GraphEditor::GraphEditor(const std::string_view name)
        : m_Name(name)
    {
        // Note: nothing here should touch ImGui, imgui-node-editor or the renderer,
        // because graph editors are also created to compile graphs when assets are loaded/deserialized, which can happen on any thread.
        // UI resources should be created when the editor is drawn for the first time

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
        if (!m_HeaderTexture)
            m_HeaderTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/BlueprintBackground.png");
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
            if (m_BeginWindowCallback)
                m_BeginWindowCallback();

            m_bGraphFocused = ImGui::IsWindowFocused();
            bRecompiled = RenderLeftPanel();

            auto& currentGraph = m_Graphs.back();
            if (bIgnoreChanges)
                OnAddGraphPre();

            currentGraph->OnImGuiRender();

            if (bIgnoreChanges)
                OnAddGraphPost();

            switch (m_NavigationRequest)
            {
                case NavigationRequest::Back: NavigateBack(); break;
                case NavigationRequest::Forward: NavigateForward(); break;
                case NavigationRequest::BackTo: NavigateBackTo(m_NavigationRequestDepth); break;
                case NavigationRequest::None: break;
            }
            m_NavigationRequest = NavigationRequest::None;
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
        {
            ed::SetCurrentEditor(m_Graphs.back()->GetEditorContext());
            ed::NavigateToContent();
        }
        ImGui::Spring();
        ImGui::EndHorizontal();
        ImGui::Separator();

        RenderAssetSettings();

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

    void GraphEditor::RenderGraphBackground(ImVec2 canvasMin, ImVec2 canvasSize)
    {
        const ImVec2 canvasMax = canvasMin + canvasSize;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(canvasMin, canvasMax, ImGui::ColorConvertFloat4ToU32(ed::GetStyle().Colors[ed::StyleColor_Bg]));

        // The type of the graph, faintly in the bottom-right corner
        const std::string_view typeLabel = GetGraphTypeLabel();
        if (!typeLabel.empty())
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            UI::PushFontHeader2(ImGui::GetFontSize() * 3.f);
            const ImVec2 textSize = ImGui::CalcTextSize(typeLabel.data(), typeLabel.data() + typeLabel.size());
            ImVec4 color = style.Colors[ImGuiCol_Text];
            color.w *= 0.12f;
            drawList->AddText(canvasMax - style.WindowPadding - textSize, ImGui::GetColorU32(color), typeLabel.data(), typeLabel.data() + typeLabel.size());
            UI::PopFont();
        }
    }

    void GraphEditor::RenderNavigationBar(ImVec2 canvasMin, ImVec2 canvasSize, float zoom)
    {
        if (canvasSize.x <= 0.f || canvasSize.y <= 0.f)
            return;

        // A child window, so that it's drawn on top of the graph, and the graph doesn't react to the mouse while it's over the bar.
        const ImGuiStyle& style = ImGui::GetStyle();
        const float height = ImGui::GetFrameHeight() + style.FramePadding.y * 2.f;
        ImGui::SetCursorScreenPos(canvasMin);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);

        constexpr ImGuiChildFlags childFlags = ImGuiChildFlags_AlwaysUseWindowPadding;
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav;
        if (ImGui::BeginChild("##GraphNavigationBar", ImVec2(canvasSize.x, height), childFlags, windowFlags))
        {
            const ImGuiWindow* window = ImGui::GetCurrentWindow();
            const ImVec2 barMin = window->Pos;
            const ImVec2 barMax = window->Pos + window->Size;
            {
                ImVec4 background = style.Colors[ImGuiCol_WindowBg];
                background.w = 0.75f;
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->PushClipRect(barMin, barMax, true);
                drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(background));
                drawList->PopClipRect();
            }

            // The zoom level, on the right
            char zoomText[32];
            const int zoomPercent = int(zoom * 100.f + 0.5f);
            if (zoomPercent == 100)
                snprintf(zoomText, sizeof(zoomText), "Zoom 1:1");
            else
                snprintf(zoomText, sizeof(zoomText), "Zoom %d%%", zoomPercent);
            const float zoomX = barMax.x - style.FramePadding.x - ImGui::CalcTextSize(zoomText).x;

            // The navigation, on the left. It's clipped before the zoom level, if the graph is too narrow for both
            const ImVec2 lineStart = ImGui::GetCursorScreenPos();
            ImGui::PushClipRect(barMin, ImVec2(ImMax(barMin.x, zoomX - style.ItemSpacing.x), barMax.y), true);

            ImGui::BeginDisabled(m_Graphs.size() <= 1);
            if (ImGui::ArrowButton("##Back", ImGuiDir_Left))
                m_NavigationRequest = NavigationRequest::Back;
            ImGui::SetItemTooltip("Back (Mouse Back Button)");
            ImGui::EndDisabled();

            ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(m_History.empty());
            if (ImGui::ArrowButton("##Forward", ImGuiDir_Right))
                m_NavigationRequest = NavigationRequest::Forward;
            ImGui::SetItemTooltip("Forward (Mouse Forward Button)");
            ImGui::EndDisabled();

            // The path to the opened graph. The graphs it goes through can be clicked to go back to them
            ImGui::SameLine(0.f, style.ItemSpacing.x * 2.f);
            ImGui::AlignTextToFramePadding();
            const size_t graphsCount = m_Graphs.size();
            for (size_t i = 0; i < graphsCount; ++i)
            {
                ImGui::PushID(int(i));
                if (i > 0)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled(">");
                    ImGui::SameLine();
                }

                const std::string& name = m_Graphs[i]->GetName();
                if (i + 1 == graphsCount)
                {
                    UI::PushFontBold();
                    ImGui::TextUnformatted(name.c_str());
                    UI::PopFont();
                }
                else if (ImGui::TextLink(name.c_str()))
                {
                    m_NavigationRequest = NavigationRequest::BackTo;
                    m_NavigationRequestDepth = i;
                }
                ImGui::PopID();
            }
            ImGui::PopClipRect();

            ImGui::SetCursorScreenPos(ImVec2(zoomX, lineStart.y));
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", zoomText);
        }
        ImGui::EndChild();

        ImGui::PopStyleVar();
    }

    bool GraphEditor::NavigateBack()
    {
        if (m_Graphs.size() <= 1)
            return false;

        m_History.push_back(m_Graphs.back());
        m_Graphs.pop_back();
        return true;
    }

    bool GraphEditor::NavigateForward()
    {
        if (m_History.empty())
            return false;

        m_Graphs.push_back(m_History.back());
        m_History.pop_back();
        return true;
    }

    void GraphEditor::NavigateBackTo(size_t depth)
    {
        while (m_Graphs.size() > depth + 1 && NavigateBack())
        {
        }
    }

    void GraphEditor::RenderAssetSettings()
    {
        if (!m_RenderAssetSettingsCallback)
            return;

        m_RenderAssetSettingsCallback();
        ImGui::Separator();
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
        const Mouse button = e.GetMouseCode();
        if (button == Mouse::Button3)
            return NavigateBack();
        if (button == Mouse::Button4)
            return NavigateForward();

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

    GraphEditorSerializationData GraphEditor::CaptureState(bool bCloneValues) const
    {
        GraphEditorSerializationData result;

        result.Graph = m_Graphs[0]->Serialize();
        if (bCloneValues)
            CloneDefaultValues(result.Graph); // Otherwise, they're the values the nodes use

        const auto& variables = GetVariables();
        // Vars
        for (const auto& [name, value] : variables)
        {
            auto& var = result.Variables.emplace_back();
            var.Name = name;
            var.Value = bCloneValues && value ? value->Clone() : value;
        }

        return result;
    }

    void GraphEditor::RestoreState(const GraphEditorSerializationData& data)
    {
        // The nodes are going to use the values, so they're copied again. `data` stays as it is
        GraphEditorSerializationData state = data;
        CloneDefaultValues(state.Graph);

        // Opened graphs keep their views and selected nodes, and stay opened if they still exist. So do the ones that can be reopened with the "forward" mouse button
        std::unordered_map<GUID, std::pair<ImVec2, float>> views;
        std::unordered_map<GUID, std::vector<GUID>> selectedNodes;
        std::vector<GUID> openedGraphs;
        std::vector<GUID> forwardGraphs; // `m_History`, the next one to reopen first. Each one is a subgraph of the previous one (the first one, of the last opened graph)
        auto saveGraphState = [&views, &selectedNodes](const Ref<UIGraph>& graph)
        {
            ImVec2 scroll;
            float zoom = 1.f;
            graph->GetViewState(&scroll, &zoom);
            views[graph->GetID()] = { scroll, zoom };
            selectedNodes[graph->GetID()] = graph->GetSelectedNodes();
        };
        for (size_t i = 0; i < m_Graphs.size(); ++i)
        {
            saveGraphState(m_Graphs[i]);
            if (i > 0)
                openedGraphs.push_back(m_Graphs[i]->GetID());
        }
        for (auto it = m_History.rbegin(); it != m_History.rend(); ++it)
        {
            saveGraphState(*it);
            forwardGraphs.push_back((*it)->GetID());
        }

        auto applyViews = [&views](auto& self, GraphSerializationData& graph) -> void
        {
            if (auto it = views.find(graph.ID); it != views.end())
            {
                graph.ScrollOffset = glm::vec2(it->second.first.x, it->second.first.y);
                graph.Zoom = it->second.second;
            }
            for (auto& subgraph : graph.Subgraphs)
                self(self, subgraph);
        };
        applyViews(applyViews, state.Graph);

        // The graphs refer to the nodes of each other (pose caches), so they're all rebuilt
        m_Graphs.clear();
        m_GraphsToAdd.clear();
        m_History.clear(); // Restored below
        m_PoseCacheNodes.clear();

        m_Variables.clear();
        for (const auto& var : state.Variables)
        {
            if (var.Value)
                CreateVariable(var.Value->Clone(), var.Name);
        }
        if (!GetVariable(m_SelectedVar))
            m_SelectedVar.clear();

        Ref<UIGraph> root = CreateRootGraph();
        m_Graphs.push_back(root);
        m_bRestoringState = true;
        OnAddGraphPre();
        root->Deserialize(state, state.Graph);
        OnAddGraphPost();
        m_bRestoringState = false;

        auto findSubgraph = [](const Ref<UIGraph>& parent, const GUID& graphID) -> Ref<UIGraph>
        {
            for (const auto& [_, node] : parent->GetGraphData().Nodes)
            {
                if (node.Graph && node.Graph->GetID() == graphID)
                    return node.Graph;
            }
            return nullptr;
        };

        bool bAllReopened = true;
        for (const GUID& graphID : openedGraphs)
        {
            Ref<UIGraph> opened = findSubgraph(m_Graphs.back(), graphID);
            if (!opened)
            {
                bAllReopened = false;
                break;
            }
            m_Graphs.push_back(opened);
        }

        std::vector<Ref<UIGraph>> forward;
        if (bAllReopened)
        {
            Ref<UIGraph> parent = m_Graphs.back();
            for (const GUID& graphID : forwardGraphs)
            {
                parent = findSubgraph(parent, graphID);
                if (!parent)
                    break;
                forward.push_back(parent);
            }
        }
        m_History.assign(forward.rbegin(), forward.rend());

        auto restoreSelection = [&selectedNodes](const Ref<UIGraph>& graph)
        {
            if (auto it = selectedNodes.find(graph->GetID()); it != selectedNodes.end())
                graph->SelectNodes(it->second);
        };
        for (const auto& graph : m_Graphs)
            restoreSelection(graph);
        for (const auto& graph : m_History)
            restoreSelection(graph);

        // When the new graphs are drawn for the first time, the node editor reports their nodes as changed
        m_bIgnoreChangedEvent = true;
    }

    void GraphEditor::SetInFocus()
    {
        if (ImGuiWindow* window = ImGui::FindWindowByName(m_Name.c_str()))
            ImGui::FocusWindow(window);
    }
}
