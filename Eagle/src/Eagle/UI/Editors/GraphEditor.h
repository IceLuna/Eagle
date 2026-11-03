#pragma once

#include "Eagle/UI/Graphs/UIGraph.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Core/Serializer.h"

namespace Eagle
{
    // TODO: Shouldn't exist on Engine side. Move to editor.
	// Base class for graph editors
	class GraphEditor
	{
    public:
        GraphEditor(const std::string_view name);
        virtual ~GraphEditor();

        // Returns true if the graph was recompiled this frame
        virtual bool OnImGuiRender(bool* pOpen = nullptr);
        // Returns true if the graph was recompiled this frame
        virtual bool RenderLeftPanel();

        // @canvasMin. The top-left corner of the canvas, in screen space
        // @canvasSize. Its size
        // @zoom. The canvas' scale. 1 is the actual size
        void RenderNavigationBar(ImVec2 canvasMin, ImVec2 canvasSize, float zoom);

        // @canvasMin. The top-left corner of the canvas, in screen space
        // @canvasSize. Its size
        void RenderGraphBackground(ImVec2 canvasMin, ImVec2 canvasSize);

        // What kind of graph the editor edits, such as "ANIMATION". It's shown faintly in the corner of the graph. Empty for none
        virtual std::string_view GetGraphTypeLabel() const { return {}; }

        bool NavigateBack();
        bool NavigateForward();
        void NavigateBackTo(size_t depth); // Goes back to the graph at `depth` in the path to the opened graph (0 is the root graph)

        virtual void OnEvent(Event& e);

        virtual void Compile() {}

        // Writes the graphs & variables into the asset (see `WriteToAsset()`), and saves the asset to disk
        virtual void Save() = 0;

        // Writes the graphs & variables into the asset, without saving the asset to disk. The asset is marked as modified.
        // The asset is what everything else uses (saving it from outside the editor, compiling it, the game), so it's kept up to date with the editor
        virtual void WriteToAsset() = 0;

        // @bCloneValues. Values (variables, default values of pins) are copies, so the result isn't affected when the graphs are edited further.
        // For example, for undo
        GraphEditorSerializationData CaptureState(bool bCloneValues = false) const;

        // Replaces the graphs & variables with a state captured by `CaptureState()`.
        // The views (scroll & zoom) of the graphs, the opened graph, and the selected nodes are kept where possible
        void RestoreState(const GraphEditorSerializationData& data);

        // True while `RestoreState()` rebuilds the graphs
        bool IsRestoringState() const { return m_bRestoringState; }

        const UIGraph& GetRootGraph() const { return *m_Graphs[0]; }

        // Called right after the editor's window has begun, before its content is drawn
        void SetBeginWindowCallback(const std::function<void()>& func)
        {
            m_BeginWindowCallback = func;
        }

        void SetInFocus();

        template<typename T, typename... Args>
        void AddGraph(const std::string_view name, Args&&... args)
        {
            m_History.clear();
            OnAddGraphPre();
            m_GraphsToAdd.emplace_back(MakeRef<T>(*this, name, std::forward<Args>(args)...));
            OnAddGraphPost();
        }

        template<typename T>
        void AddGraph(const Ref<T>& graph)
        {
            m_History.clear();
            OnAddGraphPre();
            m_GraphsToAdd.emplace_back(graph);
            OnAddGraphPost();
        }

        bool CreateVariable(const Ref<GraphVariable>& var, const std::string& name)
        {
            if (m_Variables.find(name) != m_Variables.end())
                return false;

            m_Variables.emplace(name, var);
            return true;
        }

        template <typename T>
        std::string CreateNewVar(const Ref<GraphVariable>& defaultVal, const std::string& baseName = "NewVar")
        {
            static_assert(std::is_base_of_v<GraphVariable, T> == true, "Not a var");

            Ref<T> var = MakeRef<T>();
            if (defaultVal)
            {
                if (auto casted = Cast<T>(defaultVal))
                    var->Value = casted->Value;
            }
            std::string name = baseName;
            if (CreateVariable(var, name) == false)
            {
                size_t i = 1;
                do
                {
                    name = baseName + std::to_string(i++);
                } while (CreateVariable(var, name) == false);
            }
            return name;
        }

        std::string CreateNewVarFromType(GraphVariableType type, const Ref<GraphVariable>& defaultVal, const std::string& baseName = "NewVar")
        {
            switch (type)
            {
            case GraphVariableType::Bool: return CreateNewVar<GraphVariableBool>(defaultVal, baseName);
            case GraphVariableType::Int: return CreateNewVar<GraphVariableInt>(defaultVal, baseName);
            case GraphVariableType::Float: return CreateNewVar<GraphVariableFloat>(defaultVal, baseName);
            case GraphVariableType::Animation: return CreateNewVar<GraphVariableAnimation>(defaultVal, baseName);
            case GraphVariableType::String: return CreateNewVar<GraphVariableString>(defaultVal, baseName);
            case GraphVariableType::Vec4: return CreateNewVar<GraphVariableVec4>(defaultVal, baseName);
            }
            EG_CORE_ASSERT(false);
            return "";
        }

