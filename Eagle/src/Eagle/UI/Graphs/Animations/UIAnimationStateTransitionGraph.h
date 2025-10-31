#pragma once

#include "Eagle/UI/Graphs/UIGraph.h"

namespace Eagle
{
	class UIAnimationStateTransitionGraph : public UIGraph
	{
	public:
		UIAnimationStateTransitionGraph(GraphEditor& editor, const std::string_view name);

		Node* GetOutputNode() override { return FindNode(m_OutputNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() const override { return m_OutputNodeId; };

	protected:
		void SetupInitialNodes();

	private:
		ax::NodeEditor::NodeId m_OutputNodeId;

		friend class UIAnimationStateMachineGraph; // To call `Deserialize_Internal`
	};
}
