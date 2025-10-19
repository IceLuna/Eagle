#pragma once

#include "Eagle/UI/Graphs/UIGraph.h"
#include "Eagle/UI/Editors/BehaviorGraphEditor.h"

namespace Eagle
{
	class Texture2D;

	// Main graph that's displayed when behavior graph editor is opened
	class UIBehaviorGraph : public UIGraph
	{
	public:
		UIBehaviorGraph(BehaviorGraphEditor& editor, const std::string_view name);
		~UIBehaviorGraph();

		const Ref<AssetBehaviorGraph>& GetBehaviorGraphAsset() const { return m_Asset; }

		void DrawLinks() override;
		void OnImGuiRender(bool* pOpen = nullptr) override;

		Node* GetOutputNode() override { return FindNode(m_EntryNodeId); };
		ax::NodeEditor::NodeId GetOutputNodeID() override { return m_EntryNodeId; };

		void DrawCreateNewNodePopup() override;

		void RebuildBehaviorTree();
		void SetBuildsDisabled(bool bDisable) { m_bDisableBuilds = bDisable; }
		void CheckIfNodesAreValid();

	protected:
		void SetupInitialNodes();
		bool ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin) override;
		bool CanSpawnVariables() const override { return false; }
		bool AllowMultipleLinksToInput() const override { return false; }
		bool AllowRenaming() const override { return false; }
		void DrawNodeContextPopup() override;
		void OnAppAssemblyReloaded();
		void RenderLeftPanel();
		void HandleSelectedNode();

		bool AreConnected_DeepSearch(const Node* startNode, const Node* endNode) const;
		void RebuildBehaviorTree_Internal(Node* root, AIBehaviorNode& nodeData, size_t& index);

		void OnLinkCreated(const Link& link) override;
		void OnLinkDeleted(const Link& link) override;

	private:
		Ref<AssetBehaviorGraph> m_Asset;
		ax::NodeEditor::NodeId m_EntryNodeId;
		GUID m_AppAssemblyReloadedCallback = GUID(0, 0);
		Node* m_Selected = nullptr;

		float m_LeftPanelWidth = 400.f;
		float m_RightPanelWidth = 800.f;

		bool m_bRebuild = false;

		// If enabled, `RebuildBehaviorTree()` won't do anything. Needed during deserialization to avoid rebuilding it during graph creation
		bool m_bDisableBuilds = false;
		bool m_bNodesValid = true;
	};
}
