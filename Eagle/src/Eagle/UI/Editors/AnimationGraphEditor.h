#pragma once

#include "GraphEditor.h"
#include "Eagle/Animation/AnimationGraph.h"

namespace Eagle
{
    class AnimationGraphStateMachineEditor;
    class AnimationGraphNode;
    class AssetAnimationGraph;

	class AnimationGraphEditor : public GraphEditor
	{
	public:
		AnimationGraphEditor(const Ref<AssetAnimationGraph>& graph, const std::string& name = "Animation Editor");

        const Ref<AssetAnimationGraph>& GetGraphAsset() const { return m_Graph; }

        GraphEditorSerializationData Save() override;

        void OnGraphChanged() override;
        void OnAddGraphPre() override;
        void OnAddGraphPost() override;

        void DrawEditor();

        void Compile() override;

    private:
        // @bCloneVars. If set to true, vars will be cloned before set to graph
        // @outVariables. All variables that were used
        void Parse(const Ref<UIGraph>& graph, Node* node, bool bCloneVars, VariablesMap& outVariables);

        void Deserialize();

        bool IgnoreChangedEvent() const { return m_bIgnoreChangedEvent; }

	private:
        Ref<AssetAnimationGraph> m_Graph;

        bool m_GraphIsDirty = false;
    };
}
