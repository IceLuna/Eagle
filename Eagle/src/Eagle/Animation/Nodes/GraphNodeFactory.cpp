#include "egpch.h"
#include "GraphNodeFactory.h"
#include "Eagle/UI/Graphs/UIGraph.h"
#include "Eagle/UI/Graphs/UIAnimationStateMachineGraph.h"
#include "Eagle/UI/Graphs/UIAnimationStateGraph.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"

namespace Eagle
{
    static const char* s_FrozenTransitionHelpMsg = "If set to false, frozen transition will be used: clip A is frozen while clip B gradually takes over the movement.\
This kind of transitional blend works well when the two clips/poses are unrelated and smooth transition looks unnatural";

    void GraphNodeFactory::FillCommonNodes(std::unordered_map<std::string, NodeFactoryMap>& factory)
    {
        // Math catergory
        {
            auto& mathCategory = factory["Math"];
            mathCategory["Add"] = &GraphNodeFactory::SpawnAddNode;
            mathCategory["Subtract"] = &GraphNodeFactory::SpawnSubNode;
            mathCategory["Multiply"] = &GraphNodeFactory::SpawnMulNode;
            mathCategory["Divide"] = &GraphNodeFactory::SpawnDivNode;
            mathCategory["Sqrt"] = &GraphNodeFactory::SpawnSqrtNode;
            mathCategory["Abs"] = &GraphNodeFactory::SpawnAbsNode;
            mathCategory["Sin (rad)"] = &GraphNodeFactory::SpawnSinNode;
            mathCategory["Cos (rad)"] = &GraphNodeFactory::SpawnCosNode;
            mathCategory["ASin"] = &GraphNodeFactory::SpawnASinNode;
            mathCategory["ACos"] = &GraphNodeFactory::SpawnACosNode;
            mathCategory["To Radians"] = &GraphNodeFactory::SpawnToRadNode;
            mathCategory["To Degrees"] = &GraphNodeFactory::SpawnToDegNode;
            mathCategory["Map Range"] = &GraphNodeFactory::SpawnMapRangeNode;
            mathCategory["Euler (rad) to Quat"] = &GraphNodeFactory::SpawnEulerToQuatNode;
        }

        // Logical catergory
        {
            auto& logicalCategory = factory["Logical"];
            logicalCategory["AND"] = &GraphNodeFactory::SpawnAndNode;
            logicalCategory["OR"] = &GraphNodeFactory::SpawnOrNode;

            logicalCategory["XOR"] = &GraphNodeFactory::SpawnXorNode;
            logicalCategory["NOT"] = &GraphNodeFactory::SpawnNotNode;
            logicalCategory["<"] = &GraphNodeFactory::SpawnLessNode;
            logicalCategory["<="] = &GraphNodeFactory::SpawnLessEqNode;
            logicalCategory[">"] = &GraphNodeFactory::SpawnGreaterNode;
            logicalCategory[">="] = &GraphNodeFactory::SpawnGreaterEqNode;
            logicalCategory["=="] = &GraphNodeFactory::SpawnEqualNode;
            logicalCategory["!="] = &GraphNodeFactory::SpawnNotEqualNode;
        }

        // Other
        {
            auto& otherCategory = factory["Other"];
            otherCategory["Comment"] = &GraphNodeFactory::SpawnComment;
        }
    }

    void GraphNodeFactory::FillAnimationNodes(std::unordered_map<std::string, NodeFactoryMap>& factory)
    {
        auto& animationsCategory = factory["Animations"];
        animationsCategory["Animation Clip"] = &GraphNodeFactory::SpawnAnimClipNode;
        animationsCategory["Blend Poses"] = &GraphNodeFactory::SpawnAnimBlendNode;
        animationsCategory["Additive Blend"] = &GraphNodeFactory::SpawnAnimAdditiveBlendNode;
        animationsCategory["Calculate Additive"] = &GraphNodeFactory::SpawnAnimCalculateAdditiveNode;
        animationsCategory["Select Pose by Bool"] = &GraphNodeFactory::SpawnSelectPoseByBoolNode;
        animationsCategory["Filter Bones"] = &GraphNodeFactory::SpawnAnimFilterBones;
        animationsCategory["Transform Bone"] = &GraphNodeFactory::SpawnAnimTransformBone;
    }

