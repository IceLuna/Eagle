#include "egpch.h"
#include "UIAnimationStateMachineGraph.h"
#include "UIAnimationStateTransitionGraph.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"
#include "Eagle/Animation/AnimationStateGraph.h"
#include "Eagle/Animation/AnimationStateMachineGraph.h"
#include "Eagle/UI/UI.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"

#include <glm/ext/scalar_constants.hpp>

namespace Eagle
{
    static std::string GetTransitionNodeName(const std::string& from, const std::string& to)
    {
        return "Transition: " + from + " to " + to;
    }

    UIAnimationStateMachineGraph::UIAnimationStateMachineGraph(GraphEditor& editor, const std::string_view name)
		: UIGraph(editor, name)
	{
		m_ArrowTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/arrow.png");

        SetupInitialNodes();
        SetupNodeFactory();
	}

    void UIAnimationStateMachineGraph::OnNodeAdded(Node& node)
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
            node.Graph->SetName(node.UserData);
        }

        UIGraph::OnNodeAdded(node);

        if (node.Type == NodeType::StateMachineState)
            m_StateNodes.push_back(node.ID);
    }

    void UIAnimationStateMachineGraph::OnNodeDeleted(const Node& node)
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

    static Ref<GraphNode> ProcessStateNode(Node* stateNode, VariablesMap& outVariables)
    {
        if (!stateNode || stateNode->Type != NodeType::StateMachineState)
            return {};

        EG_CORE_ASSERT(stateNode->Graph);
        return stateNode->Graph->Compile(outVariables);
    }

    Ref<AnimationStateGraph> UIAnimationStateMachineGraph::CompileStateNode(Node* node, const Ref<AssetSkeletalMesh>& skeletalAsset, const Ref<AnimationStateMachineGraph>& stateMachine, VariablesMap& outVariables)
    {
        // Check if already was compiled
        auto it = m_CompiledNodes.find(node->ID);
        if (it != m_CompiledNodes.end())
            return it->second;
            
        Ref<AnimationStateGraph> state;
        auto compiledNode = ProcessStateNode(node, outVariables);
        if (compiledNode)
        {
            state = MakeRef<AnimationStateGraph>(skeletalAsset);
            state->SetResult(compiledNode);
            stateMachine->AddState(state);
        }

        m_CompiledNodes.emplace(node->ID, state);
        return state;
    }

    Ref<AnimationStateGraph> UIAnimationStateMachineGraph::Parse(Node* node, const Ref<AssetSkeletalMesh>& skeletalAsset, const Ref<AnimationStateMachineGraph>& stateMachine, bool bCloneVars, VariablesMap& outVariables)
    {
        if (node->GraphNode)
            node->GraphNode->ResetInputs();

        // We proccess Input and Outputs at the same time because they're the same in case of a state machine.
        // They're not considered to be inputs/outputs, but rather transition directions.
        // Meaning an execution can continue in any way.

        // Compile current node & and add to state machine
        Ref<AnimationStateGraph> compiledState = CompileStateNode(node, skeletalAsset, stateMachine, outVariables);

        // Input directions
        {
            const size_t baseInputsCount = node->InputsPerPin.size();
            for (size_t baseNodeInputIdx = 0; baseNodeInputIdx < baseInputsCount; ++baseNodeInputIdx)
            {
                for (auto& pinInput : node->InputsPerPin[baseNodeInputIdx])
                {
                    auto& input = pinInput.NodeID;
                    if (!input)
                        continue;

                    StatesConnection connection;
                    Node* connectedNode = FindNode(input);
                    EG_CORE_ASSERT(connectedNode);
                    if (connectedNode->Type != NodeType::StateMachineState)
                        continue;

                    // Check if already was compiled
                    auto it = m_CompiledNodes.find(connectedNode->ID);
                    if (it != m_CompiledNodes.end())
                        connection.ConnectedTo = it->second;
                    else
                        connection.ConnectedTo = Parse(connectedNode, skeletalAsset, stateMachine, bCloneVars, outVariables);

                    // Find and set a transition link
                    {
                        ed::PinId startPinID = connectedNode->OutputPins[pinInput.PinIndex].ID;
                        ed::PinId endPinID = node->InputPins[baseNodeInputIdx].ID;
                        auto it = std::find_if(m_LinkTransitions.begin(), m_LinkTransitions.end(), [this, startPinID, endPinID](auto val)
                        {
                            Link* link = FindLink(val.first);
                            if (!link)
                                return false;

                            return link->StartPinID == startPinID && link->EndPinID == endPinID;
                        });

                        if (it != m_LinkTransitions.end())
                        {
                            Ref<UIAnimationStateTransitionGraph> transitionGraph = it->second[1];
                            connection.Transition = MakeRef<AnimationGraph>(skeletalAsset);
                            connection.Transition->SetResult(transitionGraph->Compile(outVariables));
                            connection.Transition->SetVariables(outVariables);
                            // EG_CORE_INFO("{} is connected to {} via {}", node->GetName(), connectedNode->GetName(), transitionGraph->GetName());
                        }
                        else
                        {
                            EG_CORE_ASSERT(false); // Transition should always exist!
                        }
                    }
                    
                    compiledState->AddConnection(connection);
                }
            }
        }

        // Output directions
        {
            const size_t baseOutputsCount = node->OutputsPerPin.size();
            for (size_t baseNodeOutputIdx = 0; baseNodeOutputIdx < baseOutputsCount; ++baseNodeOutputIdx)
            {
                for (auto& pinOutput : node->OutputsPerPin[baseNodeOutputIdx])
                {
                    auto& output = pinOutput.NodeID;
                    if (!output)
                        continue;

                    StatesConnection connection;
                    Node* connectedNode = FindNode(output);
                    EG_CORE_ASSERT(connectedNode);
                    if (connectedNode->Type != NodeType::StateMachineState)
                        continue;

                    // Check if already was compiled
                    auto it = m_CompiledNodes.find(connectedNode->ID);
                    if (it != m_CompiledNodes.end())
                        connection.ConnectedTo = it->second;
                    else
                        connection.ConnectedTo = Parse(connectedNode, skeletalAsset, stateMachine, bCloneVars, outVariables);

                    // Find and set a transition link
                    {
                        ed::PinId startPinID = node->OutputPins[baseNodeOutputIdx].ID;
                        ed::PinId endPinID = connectedNode->InputPins[pinOutput.PinIndex].ID;
                        auto it = std::find_if(m_LinkTransitions.begin(), m_LinkTransitions.end(), [this, startPinID, endPinID](auto val)
                        {
                            Link* link = FindLink(val.first);
                            if (!link)
                                return false;

                            return link->StartPinID == startPinID && link->EndPinID == endPinID;
                        });

                        if (it != m_LinkTransitions.end())
                        {
                            Ref<UIAnimationStateTransitionGraph> transitionGraph = it->second[0];
                            connection.Transition = MakeRef<AnimationGraph>(skeletalAsset);
                            connection.Transition->SetResult(transitionGraph->Compile(outVariables));
                            connection.Transition->SetVariables(outVariables);

                            // EG_CORE_INFO("{} is connected to {} via {}", node->GetName(), connectedNode->GetName(), transitionGraph->GetName());
                        }
                        else
                        {
                            EG_CORE_ASSERT(false); // Transition should always exist!
                        }
                    }

                    compiledState->AddConnection(connection);
                }
            }
        }

        return compiledState;
    }

    Ref<GraphNode> UIAnimationStateMachineGraph::Compile_Internal(Node* node, VariablesMap& outUsedVars, std::unordered_set<UIGraph*> compiledGraphs)
    {
        ed::Detail::EditorContext* editorBefore = ed::GetCurrentEditor();
        ed::SetCurrentEditor(m_GraphData.Editor);

        auto stateMachine = MakeRef<AnimationStateMachineGraph>();
        const auto& graphAsset = ((AnimationGraphEditor&)m_Editor).GetGraphAsset();
        const auto& graph = graphAsset->GetGraph();
        const uint32_t stateMachineIndex = graph->AddStateMachine(stateMachine);

        EG_CORE_ASSERT(node->GraphNode);
        node->GraphNode->ResetInputs();

        if (auto entryNode = Cast<AnimationGraphStateMachineEntry>(node->GraphNode))
            entryNode->SetStateMachineIndex(stateMachineIndex);

        if (node->OutputsPerPin[0].size() > 0)
        {
            const auto& skeletalAsset = graph->GetSkeletalAsset();
            Node* connectedToEntry = FindNode(node->OutputsPerPin[0][0].NodeID);
            if (connectedToEntry)
            {
                auto compiled = Parse(connectedToEntry, skeletalAsset, stateMachine, true, outUsedVars);
                node->GraphNode->SetInput(compiled->GetResult(), 0);

                // Set varaibles map to newly created graphs
                for (auto& [_, graph] : m_CompiledNodes)
                    graph->SetVariables(outUsedVars);
            }
        }

        Ref<GraphNode> compiledNode = node->GraphNode;
        m_CompiledNodes.clear();

        ed::SetCurrentEditor(editorBefore);

        return compiledNode;
    }

    void UIAnimationStateMachineGraph::OnVariableRenamed(const std::string& varName, const std::string& newName)
    {
        UIGraph::OnVariableRenamed(varName, newName);

        for (auto& [_, transitionGraphs] : m_LinkTransitions)
        {
            transitionGraphs[0]->OnVariableRenamed(varName, newName);
            transitionGraphs[1]->OnVariableRenamed(varName, newName);
        }
    }

    void UIAnimationStateMachineGraph::SetupInitialNodes()
    {
        m_EntryNodeId = GraphNodeFactory::SpawnEntryStateNode(*this).ID;
    }

    void UIAnimationStateMachineGraph::SetupNodeFactory()
    {
        auto& category = m_NodeFactory["State Machine"];
        category["New State"] = &GraphNodeFactory::SpawnState;
        category["Comment"] = &GraphNodeFactory::SpawnComment;
    }

    bool UIAnimationStateMachineGraph::ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin)
    {
        // Default reject behavior
        if (UIGraph::ProcessNewLinkRejection(startPin, endPin))
            return true;

        // Check if nodes are already connected. If so, reject the link
        // Since it doesn't make sense because the existing link already has a back transition that can be used
        bool bReject = false;
        Node* startNode = FindNode(startPin.NodeID);
        Node* endNode = FindNode(endPin.NodeID);


        // Check if a user tries to connect nodes again but using other input-output pins
        {
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
        }

        // Check if a user tries to connect nodes again using same input-output pins
        {
            const auto& outputs = startNode->OutputsPerPin[0];
            for (const auto& output : outputs)
            {
                if (output.NodeID == endNode->ID)
                {
                    bReject = true;
                    ShowLabel("x Connection between the states already exist!", ImColor(45, 32, 32, 180));
                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                    break;
                }
            }
        }

        return bReject;
    }

    bool UIAnimationStateMachineGraph::RenameGraph(std::string graphName, const std::string& newName)
    {
        if (UIGraph::RenameGraph(graphName, newName) == false)
            return false;

        for (auto& [linkID, graphs] : m_LinkTransitions)
        {
            Link* link = FindLink(linkID);
            if (!link)
                continue;

            const Pin* startPin = FindPin(link->StartPinID);
            const Pin* endPin = FindPin(link->EndPinID);
            EG_CORE_ASSERT(startPin && endPin);

            const Node* startNode = FindNode(startPin->NodeID);
            const Node* endNode = FindNode(endPin->NodeID);
            EG_CORE_ASSERT(startNode && endNode);

            // Since the node was already rename by calling `UIGraph::RenameGraph()`, we check for `newName`
            if (startNode->GetName() == newName || endNode->GetName() == newName)
            {
                graphs[0]->SetName(GetTransitionNodeName(startNode->GetName(), endNode->GetName()));
                graphs[1]->SetName(GetTransitionNodeName(endNode->GetName(), startNode->GetName()));
            }
        }

        return true;
    }

    void UIAnimationStateMachineGraph::OnLinkCreated(const Link& link)
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
            auto& transitionGraphs = m_LinkTransitions[link.ID];
            transitionGraphs[0] = MakeRef<UIAnimationStateTransitionGraph>(m_Editor, GetTransitionNodeName(startNode->GetName(), endNode->GetName()));
            transitionGraphs[1] = MakeRef<UIAnimationStateTransitionGraph>(m_Editor, GetTransitionNodeName(endNode->GetName(), startNode->GetName()));
        }
    }

    void UIAnimationStateMachineGraph::OnLinkDeleted(const Link& link)
    {
        UIGraph::OnLinkDeleted(link);

        auto it = m_LinkTransitions.find(link.ID);
        if (it != m_LinkTransitions.end())
            m_LinkTransitions.erase(it);
    }

    GraphSerializationData UIAnimationStateMachineGraph::Serialize() const
    {
        auto data = UIGraph::Serialize();

        // Serialize transition graphs attached to links
        for (const auto& [linkID, graphs] : m_LinkTransitions)
        {
            for (const auto& graph : graphs)
            {
                auto serialized = graph->Serialize();
                data.Subgraphs.emplace_back(std::move(serialized));
            }
        }

        return data;
    }

    void UIAnimationStateMachineGraph::Deserialize_Internal(const GraphEditorSerializationData& editorData, const GraphSerializationData& data, std::vector<UIGraph*>& deserializedGraphs, std::vector<PoseCacheGetterDeserializationData>& poseCacheGetterData)
    {
        UIGraph::Deserialize_Internal(editorData, data, deserializedGraphs, poseCacheGetterData);

        // Deserialize transition graphs attached to links
        for (const auto& [linkID, graphs] : m_LinkTransitions)
        {
            for (const auto& graph : graphs)
            {
                for (const auto& graphData : data.Subgraphs)
                {
                    if (graphData.Name == graph->GetName())
                    {
                        graph->Deserialize_Internal(editorData, graphData, deserializedGraphs, poseCacheGetterData);
                        break;
                    }
                }
            }
        }
    }

    void UIAnimationStateMachineGraph::DrawLinks()
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
