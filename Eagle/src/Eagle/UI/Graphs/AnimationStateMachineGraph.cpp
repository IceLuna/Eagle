#include "egpch.h"
#include "AnimationStateMachineGraph.h"

#include "Eagle/Core/Application.h"
#include "Eagle/UI/UI.h"

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

                    if (graphNode->Name == name)
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
                if (UI::ImageButtonRotated(id + i * 2, textureID, {imageSize, imageSize}, rotation))
                    EG_CORE_INFO("Forward {}", i);

                // Backward Transition
                ImGui::SetCursorScreenPos(ImVec2{ forwardPos.x + offset2 * cosAngleAbs, forwardPos.y - 0.9f * offset2 * (1.f - cosAngleAbs) });
                if (UI::ImageButtonRotated(id + i * 2 + 1, textureID, { imageSize, imageSize }, rotation, ImVec2(1, 1), ImVec2(0, 0)))
                    EG_CORE_INFO("Backward {}", i);

                i++;
            }
        }
    }
}
