#pragma once

#include "UIGraph.h"

namespace Eagle
{
	class Texture2D;

	// Main graph that's displayed when animation graph editor is opened
	class AnimationStateMachineGraph : public UIGraph
	{
	public:
		AnimationStateMachineGraph(GraphEditor& editor, const std::string_view name);

		void DrawLinks() override;

		Node* GetOutputNode() override { return FindNode(m_EntryNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_EntryNodeId; };

	protected:
		void SetupInitialNodes();
		void SetupNodeFactory();

	private:
		ax::NodeEditor::NodeId m_EntryNodeId;

		Ref<Texture2D> m_ArrowTexture;
	};
}
