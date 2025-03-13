#pragma once

namespace Eagle
{
    class UIGraph;
    struct Node;
    enum class PinType;

    typedef Node& (*CreateNodeFunc)(UIGraph& graph, const std::string_view name);
    using NodeFactoryMap = std::unordered_map<std::string, CreateNodeFunc>; // Key: Node name; Value: Node-creation function

	class GraphNodeFactory
	{
    public:
        static void FillCommonNodes(std::unordered_map<std::string, NodeFactoryMap>& factory);
        static void FillAnimationNodes(std::unordered_map<std::string, NodeFactoryMap>& factory);

        // Currently unused nodes
        static Node& SpawnInputActionNode(UIGraph& graph);
        static Node& SpawnBranchNode(UIGraph& graph);
        static Node& SpawnDoNNode(UIGraph& graph);
        static Node& SpawnOutputActionNode(UIGraph& graph);
        static Node& SpawnPrintStringNode(UIGraph& graph);
        static Node& SpawnMessageNode(UIGraph& graph);
        static Node& SpawnSetTimerNode(UIGraph& graph);
        static Node& SpawnWeirdNode(UIGraph& graph);
        static Node& SpawnTraceByChannelNode(UIGraph& graph);
        static Node& SpawnTreeTaskNode(UIGraph& graph);
        static Node& SpawnTreeTask2Node(UIGraph& graph);
        static Node& SpawnHoudiniTransformNode(UIGraph& graph);
        static Node& SpawnHoudiniGroupNode(UIGraph& graph);
        static Node& SpawnEntryNode(UIGraph& graph);
        static Node& SpawnIntToStringNode(UIGraph& graph);

        // Outputs
        static Node& SpawnOutputPoseNode(UIGraph& graph);
        static Node& SpawnStateOutputPoseNode(UIGraph& graph);
        static Node& SpawnOutputTransitionNode(UIGraph& graph);

        // Animations
        static Node& SpawnAnimBlendNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnAnimCalculateAdditiveNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnAnimAdditiveBlendNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnAnimClipNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnSelectPoseByBoolNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnAnimFilterBones(UIGraph& graph, const std::string_view name);

        // Logical
        static Node& SpawnAndNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnOrNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnXorNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnNotNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnLessNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnLessEqNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnGreaterNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnGreaterEqNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnEqualNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnNotEqualNode(UIGraph& graph, const std::string_view name);

        // Math
        static Node& SpawnAddNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnSubNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnMulNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnDivNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnSqrtNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnAbsNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnSinNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnCosNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnASinNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnACosNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnToRadNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnToDegNode(UIGraph& graph, const std::string_view name);
        static Node& SpawnMapRangeNode(UIGraph& graph, const std::string_view name);
        
        // State Machine
        static Node& SpawnStateMachine(UIGraph& graph, const std::string_view name);
        static Node& SpawnState(UIGraph& graph, const std::string_view name); // State of a state machine
        static Node& SpawnEntryStateNode(UIGraph& graph);

        // Other
        static Node& SpawnComment(UIGraph& graph, const std::string_view name);
        static Node& SpawnVarNode(UIGraph& graph, const std::string& name, const PinType& type);
    };
}
