#include "egpch.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateTransitionGraph.h"

#include "Eagle/Core/Application.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Editors/GraphEditor.h"

#include <glm/ext/scalar_constants.hpp>

namespace Eagle
{
	AnimationStateMachineGraph::AnimationStateMachineGraph(GraphEditor& editor, const std::string_view name)
		: UIGraph(editor, name)
	{
		m_ArrowTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/arrow.png");

        SetupInitialNodes();
        SetupNodeFactory();
	}

    void AnimationStateMachineGraph::OnNodeAdded(Node& node)
    {
        // Should always be true
        if (node.Type == NodeType::StateMachineState)
        {
            // Set unique name for the state node.
            // Do this before calling parent's "OnNodeAdded"
            std::string name = node.UserData;
            uint32_t i = 0;
            bool bContinue = true;
            while (bContinue)
            {
                bContinue = false;
                for (const auto& nodeID : m_StateNodes)
                {
                    Node* graphNode = FindNode(nodeID);
                    if (!graphNode)
                        continue;

                    if (graphNode->UserData == name)
                    {
                        name = node.Name + std::to_string(i++);
                        bContinue = true;
                        break;
                    }
                }
            }
            // The actual name of the "State" node, displayed in UI, is stored here
            node.UserData = std::move(name);
        }

        UIGraph::OnNodeAdded(node);

        if (node.Type == NodeType::StateMachineState)
            m_StateNodes.push_back(node.ID);
    }

    void AnimationStateMachineGraph::OnNodeDeleted(const Node& node)
    {
        UIGraph::OnNodeDeleted(node);

        if (node.Type != NodeType::StateMachineState)
            return;
        
        auto it = std::find_if(m_StateNodes.begin(), m_StateNodes.end(), [node](const ed::NodeId& a)
        {
            return a == node.ID;
        });
        if (it != m_StateNodes.end())
            m_StateNodes.erase(it);
    }

    void AnimationStateMachineGraph::SetupInitialNodes()
    {
        // TODO: Fix deserialization of state machines
        m_EntryNodeId = GraphNodeFactory::SpawnEntryStateNode(*this).ID;
    }

    void AnimationStateMachineGraph::SetupNodeFactory()
    {
        auto& category = m_NodeFactory["State Machine"];
        category["New State"] = &GraphNodeFactory::SpawnState;
        category["Comment"] = &GraphNodeFactory::SpawnComment;
    }

