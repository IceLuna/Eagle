#pragma once

#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/UI/Nodes/GraphNodeFactory.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>
#include <examples/blueprints-example/utilities/builders.h>
#include <examples/blueprints-example/utilities/widgets.h>

namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;

namespace Eagle
{
    enum class PinType
    {
        Flow,
        Bool,
        Int,
        Float,
        String,
        Object,
        Pose,
        Function,
        Delegate,
    };

    enum class PinKind
    {
        Output,
        Input
    };

    enum class NodeType
    {
        Blueprint,
        Simple,
        Variable,
        StateMachine,
        Tree,
        Comment,
        Houdini
    };

    struct Node;
    struct Pin
    {
        ed::PinId   ID;
        ed::NodeId NodeID;
        std::string Name;
        PinType     Type;
        PinKind     Kind;
        uint32_t Index; // Index inside `Node::InputPins` or `Node::OutputPins`
        Ref<GraphVariable> DefaultValue;

        Pin(int id, const char* name, PinType type, const Ref<GraphVariable>& defaultValue = nullptr) :
            ID(id), NodeID(), Name(name), Type(type), Kind(PinKind::Input), Index(0), DefaultValue(defaultValue)
        {
        }
    };

    struct OutputConnectionData
    {
        ed::NodeId NodeID;
        uint32_t PinIndex; // The pin index it's connected to (first pin, or second pin, etc...)
    };

    class AnimationGraphNode;
    struct Node
    {
        ed::NodeId ID;
        std::string Name;
        std::vector<Pin> InputPins;
        std::vector<Pin> OutputPins;
        ImColor Color;
        NodeType Type;
        ImVec2 Size;
        Ref<AnimationGraphNode> GraphNode; // Used if a node is a function (for example, addition)
        Ref<UIGraph> Graph; // Used if a node is a graph (for example, state machine graph)

        std::vector<ed::NodeId> Inputs;
        std::vector<std::vector<OutputConnectionData>> OutputsPerPin; // One pin-output can be used as an input for multiple nodes.

        std::string UserData = "Message";
        bool bDeletable = true;
        bool bEditing = false; // Can be used to indicate that it's in "editing" state (for example, it'll be `true` while renaming a node)

        Node(int id, const std::string_view name, ImColor color = ImColor(255, 255, 255), bool bDeletable = true) :
            ID(id), Name(name), Color(color), bDeletable(bDeletable), Type(NodeType::Blueprint), Size(0, 0)
        {
        }
    };

    struct Link
    {
        ed::LinkId ID;

        ed::PinId StartPinID;
        ed::PinId EndPinID;

        ImColor Color;

        Link(ed::LinkId id, ed::PinId startPinId, ed::PinId endPinId) :
            ID(id), StartPinID(startPinId), EndPinID(endPinId), Color(255, 255, 255)
        {
        }
    };

    struct NodeIdLess
    {
        bool operator()(const ed::NodeId& lhs, const ed::NodeId& rhs) const
        {
            return lhs.AsPointer() < rhs.AsPointer();
        }
    };

    struct GraphData
    {
        ax::NodeEditor::Detail::EditorContext* Editor = nullptr;
        std::string Name;

        int NextId = 1;
        const int PinIconSize = 24;

        std::vector<Node>    Nodes; // TODO: Improve by replacing with map?
        std::vector<ax::NodeEditor::NodeId> NodesPendingDeletion;
        std::vector<Link>    Links;
        std::map<ed::NodeId, float, NodeIdLess> NodeTouchTime;
    };

    static ImColor GetIconColor(PinType type)
    {
        switch (type)
        {
        case PinType::Flow:     return ImColor(255, 255, 255);
        case PinType::Bool:     return ImColor(220, 48, 48);
        case PinType::Int:      return ImColor(68, 201, 156);
        case PinType::Float:    return ImColor(147, 226, 74);
        case PinType::String:   return ImColor(124, 21, 153);
        case PinType::Object:   return ImColor(51, 150, 215);
        case PinType::Pose:     return ImColor(255, 150, 25);
        case PinType::Function: return ImColor(218, 0, 183);
        case PinType::Delegate: return ImColor(255, 48, 48);
        default:
            EG_CORE_ASSERT(false);
            return ImColor(0, 0, 0);
        }
    };

