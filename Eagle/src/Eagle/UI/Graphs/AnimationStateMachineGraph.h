#pragma once

#include "UIGraph.h"

namespace Eagle
{
	class Texture2D;
	class AnimationStateTransitionGraph;

	// Main graph that's displayed when animation graph editor is opened
	class AnimationStateMachineGraph : public UIGraph
	{
	public:
		AnimationStateMachineGraph(GraphEditor& editor, const std::string_view name);

		void DrawLinks() override;

		Node* GetOutputNode() override { return FindNode(m_EntryNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_EntryNodeId; };

		void OnNodeAdded(Node& node) override;
		void OnNodeDeleted(const Node& node) override;

		// Can return serialization data of inner graphs as well
		GraphSerializationData Serialize() const override;
		void Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data) override;

	protected:
		void SetupInitialNodes();
		void SetupNodeFactory();
		bool ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin) override;
		bool CanSpawnVariables() const override { return false; }

		void OnLinkCreated(const Link& link) override;
		void OnLinkDeleted(const Link& link) override;

	private:
		ax::NodeEditor::NodeId m_EntryNodeId;

		std::vector<ed::NodeId> m_StateNodes;

		// Each link in this graph has two transition graphs
		std::unordered_map<ed::LinkId, std::array<Ref<AnimationStateTransitionGraph>, 2>> m_LinkTransitions;

		Ref<Texture2D> m_ArrowTexture;
	};
}