    Node& GraphNodeFactory::SpawnInputActionNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("InputAction Fire", ImColor(255, 128, 128));
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Delegate);
        node.OutputPins.emplace_back(graph.GetNextId(), "Pressed", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "Released", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBranchNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Branch");
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "Condition", PinType::Bool);
        node.OutputPins.emplace_back(graph.GetNextId(), "True", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "False", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnDoNNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Do N");
        node.InputPins.emplace_back(graph.GetNextId(), "Enter", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "N", PinType::Int);
        node.InputPins.emplace_back(graph.GetNextId(), "Reset", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "Exit", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "Counter", PinType::Int);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOutputActionNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("OutputAction");
        node.InputPins.emplace_back(graph.GetNextId(), "Sample", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextId(), "Condition", PinType::Bool);
        node.InputPins.emplace_back(graph.GetNextId(), "Event", PinType::Delegate);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnPrintStringNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Print String");
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "In String", PinType::String);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnMessageNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("", ImColor(128, 195, 248));
        node.Type = NodeType::Simple;
        node.OutputPins.emplace_back(graph.GetNextId(), "Message", PinType::String);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnSetTimerNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Set Timer", ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "Object", PinType::Object);
        node.InputPins.emplace_back(graph.GetNextId(), "Function Name", PinType::Function);
        node.InputPins.emplace_back(graph.GetNextId(), "Time", PinType::Float);
        node.InputPins.emplace_back(graph.GetNextId(), "Looping", PinType::Bool);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnWeirdNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("o.O", ImColor(128, 195, 248));
        node.Type = NodeType::Simple;
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnTraceByChannelNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Single Line Trace by Channel", ImColor(255, 128, 64));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "Start", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "End", PinType::Int);
        node.InputPins.emplace_back(graph.GetNextId(), "Trace Channel", PinType::Float);
        node.InputPins.emplace_back(graph.GetNextId(), "Trace Complex", PinType::Bool);
        node.InputPins.emplace_back(graph.GetNextId(), "Actors to Ignore", PinType::Int);
        node.InputPins.emplace_back(graph.GetNextId(), "Draw Debug Type", PinType::Bool);
        node.InputPins.emplace_back(graph.GetNextId(), "Ignore Self", PinType::Bool);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "Out Hit", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextId(), "Return Value", PinType::Bool);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnTreeTaskNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Move To");
        node.Type = NodeType::Tree;
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnTreeTask2Node(UIGraph& graph)
    {
        auto& node = graph.AddNode("Random Wait");
        node.Type = NodeType::Tree;
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnHoudiniTransformNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Transform");
        node.Type = NodeType::Houdini;
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnHoudiniGroupNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Group");
        node.Type = NodeType::Houdini;
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnEntryNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Entry", ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnIntToStringNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Int", ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Int);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::String);
        node.Type = NodeType::Blueprint;

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnVarNode(UIGraph& graph, const std::string& name, const PinType& type)
    {
        auto& node = graph.AddNode(name.c_str(), ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextId(), "", type);
        node.Type = NodeType::Variable;

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOutputPoseNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();
        
        auto& node = graph.AddNode("Output Pose", ImColor(128, 195, 248), false);
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Pose);

        node.GraphNode = MakeRef<AnimationGraphNodeOutput>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnStateOutputPoseNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();
        
        auto& node = graph.AddNode("Output Pose", ImColor(128, 195, 248), false);
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Pose);

        node.GraphNode = MakeRef<AnimationGraphNodeStateOutput>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOutputTransitionNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();
        
        auto& node = graph.AddNode("Transition", ImColor(128, 195, 248), false);
        node.InputPins.emplace_back(graph.GetNextId(), "Should Transition", PinType::Bool, MakeRef<GraphVariableBool>(false));
        node.InputPins.emplace_back(graph.GetNextId(), "Transition Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f));
        node.InputPins.emplace_back(graph.GetNextId(), "Smooth Transition", PinType::Bool, MakeRef<GraphVariableBool>(true), s_FrozenTransitionHelpMsg);

        node.GraphNode = MakeRef<AnimationGraphNodeTransitionOutput>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnEntryStateNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode("Entry", ImColor(128, 195, 248), false);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Flow);
        
        node.GraphNode = MakeRef<AnimationGraphStateMachineEntry>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimBlendNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Pose 1", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Pose 2", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Blend weight", PinType::Float, MakeRef<GraphVariableFloat>(0.5f));

        node.OutputPins.emplace_back(graph.GetNextId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeBlend>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimCalculateAdditiveNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Reference pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Source pose", PinType::Pose);

        node.OutputPins.emplace_back(graph.GetNextId(), "Additive pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeCalculateAdditive>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimAdditiveBlendNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Target pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Additive pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Blend weight", PinType::Float, MakeRef<GraphVariableFloat>(1.f));

        node.OutputPins.emplace_back(graph.GetNextId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeAdditiveBlend>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimClipNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Animation", PinType::Object, MakeRef<GraphVariableAnimation>());
        node.InputPins.emplace_back(graph.GetNextId(), "Playback speed", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.InputPins.emplace_back(graph.GetNextId(), "Loop", PinType::Bool, MakeRef<GraphVariableBool>(true));

        node.OutputPins.emplace_back(graph.GetNextId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeClip>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnSelectPoseByBoolNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "False pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "True pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Condition", PinType::Bool, MakeRef<GraphVariableBool>(true));

        node.OutputPins.emplace_back(graph.GetNextId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeSelectPoseByBool>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimFilterBones(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Bone Name", PinType::String, MakeRef<GraphVariableString>(), "The node will filter-out the bones that are not related to the specified bone");

        node.OutputPins.emplace_back(graph.GetNextId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeFilterBones>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimTransformBone(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextId(), "Bone Name", PinType::String, MakeRef<GraphVariableString>());
        node.InputPins.emplace_back(graph.GetNextId(), "Rotation", PinType::Vec4, MakeRef<GraphVariableVec4>(glm::vec4(0, 0, 0, 1)));

        node.OutputPins.emplace_back(graph.GetNextId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeTransformBone>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAndNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeAnd>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOrNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeOr>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnXorNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeXor>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnNotNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeNot>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnLessNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeLess>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnLessEqNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeLessEqual>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnGreaterNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeGreater>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnGreaterEqNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeGreaterEqual>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnEqualNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeEqual>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnNotEqualNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeNotEqual>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAddNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeAdd>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnSubNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeSub>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnMulNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeMul>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnDivNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeDiv>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnSinNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeSin>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnSqrtNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeSqrt>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAbsNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeAbs>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnCosNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeCos>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnASinNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeASin>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnACosNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeACos>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnToRadNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeToRad>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnToDegNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeToDeg>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnMapRangeNode(UIGraph& graph, const std::string_view name)
    {
        const auto & graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "Value", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "Min A", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "Max A", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "Min B", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "Max B", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeMapRange>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnEulerToQuatNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextId(), "X", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "Y", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextId(), "Z", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Vec4);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeEulerToQuat>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnComment(UIGraph& graph, const std::string_view name)
    {
        auto& node = graph.AddNode(name);
        node.Type = NodeType::Comment;
        node.Size = ImVec2(300, 200);
        node.UserData = "Message";

        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnStateMachine(UIGraph& graph, const std::string_view name)
    {
        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::Pose);
        node.Type = NodeType::StateMachine;
        node.UserData = name;

        node.Graph = MakeRef<UIAnimationStateMachineGraph>(graph.GetEditor(), name);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnState(UIGraph& graph, const std::string_view name)
    {
        auto& node = graph.AddNode(name);
        node.Type = NodeType::StateMachineState;
        node.InputPins.emplace_back(graph.GetNextId(), "", PinType::StateFlow);
        node.OutputPins.emplace_back(graph.GetNextId(), "", PinType::StateFlow);
        node.UserData = name;

        node.Graph = MakeRef<UIAnimationStateGraph>(graph.GetEditor(), name);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }
}
