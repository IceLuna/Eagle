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

    void AnimationStateMachineGraph::SetupInitialNodes()
    {
        // TODO: Fix deserialization of state machines
        m_EntryNodeId = GraphNodeFactory::SpawnEntryStateNode(*this).ID;
    }

    void AnimationStateMachineGraph::SetupNodeFactory()
    {
        // Animations category
        {
            auto& animationsCategory = m_NodeFactory["Animations"];
            animationsCategory["Animation Clip"] = &GraphNodeFactory::SpawnAnimClipNode;
            animationsCategory["Blend Poses"] = &GraphNodeFactory::SpawnAnimBlendNode;
            animationsCategory["Additive Blend"] = &GraphNodeFactory::SpawnAnimAdditiveBlendNode;
            animationsCategory["Calculate Additive"] = &GraphNodeFactory::SpawnAnimCalculateAdditiveNode;
            animationsCategory["Select Pose by Bool"] = &GraphNodeFactory::SpawnSelectPoseByBoolNode;
        }

        UIGraph::SetupNodeFactory();

        // Other
        {
            auto& otherCategory = m_NodeFactory["Other"];
            otherCategory["New State"] = &GraphNodeFactory::SpawnState;
        }
    }

    void AnimationStateMachineGraph::DrawLinks()
    {
        UIGraph::DrawLinks();

        // Draws transition arrows
        for (auto& link : m_GraphData.Links)
        {
            const Pin* pin = FindPin(link.StartPinID);
            if (!pin)
                continue;

            const Node* node = FindNode(pin->NodeID);
            if (!node || node->Type != NodeType::Tree)
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
                if (UI::ImageButtonRotated(m_ArrowTexture, { imageSize, imageSize }, rotation))
                    EG_CORE_INFO("Forward");

                // Backward Transition
                ImGui::SetCursorScreenPos(ImVec2{ forwardPos.x + offset2 * cosAngleAbs, forwardPos.y - 0.9f * offset2 * (1.f - cosAngleAbs) });
                if (UI::ImageButtonRotated(m_ArrowTexture, { imageSize, imageSize }, rotation, ImVec2(1, 1), ImVec2(0, 0)))
                    EG_CORE_INFO("Backward");
            }
        }
    }
}