    bool AnimationStateMachineGraph::ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin)
    {
        // Default reject behavior
        if (UIGraph::ProcessNewLinkRejection(startPin, endPin))
            return true;

        // Check if nodes are already connected. If so, reject the link
        // Since it doesn't make sense because the existing link already has a back transition that can be used
        bool bReject = false;
        Node* startNode = FindNode(startPin.NodeID);
        Node* endNode = FindNode(endPin.NodeID);

        const auto& outputs = endNode->OutputsPerPin[0];
        for (const auto& output : outputs)
        {
            if (output.NodeID == startNode->ID)
            {
                bReject = true;
                ShowLabel("x Connection between the states already exist!", ImColor(45, 32, 32, 180));
                ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                break;
            }
        }

        return bReject;
    }

    void AnimationStateMachineGraph::OnLinkCreated(const Link& link)
    {
        UIGraph::OnLinkCreated(link);

        const Pin* startPin = FindPin(link.StartPinID);
        const Pin* endPin = FindPin(link.EndPinID);
        EG_CORE_ASSERT(startPin && endPin);

        const Node* startNode = FindNode(startPin->NodeID);
        const Node* endNode = FindNode(endPin->NodeID);
        EG_CORE_ASSERT(startNode && endNode);

        if (startNode->Type == NodeType::StateMachineState
            && endNode->Type == NodeType::StateMachineState)
        {
            // TODO: Rename the transition graph names when a node is renamed
            auto& transitionGraphs = m_LinkTransitions[link.ID];
            transitionGraphs[0] = MakeRef<AnimationStateTransitionGraph>(m_Editor, "Transition: " + startNode->UserData + " to " + endNode->UserData);
            transitionGraphs[1] = MakeRef<AnimationStateTransitionGraph>(m_Editor, "Transition: " + endNode->UserData + " to " + startNode->UserData);
        }
    }

    void AnimationStateMachineGraph::OnLinkDeleted(const Link& link)
    {
        UIGraph::OnLinkDeleted(link);

        auto it = m_LinkTransitions.find(link.ID);
        if (it != m_LinkTransitions.end())
            m_LinkTransitions.erase(it);
    }

    std::vector<GraphSerializationData> AnimationStateMachineGraph::Serialize() const
    {
        auto data = UIGraph::Serialize();

        // Serialize transition graphs attached to links
        for (const auto& [linkID, graphs] : m_LinkTransitions)
        {
            for (const auto& graph : graphs)
            {
                auto serialized = graph->Serialize();
                for (auto& serial : serialized)
                    data.emplace_back(std::move(serial));
            }
        }

        return data;
    }

    void AnimationStateMachineGraph::Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data)
    {
        UIGraph::Deserialize(editorData, data);

        // Deserialize transition graphs attached to links
        for (const auto& [linkID, graphs] : m_LinkTransitions)
        {
            for (const auto& graph : graphs)
            {
                for (const auto& graphData : editorData.Graphs)
                {
                    if (graphData.Name == graph->GetName())
                    {
                        graph->Deserialize(editorData, graphData);
                        break;
                    }
                }
            }
        }
    }

    void AnimationStateMachineGraph::DrawLinks()
    {
        UIGraph::DrawLinks();

        auto textureID = UI::GetTextureID(m_ArrowTexture);
        ImGuiID id = (ImGuiID)m_ArrowTexture->GetGUID().GetHash();

        // Draws transition arrows
        uint32_t i = 0;
        for (auto& [_, link] : m_GraphData.Links)
        {
            const Pin* pin = FindPin(link.StartPinID);
            if (!pin)
                continue;

            const Node* node = FindNode(pin->NodeID);
            if (!node || node->Type != NodeType::StateMachineState)
                continue;

            auto edLink = m_GraphData.Editor->GetLink(link.ID);
            const glm::vec2 start = glm::vec2(edLink->m_Start.x, edLink->m_Start.y);
            const glm::vec2 end = glm::vec2(edLink->m_End.x, edLink->m_End.y);
            const glm::vec2 diff = glm::normalize(end - start);
            const float cosAngle = glm::dot(diff, glm::vec2(0.f, 1.f));
            if (!glm::isnan(cosAngle)) // Possible if nodes are too close (in which case link aren't rendered)
            {
                constexpr float halfPi = glm::pi<float>() * 0.5f;
                const float cosAngleAbs = glm::abs(cosAngle);
                const float rotation = halfPi * (1.f - cosAngle) * (start.x > end.x ? 1.f : -1.f);
                const float imageSize = 16.f;

                const float offset1 = 30.f;
                const float offset2 = offset1 + offset1 * 0.2f;

                const glm::vec2 middle = (start + end) * 0.5f;
                const glm::vec2 forwardPos = glm::vec2(middle.x - offset1, middle.y + (1.f - cosAngleAbs) * 5.f);

                // Forward Transition
                ImGui::SetCursorScreenPos(ImVec2{ forwardPos.x, forwardPos.y });
                if (UI::ImageButtonRotated(id + i * 2, textureID, { imageSize, imageSize }, rotation))
                {
                    auto it = m_LinkTransitions.find(link.ID);
                    if (it != m_LinkTransitions.end())
                    {
                        auto& transitionGraph = it->second[0];
                        m_Editor.AddGraph(transitionGraph);
                    }
                }

                // Backward Transition
                ImGui::SetCursorScreenPos(ImVec2{ forwardPos.x + offset2 * cosAngleAbs, forwardPos.y - 0.9f * offset2 * (1.f - cosAngleAbs) });
                if (UI::ImageButtonRotated(id + i * 2 + 1, textureID, { imageSize, imageSize }, rotation, ImVec2(1, 1), ImVec2(0, 0)))
                {
                    auto it = m_LinkTransitions.find(link.ID);
                    if (it != m_LinkTransitions.end())
                    {
                        auto& transitionGraph = it->second[1];
                        m_Editor.AddGraph(transitionGraph);
                    }
                }

                i++;
            }
        }
    }
}
