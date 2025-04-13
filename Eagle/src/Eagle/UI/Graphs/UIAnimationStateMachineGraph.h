#pragma once

#include "UIGraph.h"

namespace Eagle
{
	class Texture2D;
	class UIAnimationStateTransitionGraph;
	class AnimationStateMachineGraph;
	class AnimationStateGraph;
	class AssetSkeletalMesh;

	// Main graph that's displayed when animation graph editor is opened
	class UIAnimationStateMachineGraph : public UIGraph
	{
	public:
		UIAnimationStateMachineGraph(GraphEditor& editor, const std::string_view name);

		void DrawLinks() override;

		Node* GetOutputNode() override { return FindNode(m_EntryNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_EntryNodeId; };

		void OnNodeAdded(Node& node) override;
		void OnNodeDeleted(const Node& node) override;

		// Can return serialization data of inner graphs as well
		GraphSerializationData Serialize() const override;
		void Deserialize_Internal(const GraphEditorSerializationData& editorData, const GraphSerializationData& data, std::vector<UIGraph*>& deserializedGraphs, std::vector<PoseCacheGetterDeserializationData>& poseCacheGetterData) override;

		// @outUsedVars. Map of variables that were used by this graph
        // @return. Returns an object that can be used to run compiled logic
		Ref<GraphNode> Compile_Internal(Node* node, VariablesMap& outUsedVars, std::unordered_set<UIGraph*> compiledGraphs = {}) override;

		void OnVariableRenamed(const std::string& varName, const std::string& newName) override;

	protected:
		void SetupInitialNodes();
		void SetupNodeFactory();
		bool ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin) override;
		bool CanSpawnVariables() const override { return false; }
		bool AllowMultipleLinksToInput() const override { return true; }

		bool RenameGraph(std::string graphName, const std::string& newName) override;

		void OnLinkCreated(const Link& link) override;
		void OnLinkDeleted(const Link& link) override;

	private:
		Ref<AnimationStateGraph> CompileStateNode(Node* node, const Ref<AssetSkeletalMesh>& skeletalAsset, const Ref<AnimationStateMachineGraph>& stateMachine, VariablesMap& outVariables);
		Ref<AnimationStateGraph> Parse(Node* node, const Ref<AssetSkeletalMesh>& skeletalAsset, const Ref<AnimationStateMachineGraph>& stateMachine, bool bCloneVars, VariablesMap& outVariables);

	private:
		ax::NodeEditor::NodeId m_EntryNodeId;

		std::vector<ed::NodeId> m_StateNodes;

		// Each link in this graph has two transition graphs
		std::unordered_map<ed::LinkId, std::array<Ref<UIAnimationStateTransitionGraph>, 2>> m_LinkTransitions;

		Ref<Texture2D> m_ArrowTexture;

		// Key - node ID
		// Value - compiled state graph
		// It's a cache of processed nodes that's needed to avoid the compilation of already compiled nodes
		std::unordered_map<ed::NodeId, Ref<AnimationStateGraph>> m_CompiledNodes;
	};
}