    static PinType GetPinType(GraphVariableType type)
    {
        switch (type)
        {
        case GraphVariableType::Bool: return PinType::Bool;
        case GraphVariableType::Float: return PinType::Float;
        case GraphVariableType::Animation: return PinType::Object;
        }
        EG_CORE_ASSERT(false);
        return PinType::Object;
    }

    class GraphEditor;
    struct GraphSerializationData;

	// Base class for UI graph editors (just the graph itself)
	class UIGraph
	{
	public:
        UIGraph(GraphEditor& editor, const std::string_view name);

		virtual ~UIGraph();

        virtual void OnImGuiRender(bool* pOpen = nullptr);

        void SetupNodeFactory();

        virtual void ProcessPendingDeletion();
        
        virtual void DrawNodes();
        virtual void DrawLinks();
        virtual void DrawNodeContextPopup();
        virtual void DrawPinContextPopup();
        virtual void DrawLinkContextPopup();
        virtual void DrawCreateNewNodePopup();

        virtual void Deserialize(const GraphSerializationData& data) {}

        const GraphEditor& GetEditor() const { return m_Editor; }
        GraphEditor& GetEditor() { return m_Editor; }

        virtual void DrawPinIcon(const Pin& pin, bool connected, int alpha)
        {
            using ax::Widgets::IconType;

            IconType iconType;
            ImColor  color = GetIconColor(pin.Type);
            color.Value.w = alpha / 255.0f;
            switch (pin.Type)
            {
            case PinType::Flow:     iconType = IconType::Flow;   break;
            case PinType::Bool:     iconType = IconType::Circle; break;
            case PinType::Int:      iconType = IconType::Circle; break;
            case PinType::Float:    iconType = IconType::Circle; break;
            case PinType::String:   iconType = IconType::Circle; break;
            case PinType::Object:   iconType = IconType::Circle; break;
            case PinType::Pose:     iconType = IconType::Circle; break;
            case PinType::Function: iconType = IconType::Circle; break;
            case PinType::Delegate: iconType = IconType::Square; break;
            default:
                EG_CORE_ASSERT(false);
                return;
            }

            ax::Widgets::Icon(ImVec2(static_cast<float>(m_GraphData.PinIconSize), static_cast<float>(m_GraphData.PinIconSize)), iconType, connected, color, ImColor(32, 32, 32, alpha));
        };

        int GetNextId()
        {
            return m_GraphData.NextId++;
        }

        const std::string& GetName() const { return m_GraphData.Name; }

        const GraphData& GetGraphData() const { return m_GraphData; }

        ed::LinkId GetNextLinkId()
        {
            return ed::LinkId(GetNextId());
        }

        void TouchNode(ed::NodeId id)
        {
            m_GraphData.NodeTouchTime[id] = m_TouchTime;
        }

        float GetTouchProgress(ed::NodeId id)
        {
            auto it = m_GraphData.NodeTouchTime.find(id);
            if (it != m_GraphData.NodeTouchTime.end() && it->second > 0.0f)
                return (m_TouchTime - it->second) / m_TouchTime;
            else
                return 0.0f;
        }

        void UpdateTouch()
        {
            const auto deltaTime = ImGui::GetIO().DeltaTime;
            for (auto& entry : m_GraphData.NodeTouchTime)
            {
                if (entry.second > 0.0f)
                    entry.second -= deltaTime;
            }
        }

        Node* FindNode(ed::NodeId id)
        {
            for (auto& node : m_GraphData.Nodes)
                if (node.ID == id)
                    return &node;

            return nullptr;
        }

