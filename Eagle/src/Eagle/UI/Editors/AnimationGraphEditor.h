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
		AnimationGraphEditor(const Ref<AssetAnimationGraph>& graph, const std::string& name = "Animation Graph Editor");

        const Ref<AssetAnimationGraph>& GetGraphAsset() const { return m_Graph; }

        GraphEditorSerializationData Save() override;

        void OnGraphChanged() override;
        void OnAddGraphPre() override;
        void OnAddGraphPost() override;

        void Compile() override;

    private:
        void Deserialize();

        bool IgnoreChangedEvent() const { return m_bIgnoreChangedEvent; }

	private:
        Ref<AssetAnimationGraph> m_Graph;

        bool m_GraphIsDirty = false;
    };
}