        virtual bool ChangeVariableType(const std::string& varName, GraphVariableType newType);

        // Make a copy of a 'varName' instead of a ref, so that we're sure it doesn't change mid execution.
        // For example, if a ref was used, calling `RenameVariable(node.GetName(), newName)` would result in a bug, since `node.GetName()` would change at some point
        // And other graphs would get an updated `varName` instead of an old one.
        virtual bool RenameVariable(std::string varName, const std::string& newName);

        bool RemoveVariable(const std::string& name)
        {
            return m_Variables.erase(name) == 1; // Returns true if removed successfully
        }

        void SelectVariable(const std::string& var);

        GraphVariableType GetVariableType(PinType type)
        {
            switch (type)
            {
            case PinType::Bool: return GraphVariableType::Bool;
            case PinType::Float: return GraphVariableType::Float;
            case PinType::String: return GraphVariableType::String;
            case PinType::Vec4: return GraphVariableType::Vec4;
            case PinType::Int: return GraphVariableType::Int;
            case PinType::Object: return GraphVariableType::Animation;
            }
            EG_CORE_ASSERT(false);
            return GraphVariableType::Bool;
        }

        virtual void OnGraphChanged() {}
        virtual void OnAddGraphPre() = 0;
        virtual void OnAddGraphPost() = 0;

        // Creates an empty base graph (its initial nodes only). Used to rebuild the graphs (see `RestoreState()`)
        virtual Ref<UIGraph> CreateRootGraph() = 0;

        const ed::Config& GetConfig() const { return m_Config; }
        const VariablesMap& GetVariables() const { return m_Variables; }
        const Ref<GraphVariable>& GetVariable(const std::string& name) const
        {
            static Ref<GraphVariable> s_Null;
            auto it = m_Variables.find(name);
            if (it != m_Variables.end())
                return it->second;

            return s_Null;
        }

        const std::vector<CachedNodeData>& GetPoseCacheNodes() const { return m_PoseCacheNodes; }
        std::vector<CachedNodeData>& GetPoseCacheNodes() { return m_PoseCacheNodes; }

        const Ref<Texture2D>& GetHeaderTexture() const { return m_HeaderTexture; }
        ImTextureID GetHeaderTextureID() const { return m_HeaderBackground; }

        const char* GetVarDragDropTag() { return m_VarDragDropTag.c_str(); }

        bool OnKeyPressedEvent(KeyPressedEvent& e);
        bool OnMousePressedEvent(MouseButtonPressedEvent& e);

        void SetRenderPreviewPanelCallback(const std::function<void(float, float)>& func)
        {
            m_RenderPreviewPanelCallback = func;
        }

        void SetRenderPreviewVarsCallback(const std::function<void()>& func)
        {
            m_RenderPreviewVarsCallback = func;
        }

        void SetRenderAssetSettingsCallback(const std::function<void()>& func)
        {
            m_RenderAssetSettingsCallback = func;
        }

        void RenderAssetSettings();

    private:
        void RenderVariablesUI();
        void RenderSelectedVariableDetails();

    protected:
        ed::Config m_Config;
        std::string m_Name = "Graph Editor";

        // Graphs. It always has at least 1 element for the base graph.
        // Other graphs can be opened within a graph, so they're stored here.
        // The last element is the currently opened graph
        std::vector<Ref<UIGraph>> m_Graphs;

        // Delayed graps that'll be added at the beginning of the next frame
        std::vector<Ref<UIGraph>> m_GraphsToAdd;

        std::string m_SelectedVar;
        std::string m_RenamingVarTemp;

        Ref<Texture2D> m_HeaderTexture;
        ImTextureID m_HeaderBackground = 0;

        std::vector<Ref<UIGraph>> m_History; // For back/forward navigation

        // Navigation requested by the navigation bar. It's applied once the opened graph is done drawing, since it can close that graph
        enum class NavigationRequest
        {
            None, Back, Forward, BackTo
        };
        NavigationRequest m_NavigationRequest = NavigationRequest::None;
        size_t m_NavigationRequestDepth = 0; // For `NavigationRequest::BackTo`

        VariablesMap m_Variables;

        std::vector<CachedNodeData> m_PoseCacheNodes;
        // Generate a unique tag to prevent being able to drag&drop between different editors
        std::string m_VarDragDropTag = std::to_string(Random::UInt64());

        std::function<void(float, float)> m_RenderPreviewPanelCallback;
        std::function<void()> m_RenderPreviewVarsCallback;
        std::function<void()> m_RenderAssetSettingsCallback;
        std::function<void()> m_BeginWindowCallback;

        float m_LeftPanelWidth = 400.f;
        float m_RightPanelWidth = 800.f;
        float m_PreviewPanelHeight = 500.f;
        bool m_bIgnoreChangedEvent = true;
        bool m_bRestoringState = false;
        bool m_bGraphFocused = false;
    };
}
