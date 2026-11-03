#pragma once

#include "UndoHistory.h"
#include "EditTracker.h"

#include "Eagle/Core/Serializer.h"

#include <unordered_map>

namespace Eagle
{
	class Asset;
	class GraphEditor;

	// The state of a graph editor, its graphs & variables, and the "Always build" setting of its asset
	struct GraphEditorUndoState
	{
		// Each node, split into what can change, to describe what was changed (for example, "Move Node 'Blend'" or "Connect 'Idle' to 'Output'")
		struct NodeSummary
		{
			std::string Name;
			glm::vec2 Position = glm::vec2(0.f);
			glm::vec2 Size = glm::vec2(0.f); // Only of the nodes that are resized by the user
			std::string Content; // See `Serializer::SerializeGraphNodeContent()`
			std::vector<GraphConnectionData> Connections; // Outputs
		};

		GraphEditorSerializationData Data;
		std::string SerializedGraph; // Without the view (scroll & zoom) and the sizes of the nodes. It's what's compared, along with the sizes in `Nodes`
		std::string SerializedVariables;
		std::unordered_map<GUID, NodeSummary> Nodes; // Of every graph, by `GraphNodeSerializationData::NodeID`
		bool bAlwaysIncludeInGameBuilds = false;

		static GraphEditorUndoState Capture(const GraphEditor& editor, const Asset& asset);

		size_t GetMemoryUsage() const;

		// The graphs & variables, without "Always build"
		bool HasSameGraph(const GraphEditorUndoState& other) const;
		bool operator==(const GraphEditorUndoState& other) const;
	};

	// Restores the graphs & variables of a graph editor, and "Always build" of its asset. The graphs are written into the asset afterwards (see `GraphAssetSync`).
	class GraphEditorStateCommand : public UndoCommand
	{
	public:
		GraphEditorStateCommand(GraphEditor& editor, const Ref<Asset>& asset, GraphEditorUndoState before, GraphEditorUndoState after)
			: m_Editor(editor), m_Asset(asset), m_Before(std::move(before)), m_After(std::move(after)) {}

		void Undo() override { Apply(m_Before); }
		void Redo() override { Apply(m_After); }
		bool CanApply(bool bUndo) const override { return !m_Asset.expired(); }
		size_t GetMemoryUsage() const override { return sizeof(*this) + m_Before.GetMemoryUsage() + m_After.GetMemoryUsage(); }

		// For example, "Add Node 'Blend'", or "Connect 'Idle' to 'Output'", etc
		static std::string Describe(const GraphEditorUndoState& before, const GraphEditorUndoState& after);

	private:
		void Apply(const GraphEditorUndoState& state);

	private:
		GraphEditor& m_Editor;
		Weak<Asset> m_Asset;
		GraphEditorUndoState m_Before;
		GraphEditorUndoState m_After;
	};

	// Turns the edits made in a graph editor (nodes, links, pins, their positions, variables, etc) into undo transactions.
	class GraphEditTracker
	{
	public:
		GraphEditTracker(GraphEditor& editor, const Ref<Asset>& asset);

		void BeginFrame(UndoHistory* history);
		void EndFrame() { m_Tracker.EndFrame(); }
		void Reset() { m_Tracker.Reset(); }

	private:
		struct Target
		{
			GraphEditor* Editor = nullptr;
			Weak<Asset> TargetAsset;

			bool operator==(const Target& other) const { return Editor == other.Editor && !TargetAsset.owner_before(other.TargetAsset) && !other.TargetAsset.owner_before(TargetAsset); }
		};

		GraphEditor& m_Editor;
		Weak<Asset> m_Asset;
		EditTracker<Target, GraphEditorUndoState> m_Tracker;
	};

	// Keeps the asset of a graph editor up to date with the editor's graphs.
	// Every change of the graphs is recorded into the editor's history, or undone/redone in it, so they're written whenever the history changes.
	// Call `Update()` every frame after the editor was drawn. It's called on destruction as well, so it has to be destroyed before the editor
	class GraphAssetSync
	{
	public:
		GraphAssetSync(GraphEditor& editor, const UndoHistory& history)
			: m_Editor(editor), m_History(history), m_WrittenVersion(history.GetVersion()) {}
		~GraphAssetSync() { Update(); }

		GraphAssetSync(const GraphAssetSync&) = delete;
		GraphAssetSync& operator=(const GraphAssetSync&) = delete;

		void Update();

	private:
		GraphEditor& m_Editor;
		const UndoHistory& m_History;
		uint64_t m_WrittenVersion = 0;
	};
}
