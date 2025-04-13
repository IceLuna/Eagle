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

        virtual void OnImGuiRender(bool* pOpen = nullptr);
        virtual void ShowLeftPane(float paneWidth);

        virtual void OnEvent(Event& e);

        virtual void Compile() = 0;

        virtual GraphEditorSerializationData Save();

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
            case PinType::Object: return GraphVariableType::Animation;
            }
            EG_CORE_ASSERT(false);
            return GraphVariableType::Bool;
        }

        virtual void OnGraphChanged() = 0;
        virtual void OnAddGraphPre() = 0;
        virtual void OnAddGraphPost() = 0;

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
        const Ref<Texture2D>& GetSaveTexture() const { return m_SaveTexture; }
        const Ref<Texture2D>& GetRestoreTexture() const { return m_RestoreTexture; }

        ImTextureID GetHeaderTextureID() const { return m_HeaderBackground; }
        ImTextureID GetSaveTextureID() const { return m_SaveIcon; }
        ImTextureID GetRestoreTextureID() const { return m_RestoreIcon; }

        static const char* GetVarDragDropTag() { return "EDITOR_GRAPH_VAR_TAG"; }

    protected:
        void AddGraph_Internal(const Ref<UIGraph>& graph)
        {
            OnAddGraphPre();
            m_Graphs.emplace_back(graph);
            OnAddGraphPost();
        }

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

        Ref<Texture2D>       m_HeaderTexture;
        Ref<Texture2D>       m_SaveTexture;
        Ref<Texture2D>       m_RestoreTexture;
        ImTextureID          m_HeaderBackground = nullptr;
        ImTextureID          m_SaveIcon = nullptr;
        ImTextureID          m_RestoreIcon = nullptr;

        std::vector<Ref<UIGraph>> m_History; // For back/forward navigation

        VariablesMap m_Variables;

        std::vector<CachedNodeData> m_PoseCacheNodes;

        bool m_bIgnoreChangedEvent = true;
        bool m_bGraphFocused = false;
    };
}
