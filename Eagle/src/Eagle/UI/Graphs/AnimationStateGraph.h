#pragma once

#include "UIGraph.h"

namespace Eagle
{
	class AnimationStateGraph : public UIGraph
	{
	public:
		AnimationStateGraph(GraphEditor& editor, const std::string_view name);

		Node* GetOutputNode() override { return FindNode(m_OutputNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_OutputNodeId; };

	protected:
		void SetupInitialNodes();
		void SetupNodeFactory();

	private:
		ax::NodeEditor::NodeId m_OutputNodeId;
	};
}
