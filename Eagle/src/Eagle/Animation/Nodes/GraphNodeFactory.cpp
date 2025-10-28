#include "egpch.h"
#include "GraphNodeFactory.h"
#include "Eagle/UI/Graphs/Animations/UIAnimationStateMachineGraph.h"
#include "Eagle/UI/Graphs/Animations/UIAnimationStateGraph.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Animation/Nodes/AnimationNodes.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"
#include "Eagle/AI/BehaviorGraph.h"

namespace Eagle
{
    static const char* s_FrozenTransitionHelpMsg = "If set to false, frozen transition will be used: clip A is frozen while clip B gradually takes over the movement.\
This kind of transitional blend works well when the two clips/poses are unrelated and smooth transition looks unnatural";
    static const char* s_AutoTransitionHelpMsg = "When enabled, it'll auto transition. Transition will start when `Transition Time` seconds are left to play. Transition time will be whatever time is left to play";

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
            logicalCategory["Is AnimVar valid"] = &GraphNodeFactory::SpawnAnimVarIsValidNode;
        }

        // Other
        {
            auto& otherCategory = factory["Other"];
            otherCategory["Comment"] = &GraphNodeFactory::SpawnComment;
            otherCategory["Int to Float"] = &GraphNodeFactory::SpawnIntToFloat;
        }
    }

    void GraphNodeFactory::FillAnimationNodes(std::unordered_map<std::string, NodeFactoryMap>& factory)
    {
        auto& animationsCategory = factory["Animations"];
        animationsCategory["Animation Clip"] = &GraphNodeFactory::SpawnAnimClipNode;
        animationsCategory["Blend Poses"] = &GraphNodeFactory::SpawnAnimBlendNode;
        animationsCategory["Additive Blend"] = &GraphNodeFactory::SpawnAnimAdditiveBlendNode;
        animationsCategory["Calculate Additive"] = &GraphNodeFactory::SpawnAnimCalculateAdditiveNode;
        animationsCategory["Blend Pose by Bool"] = &GraphNodeFactory::SpawnBlendPoseByBoolNode;
        animationsCategory["Blend Pose by Int"] = &GraphNodeFactory::SpawnBlendPoseByIntNode;
        animationsCategory["Filter Bones"] = &GraphNodeFactory::SpawnAnimFilterBones;
        animationsCategory["Transform Bone"] = &GraphNodeFactory::SpawnAnimTransformBone;
        animationsCategory["Cache Pose"] = &GraphNodeFactory::SpawnCachePoseNode;
    }

    Node& GraphNodeFactory::SpawnInputActionNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("InputAction Fire", ImColor(255, 128, 128));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Delegate);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Pressed", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Released", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBranchNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Branch");
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Condition", PinType::Bool);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "True", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "False", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnDoNNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Do N");
        node.InputPins.emplace_back(graph.GetNextPinId(), "Enter", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "N", PinType::Int);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Reset", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Exit", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Counter", PinType::Int);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOutputActionNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("OutputAction");
        node.InputPins.emplace_back(graph.GetNextPinId(), "Sample", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Condition", PinType::Bool);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Event", PinType::Delegate);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnPrintStringNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Print String");
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "In String", PinType::String);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnMessageNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("", ImColor(128, 195, 248));
        node.Type = NodeType::Simple;
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Message", PinType::String);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnSetTimerNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Set Timer", ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Object", PinType::Object);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Function Name", PinType::Function);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Time", PinType::Float);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Looping", PinType::Bool);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnWeirdNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("o.O", ImColor(128, 195, 248));
        node.Type = NodeType::Simple;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnTraceByChannelNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Single Line Trace by Channel", ImColor(255, 128, 64));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Start", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "End", PinType::Int);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Trace Channel", PinType::Float);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Trace Complex", PinType::Bool);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Actors to Ignore", PinType::Int);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Draw Debug Type", PinType::Bool);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Ignore Self", PinType::Bool);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Out Hit", PinType::Float);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Return Value", PinType::Bool);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnTreeTaskNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Move To");
        node.Type = NodeType::Tree;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnTreeTask2Node(UIGraph& graph)
    {
        auto& node = graph.AddNode("Random Wait");
        node.Type = NodeType::Tree;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnHoudiniTransformNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Transform");
        node.Type = NodeType::Houdini;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnHoudiniGroupNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Group");
        node.Type = NodeType::Houdini;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnEntryNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Entry", ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnIntToStringNode(UIGraph& graph)
    {
        auto& node = graph.AddNode("Int", ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Int);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::String);
        node.Type = NodeType::Blueprint;

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnVarNode(UIGraph& graph, const std::string& name, const PinType& type)
    {
        auto& node = graph.AddNode(name.c_str(), ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", type);
        node.Type = NodeType::Variable;

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnIntToFloat(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Int);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeIntToFloat>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnCachePoseGetterNode(UIGraph& graph, const Node* cache)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(cache ? cache->GetName() : "Unknown", ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "Cached", PinType::Pose);

        node.Type = NodeType::PoseCacheGetter;
        if (cache)
        {
            node.CachedNode.Owner = cache->Owner;
            node.CachedNode.NodeID = cache->ID;
        }

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOutputPoseNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();
        
        auto& node = graph.AddNode("Output Pose", ImColor(128, 195, 248), false);
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Pose);

        node.GraphNode = MakeRef<AnimationGraphNodeOutput>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnStateOutputPoseNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();
        
        auto& node = graph.AddNode("Output Pose", ImColor(128, 195, 248), false);
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Pose);

        node.GraphNode = MakeRef<AnimationGraphNodeStateOutput>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnOutputTransitionNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();
        
        auto& node = graph.AddNode("Transition", ImColor(128, 195, 248), false);
        const ed::PinId shouldTransitionPinID = node.InputPins.emplace_back(graph.GetNextPinId(), "Should Transition", PinType::Bool, MakeRef<GraphVariableBool>(false)).ID;
        node.InputPins.emplace_back(graph.GetNextPinId(), "Transition Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Smooth Transition", PinType::Bool, MakeRef<GraphVariableBool>(true), s_FrozenTransitionHelpMsg);
        auto& pin = node.InputPins.emplace_back(graph.GetNextPinId(), "Auto-Transition", PinType::Bool, MakeRef<GraphVariableBool>(false), s_AutoTransitionHelpMsg);
        pin.DisableInUIWhenPinIndexIsUsed = shouldTransitionPinID;

        node.GraphNode = MakeRef<AnimationGraphNodeTransitionOutput>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimationEntryStateNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode("Entry", ImColor(128, 195, 248), false);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);
        
        node.GraphNode = MakeRef<AnimationGraphStateMachineEntry>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimBlendNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose 1", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose 2", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Blend weight", PinType::Float, MakeRef<GraphVariableFloat>(0.5f));

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "Reference pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Source pose", PinType::Pose);

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Additive pose", PinType::Pose);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "Target pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Additive pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Blend weight", PinType::Float, MakeRef<GraphVariableFloat>(1.f));

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "Animation", PinType::Object, MakeRef<GraphVariableAnimation>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Playback speed", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Loop", PinType::Bool, MakeRef<GraphVariableBool>(true));

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeClip>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBlendPoseByBoolNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Condition", PinType::Bool, MakeRef<GraphVariableBool>(true));
        node.InputPins.emplace_back(graph.GetNextPinId(), "False pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "False Blend Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f), "Used to control how long it will take to blend into the pose");
        node.InputPins.emplace_back(graph.GetNextPinId(), "True pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "True Blend Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f), "Used to control how long it will take to blend into the pose");

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeBlendPoseByBool>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBlendPoseByIntNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Active Pose index", PinType::Int, MakeRef<GraphVariableInt>(0));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose 0", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose 0 Blend Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f), "Used to control how long it will take to blend into the pose");
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose 1", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose 1 Blend Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f), "Used to control how long it will take to blend into the pose");

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.SetAddPinsCallback([](Node& node)
        {
            UIGraph& graph = *node.Owner;

            const uint32_t poseIndex = ((uint32_t)node.InputPins.size() - 1) / 2;
            const std::string poseName = "Pose " + std::to_string(poseIndex);
            node.InputPins.emplace_back(graph.GetNextPinId(), poseName, PinType::Pose);
            node.InputPins.emplace_back(graph.GetNextPinId(), poseName + " Blend Time", PinType::Float, MakeRef<GraphVariableFloat>(0.1f), "Used to control how long it will take to blend into the pose");
            node.GraphNode->AddInput();
            node.GraphNode->AddInput();
            
            graph.BuildNode(node);
        });

        node.SetRemovePinsCallback([](Node& node)
        {
            UIGraph& graph = *node.Owner;

            node.InputPins.pop_back();
            node.InputPins.pop_back();
            node.GraphNode->PopInput();
            node.GraphNode->PopInput();

            graph.BuildNode(node);
        });

        node.SetCanRemovePinsCallback([](const Node& node)
        {
            const uint32_t poseIndex = ((uint32_t)node.InputPins.size() - 1) / 2;
            return poseIndex > 2; // Can't have less than two poses
        });

        node.GraphNode = MakeRef<AnimationGraphNodeBlendPoseByInt>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimFilterBones(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Bone Name", PinType::String, MakeRef<GraphVariableString>(), "The node will filter-out the bones that are not related to the specified bone");
        node.InputPins.emplace_back(graph.GetNextPinId(), "Ignore parent location", PinType::Bool, MakeRef<GraphVariableBool>(false), "Parent location of a specified bone will be ignored");
        node.InputPins.emplace_back(graph.GetNextPinId(), "Ignore parent rotation", PinType::Bool, MakeRef<GraphVariableBool>(true), "Parent rotation of a specified bone will be ignored");
        node.InputPins.emplace_back(graph.GetNextPinId(), "Ignore parent scale", PinType::Bool, MakeRef<GraphVariableBool>(true), "Parent scale of a specified bone will be ignored");

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose", PinType::Pose);
        node.InputPins.emplace_back(graph.GetNextPinId(), "Bone Name", PinType::String, MakeRef<GraphVariableString>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Rotation", PinType::Vec4, MakeRef<GraphVariableVec4>(glm::vec4(0, 0, 0, 1)));

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
        node.Type = NodeType::Blueprint;

        node.GraphNode = MakeRef<AnimationGraphNodeTransformBone>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnCachePoseNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Pose", PinType::Pose);

        node.Type = NodeType::PoseCache;

        node.GraphNode = MakeRef<AnimationGraphNodeCachePose>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBlendSpaceNode(UIGraph& graph, const std::string_view name, const Ref<AssetAnimationBlendSpace>& bs)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "X", PinType::Float, MakeRef<GraphVariableFloat>(0.f));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Y", PinType::Float, MakeRef<GraphVariableFloat>(0.f));

        node.OutputPins.emplace_back(graph.GetNextPinId(), "Output pose", PinType::Pose);
        node.Type = NodeType::BlendSpace;

        node.GraphNode = MakeRef<AnimationGraphNodeBlendSpace>(graphAsset->GetGraph(), bs);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAndNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool, MakeRef<GraphVariableBool>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeNotEqual>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimVarIsValidNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "Animation", PinType::Object, MakeRef<GraphVariableAnimation>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Bool);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeAnimVarIsValid>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAddNode(UIGraph& graph, const std::string_view name)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>(1.f));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "Value", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Min A", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Max A", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Min B", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Max B", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Float);
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
        node.InputPins.emplace_back(graph.GetNextPinId(), "X", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Y", PinType::Float, MakeRef<GraphVariableFloat>());
        node.InputPins.emplace_back(graph.GetNextPinId(), "Z", PinType::Float, MakeRef<GraphVariableFloat>());
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Vec4);
        node.Type = NodeType::Simple;

        node.GraphNode = MakeRef<AnimationGraphNodeEulerToQuat>(graphAsset->GetGraph());

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBehaviorTaskNode(UIGraph& graph, const AIBehaviorNode& data)
    {
        auto& node = graph.AddNode(data.Data.ClassData.UIName);
        node.Type = NodeType::BehaviorTask;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::StateFlow);
        node.Color = ImColor(128, 128, 128, 200);
        node.BehaviorNodeData = data;
        node.BehaviorNodeData.Data.ID = GUID{}; // Generate a new ID for it

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBehaviorCompositeNode(UIGraph& graph, const AIBehaviorNode& data)
    {
        auto& node = graph.AddNode(data.Data.ClassData.UIName);
        node.Type = NodeType::BehaviorComposite;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::StateFlow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::StateFlow);
        node.Color = ImColor(128, 128, 128, 200);
        node.BehaviorNodeData = data;
        node.BehaviorNodeData.Data.ID = GUID{}; // Generate a new ID for it

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnBehaviorRootNode(UIGraph& graph)
    {
        const auto& graphAsset = ((AnimationGraphEditor&)graph.GetEditor()).GetGraphAsset();

        auto& node = graph.AddNode("Root", ImColor(128, 195, 248), false);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Flow);

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

    Node& GraphNodeFactory::SpawnAnimationStateMachine(UIGraph& graph, const std::string_view name)
    {
        auto& node = graph.AddNode(name, ImColor(128, 195, 248));
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::Pose);
        node.Type = NodeType::StateMachine;
        node.UserData = name;

        node.Graph = MakeRef<UIAnimationStateMachineGraph>(graph.GetEditor(), name);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }

    Node& GraphNodeFactory::SpawnAnimationState(UIGraph& graph, const std::string_view name)
    {
        auto& node = graph.AddNode(name);
        node.Type = NodeType::StateMachineState;
        node.InputPins.emplace_back(graph.GetNextPinId(), "", PinType::StateFlow);
        node.OutputPins.emplace_back(graph.GetNextPinId(), "", PinType::StateFlow);
        node.UserData = name;
        node.Color = ImColor(128, 128, 128, 200);

        node.Graph = MakeRef<UIAnimationStateGraph>(graph.GetEditor(), name);

        graph.BuildNode(node);
        graph.OnNodeAdded(node);

        return node;
    }
}
