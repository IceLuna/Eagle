#pragma once

#include "UIGraph.h"

namespace Eagle
{
	// Main graph that's displayed when animation graph editor is opened
	class BaseAnimationGraph : public UIGraph
	{
	public:
		BaseAnimationGraph(GraphEditor& editor, const std::string_view name);

		void Deserialize(const GraphSerializationData& data) override;

		Node* GetOutputNode() override { return FindNode(m_OutputNodeId); };

	protected:
		void SetupInitialNodes();
		void SetupNodeFactory();

	private:
		ax::NodeEditor::NodeId m_OutputNodeId;
	};
}
