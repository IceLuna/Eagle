#pragma once

#include "UIGraph.h"

namespace Eagle
{
	// Main graph that's displayed when animation graph editor is opened
	class BaseAnimationGraph : public UIGraph
	{
	public:
		BaseAnimationGraph(GraphEditor& editor, const std::string_view name);

		void Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data) override;
		std::vector<GraphSerializationData> Serialize() override;

		Node* GetOutputNode() override { return FindNode(m_OutputNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_OutputNodeId; };

		void OnNodeAdded(Node& node) override;

	protected:
		void SetupInitialNodes();
		void SetupNodeFactory();

	private:
		ax::NodeEditor::NodeId m_OutputNodeId;
	};
}