        Link* FindLink(ed::LinkId id)
        {
            for (auto& link : m_GraphData.Links)
                if (link.ID == id)
                    return &link;

            return nullptr;
        }

        Pin* FindPin(ed::PinId id)
        {
            if (!id)
                return nullptr;

            for (auto& node : m_GraphData.Nodes)
            {
                for (auto& pin : node.InputPins)
                    if (pin.ID == id)
                        return &pin;

                for (auto& pin : node.OutputPins)
                    if (pin.ID == id)
                        return &pin;
            }

            return nullptr;
        }

        bool IsPinLinked(ed::PinId id)
        {
            if (!id)
                return false;

            for (auto& link : m_GraphData.Links)
                if (link.StartPinID == id || link.EndPinID == id)
                    return true;

            return false;
        }

        bool CanCreateLink(Pin* a, Pin* b)
        {
            if (!a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->NodeID == b->NodeID)
                return false;

            return true;
        }

        void BuildNode(Node& node)
        {
            uint32_t idx = 0;
            for (auto& input : node.InputPins)
            {
                input.NodeID = node.ID;
                input.Kind = PinKind::Input;
                input.Index = idx++;
            }

            idx = 0;
            for (auto& output : node.OutputPins)
            {
                output.NodeID = node.ID;
                output.Kind = PinKind::Output;
                output.Index = idx++;
            }

            node.Inputs.resize(node.InputPins.size());
            node.OutputsPerPin.resize(node.OutputPins.size());
        }

        void BuildNodes()
        {
            for (auto& node : m_GraphData.Nodes)
                BuildNode(node);
        }

        Node& AddNode(const std::string_view name, ImColor color = ImColor(255, 255, 255), bool bDeletable = true)
        {
            return m_GraphData.Nodes.emplace_back(GetNextId(), name, color, bDeletable);
        }

        void SetNodeAsVar(const Node& node, const std::string& varName)
        {
            m_VarToNodesMapping[varName].push_back(node.ID);
        }

        void OnVariableDeleted(const std::string& var);
        void OnVariableRenamed(const std::string& varName, const std::string& newName);

        virtual Node* GetOutputNode() { return nullptr; };

    protected:
        virtual void HandleBPNode(util::BlueprintNodeBuilder& builder, Node& node, Pin* newLinkPin);
        virtual void HandleTreeNode(Node& node, Pin* newLinkPin);
        virtual void HandleCommentNode(Node& node, Pin* newLinkPin);
        virtual void HandleNodeCreation(Node& node, ImVec2 pos, Pin* newNodeLinkPin);
        virtual void HandleCreatingDeletion();
        virtual void HandleDragDrop();
        virtual void HandleIfPopupShouldOpen();

        void ChangeVariableType(const std::string& varName, GraphVariableType newType);
        void DeleteNode(const Node* node);
        bool RenameVariable(const std::string& varName, const std::string& newName);
        void DeleteVariable(const std::string& var); // Deletes var and nodes

        void OnLinkCreated(const Link& link);
        void OnLinkDeleted(const Link& link);

    protected:
        GraphData m_GraphData;
        GraphEditor& m_Editor; // Owner of this UI graph
        const float m_TouchTime = 1.0f;

        ImVec2 m_CursorTopLeft;
        ImVec2 m_CreateNodeOpenPopupPos;
        bool m_bGetPopupPos = false;
        bool m_bShowCreateNewVarInPopup = false;

        ed::NodeId m_ContextNodeId = 0;
        ed::LinkId m_ContextLinkId = 0;
        ed::PinId  m_ContextPinId = 0;
        bool m_CreateNewNode = false;
        Pin* m_NewNodeLinkPin = nullptr;
        Pin* m_NewLinkPin = nullptr;

        std::map<std::string, std::vector<ax::NodeEditor::NodeId>> m_VarToNodesMapping;

        std::unordered_map<std::string, NodeFactoryMap> m_NodeFactory; // Key: Category
    };
}
