#pragma once

#include "UIGraph.h"

namespace Eagle
{
	class UIAnimationStateTransitionGraph : public UIGraph
	{
	public:
		UIAnimationStateTransitionGraph(GraphEditor& editor, const std::string_view name);

		Node* GetOutputNode() override { return FindNode(m_OutputNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_OutputNodeId; };

	protected:
		void SetupInitialNodes();

	private:
		ax::NodeEditor::NodeId m_OutputNodeId;
	};
}
