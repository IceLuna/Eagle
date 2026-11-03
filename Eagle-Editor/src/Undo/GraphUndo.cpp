#include "GraphUndo.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/Editors/GraphEditor.h"

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	namespace
	{
		// The nodes of the graph and of all its subgraphs, by `Node::UUID`
		void CollectNodes(const UIGraph& graph, std::unordered_map<GUID, const Node*>& outNodes)
		{
			for (const auto& [_, node] : graph.GetGraphData().Nodes)
			{
				outNodes.emplace(node.UUID, &node);
				if (node.Graph)
					CollectNodes(*node.Graph, outNodes);
			}
		}

		void SummarizeNodes(const GraphSerializationData& graph, const std::unordered_map<GUID, const Node*>& nodes,
			std::unordered_map<GUID, GraphEditorUndoState::NodeSummary>& outNodes)
		{
			for (const auto& node : graph.Nodes)
			{
				GraphEditorUndoState::NodeSummary& summary = outNodes[node.NodeID];

				auto it = nodes.find(node.NodeID);
				const Node* editorNode = it != nodes.end() ? it->second : nullptr;
				summary.Name = editorNode ? editorNode->GetName() : node.Name;
				summary.Position = node.Position;
				summary.Size = editorNode && editorNode->IsResizable() ? node.Size : glm::vec2(0.f);
				summary.Connections = node.OutputConnections;

				YAML::Emitter out;
				Serializer::SerializeGraphNodeContent(out, node);
				summary.Content.assign(out.c_str(), out.size());
			}

			for (const auto& subgraph : graph.Subgraphs)
				SummarizeNodes(subgraph, nodes, outNodes);
		}

		bool HasConnection(const std::vector<GraphConnectionData>& connections, const GraphConnectionData& connection)
		{
			return std::any_of(connections.begin(), connections.end(),
				[&connection](const GraphConnectionData& other) { return other.NodeID == connection.NodeID && other.PinIndex == connection.PinIndex; });
		}

		std::string Quoted(const std::string& name)
		{
			return "'" + name + "'";
		}

		size_t GetGraphMemoryUsage(const GraphSerializationData& graph)
		{
			size_t result = sizeof(GraphSerializationData) + graph.Name.capacity() + graph.Nodes.capacity() * sizeof(GraphNodeSerializationData);
			for (const auto& node : graph.Nodes)
			{
				result += node.Name.capacity() + node.UserData.capacity() + node.InputPins.capacity() * sizeof(InputPinData)
					+ node.OutputConnections.capacity() * sizeof(GraphConnectionData);
			}

			for (const auto& subgraph : graph.Subgraphs)
				result += GetGraphMemoryUsage(subgraph);

			return result;
		}
	}

	// --------------------
	// GraphEditorUndoState
	// --------------------
	GraphEditorUndoState GraphEditorUndoState::Capture(const GraphEditor& editor, const Asset& asset)
	{
		GraphEditorUndoState state;
		state.Data = editor.CaptureState(true);
		state.bAlwaysIncludeInGameBuilds = asset.IsAlwaysIncludedInGameBuilds();

		{
			YAML::Emitter out;
			Serializer::SerializeGraphEditorContent(out, state.Data);
			state.SerializedGraph.assign(out.c_str(), out.size());
		}
		{
			YAML::Emitter out;
			Serializer::SerializeGraphVariables(out, state.Data.Variables);
			state.SerializedVariables.assign(out.c_str(), out.size());
		}
		std::unordered_map<GUID, const Node*> editorNodes;
		CollectNodes(editor.GetRootGraph(), editorNodes);
		SummarizeNodes(state.Data.Graph, editorNodes, state.Nodes);
		return state;
	}

	size_t GraphEditorUndoState::GetMemoryUsage() const
	{
		size_t result = sizeof(*this) + SerializedGraph.capacity() + SerializedVariables.capacity() + Data.Variables.capacity() * sizeof(GraphVariableSerializationData) + GetGraphMemoryUsage(Data.Graph);
		for (const auto& [id, node] : Nodes)
			result += sizeof(GUID) + sizeof(NodeSummary) + node.Name.capacity() + node.Content.capacity() + node.Connections.capacity() * sizeof(GraphConnectionData);

		return result;
	}

	bool GraphEditorUndoState::HasSameGraph(const GraphEditorUndoState& other) const
	{
		if (SerializedGraph != other.SerializedGraph)
			return false;

		// The same nodes, since the graphs are the same
		for (const auto& [id, node] : Nodes)
		{
			auto it = other.Nodes.find(id);
			if (it == other.Nodes.end() || it->second.Size != node.Size)
				return false;
		}

		return true;
	}

	bool GraphEditorUndoState::operator==(const GraphEditorUndoState& other) const
	{
		return bAlwaysIncludeInGameBuilds == other.bAlwaysIncludeInGameBuilds && HasSameGraph(other);
	}

	// -----------------------
	// GraphEditorStateCommand
	// -----------------------
	void GraphEditorStateCommand::Apply(const GraphEditorUndoState& state)
	{
		const Ref<Asset> asset = m_Asset.lock();
		if (!asset)
			return;

		if (!m_Before.HasSameGraph(m_After))
			m_Editor.RestoreState(state.Data);

		asset->SetAlwaysIncludedInGameBuilds(state.bAlwaysIncludeInGameBuilds);
		asset->SetDirty(true);
	}

	std::string GraphEditorStateCommand::Describe(const GraphEditorUndoState& before, const GraphEditorUndoState& after)
	{
		if (before.HasSameGraph(after))
			return "Edit Always Build";

		// Adding, renaming or deleting a variable can change the nodes that use it as well, so variables are checked first
		if (before.Data.Variables.size() != after.Data.Variables.size())
			return after.Data.Variables.size() > before.Data.Variables.size() ? "Add Variable" : "Delete Variable";

		// Added & deleted nodes. Their links are added or deleted with them
		std::vector<const GraphEditorUndoState::NodeSummary*> added;
		std::vector<const GraphEditorUndoState::NodeSummary*> deleted;
		for (const auto& [id, node] : after.Nodes)
		{
			if (before.Nodes.find(id) == before.Nodes.end())
				added.push_back(&node);
		}
		for (const auto& [id, node] : before.Nodes)
		{
			if (after.Nodes.find(id) == after.Nodes.end())
				deleted.push_back(&node);
		}

		if (!added.empty() && deleted.empty())
			return added.size() == 1u ? "Add Node " + Quoted(added[0]->Name) : "Add " + std::to_string(added.size()) + " Nodes";
		if (!deleted.empty() && added.empty())
			return deleted.size() == 1u ? "Delete Node " + Quoted(deleted[0]->Name) : "Delete " + std::to_string(deleted.size()) + " Nodes";
		if (!added.empty())
			return "Edit Graph";

		// Renaming a variable renames the nodes that use it
		if (before.SerializedVariables != after.SerializedVariables)
			return "Edit Variables";

		// The same nodes. What changed in them
		std::vector<const GraphEditorUndoState::NodeSummary*> moved;
		std::vector<const GraphEditorUndoState::NodeSummary*> resized;
		std::vector<const GraphEditorUndoState::NodeSummary*> edited;
		std::vector<std::pair<const GraphEditorUndoState::NodeSummary*, GUID>> connected; // From, to
		std::vector<std::pair<const GraphEditorUndoState::NodeSummary*, GUID>> disconnected;
		for (const auto& [id, nodeAfter] : after.Nodes)
		{
			const GraphEditorUndoState::NodeSummary& nodeBefore = before.Nodes.at(id);
			const bool bResized = nodeBefore.Size != nodeAfter.Size;
			if (bResized)
				resized.push_back(&nodeAfter);
			else if (nodeBefore.Position != nodeAfter.Position) // Resizing a node by its top or left edge moves it too
				moved.push_back(&nodeAfter);
			if (nodeBefore.Content != nodeAfter.Content)
				edited.push_back(&nodeAfter);

			for (const GraphConnectionData& connection : nodeAfter.Connections)
			{
				if (!HasConnection(nodeBefore.Connections, connection))
					connected.emplace_back(&nodeAfter, connection.NodeID);
			}
			for (const GraphConnectionData& connection : nodeBefore.Connections)
			{
				if (!HasConnection(nodeAfter.Connections, connection))
					disconnected.emplace_back(&nodeAfter, connection.NodeID);
			}
		}

		auto getName = [&after](const GUID& nodeID)
		{
			auto it = after.Nodes.find(nodeID);
			return it != after.Nodes.end() ? Quoted(it->second.Name) : std::string("a node");
		};

		const bool bLinksChanged = !connected.empty() || !disconnected.empty();
		if (bLinksChanged && moved.empty() && resized.empty() && edited.empty())
		{
			// Connecting to an input that's already connected replaces its link
			if (connected.size() == 1u && disconnected.size() <= 1u)
				return "Connect " + Quoted(connected[0].first->Name) + " to " + getName(connected[0].second);

			if (disconnected.size() == 1u && connected.empty())
				return "Disconnect " + Quoted(disconnected[0].first->Name) + " from " + getName(disconnected[0].second);

			return "Edit Links";
		}

		if (!edited.empty() && moved.empty() && resized.empty() && !bLinksChanged)
			return edited.size() == 1u ? "Edit Node " + Quoted(edited[0]->Name) : "Edit " + std::to_string(edited.size()) + " Nodes";

		if (!moved.empty() && resized.empty() && edited.empty() && !bLinksChanged)
			return moved.size() == 1u ? "Move Node " + Quoted(moved[0]->Name) : "Move " + std::to_string(moved.size()) + " Nodes";

		if (!resized.empty() && moved.empty() && edited.empty() && !bLinksChanged)
			return resized.size() == 1u ? "Resize Node " + Quoted(resized[0]->Name) : "Resize " + std::to_string(resized.size()) + " Nodes";

		return "Edit Graph";
	}

	// ----------------
	// GraphEditTracker
	// ----------------
	GraphEditTracker::GraphEditTracker(GraphEditor& editor, const Ref<Asset>& asset)
		: m_Editor(editor)
		, m_Asset(asset)
		, m_Tracker(
			[](const Target& target) -> std::optional<GraphEditorUndoState>
			{
				const Ref<Asset> asset = target.TargetAsset.lock();
				if (!asset || !target.Editor)
					return {};
				return GraphEditorUndoState::Capture(*target.Editor, *asset);
			},
			[](UndoHistory& history, const Target& target, GraphEditorUndoState&& before, GraphEditorUndoState&& after)
			{
				const Ref<Asset> asset = target.TargetAsset.lock();
				if (!asset || !target.Editor)
					return;

				std::string name = GraphEditorStateCommand::Describe(before, after);
				history.Record(std::move(name), MakeScope<GraphEditorStateCommand>(*target.Editor, asset, std::move(before), std::move(after)));
			},
			true) // The node editor creates & deletes links, and deletes nodes, a frame or two after the input that caused it
	{
	}

	void GraphEditTracker::BeginFrame(UndoHistory* history)
	{
		m_Tracker.BeginFrame(m_Asset.expired() ? nullptr : history, Target{ &m_Editor, m_Asset });
	}

	// --------------
	// GraphAssetSync
	// --------------
	void GraphAssetSync::Update()
	{
		if (m_History.GetVersion() == m_WrittenVersion)
			return;

		m_WrittenVersion = m_History.GetVersion();
		m_Editor.WriteToAsset();
	}
}
