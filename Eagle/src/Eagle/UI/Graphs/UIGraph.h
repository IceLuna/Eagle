#pragma once

#include "Eagle/Core/Serializer.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Animation/Nodes/GraphNodeFactory.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>
#include <examples/blueprints-example/utilities/builders.h>
#include <examples/blueprints-example/utilities/widgets.h>

namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;

namespace std
{
    template <>
    struct hash<ed::NodeId>
    {
        std::size_t operator()(const ed::NodeId& id) const
        {
            return std::hash<uintptr_t>()(id.Get());
        }
    };

    template <>
    struct hash<ed::LinkId>
    {
        std::size_t operator()(const ed::LinkId& id) const
        {
            return std::hash<uintptr_t>()(id.Get());
        }
    };
}

namespace Eagle
{
    enum class PinType
    {
        Flow,
        StateFlow, // Flow between machine states
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
        StateMachineState,
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
        std::string HelpMessage;

        Pin(int id, const char* name, PinType type, const Ref<GraphVariable>& defaultValue = nullptr, const std::string help = "") :
            ID(id), NodeID(), Name(name), Type(type), Kind(PinKind::Input), Index(0), DefaultValue(defaultValue), HelpMessage(help)
        {
        }
    };

    struct PinConnectionData
    {
        ed::NodeId NodeID;
        uint32_t PinIndex; // The pin index it's connected to (first pin, or second pin, etc...)
    };

    class AnimationGraphNode;
    struct Node
    {
        ed::NodeId ID;
        std::string Name; // Node's name, which can be used for node factory. So user provided names are not stored here, but rather in "UserData"
        std::vector<Pin> InputPins;
        std::vector<Pin> OutputPins;
        ImColor Color;
        NodeType Type;
        ImVec2 Size;
        Ref<AnimationGraphNode> GraphNode; // Used if a node is a function (for example, addition)
        Ref<UIGraph> Graph; // Used if a node is a graph (for example, state machine graph)

        std::vector<std::vector<PinConnectionData>> InputsPerPin;
        std::vector<std::vector<PinConnectionData>> OutputsPerPin; // One pin-output can be used as an input for multiple nodes.

        std::string UserData; // User provided data such as: node name; string, or comment
        bool bDeletable = true;
        bool bEditing = false; // Can be used to indicate that it's in "editing" state (for example, it'll be `true` while renaming a node)

        Node(ed::NodeId id, const std::string_view name, ImColor color = ImColor(255, 255, 255), bool bDeletable = true) :
            ID(id), Name(name), Color(color), bDeletable(bDeletable), Type(NodeType::Blueprint), Size(0, 0)
        {
        }

        void SetName(const std::string_view name)
        {
            // Graphs use 'UserData' to store user provided names, since 'Name' is used for node factory
            if (Graph)
                UserData = name;
            else
                Name = name;
        }

        const std::string& GetName() const { return Graph ? UserData : Name; }
        std::string& GetName() { return Graph ? UserData : Name; }
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

        std::unordered_map<ax::NodeEditor::NodeId, Node> Nodes;
        std::vector<ax::NodeEditor::NodeId> NodesPendingDeletion;

        std::unordered_map<ed::LinkId, Link> Links;
        std::map<ed::NodeId, float, NodeIdLess> NodeTouchTime;
    };

    static ImColor GetIconColor(PinType type)
    {
        switch (type)
        {
        case PinType::Flow:      return ImColor(255, 255, 255);
        case PinType::StateFlow: return ImColor(255, 255, 255);
        case PinType::Bool:      return ImColor(220, 48, 48);
        case PinType::Int:       return ImColor(68, 201, 156);
        case PinType::Float:     return ImColor(147, 226, 74);
        case PinType::String:    return ImColor(124, 21, 153);
        case PinType::Object:    return ImColor(51, 150, 215);
        case PinType::Pose:      return ImColor(255, 150, 25);
        case PinType::Function:  return ImColor(218, 0, 183);
        case PinType::Delegate:  return ImColor(255, 48, 48);
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

	// Base class for UI graph editors (just the graph itself)
	class UIGraph
	{
	public:
        UIGraph(GraphEditor& editor, const std::string_view name);

		virtual ~UIGraph();

        virtual void OnEvent(Event& e);

        virtual void OnImGuiRender(bool* pOpen = nullptr);

        // @outUsedVars. Map of variables that were used by this graph
        // @return. Returns an object that can be used to run compiled logic
        virtual Ref<GraphNode> Compile(VariablesMap& outUsedVars);

        void SetupNodeFactory();

        virtual void DrawNodes();
        virtual void DrawLinks();
        virtual void DrawNodeContextPopup();
        virtual void DrawPinContextPopup();
        virtual void DrawLinkContextPopup();
        virtual void DrawCreateNewNodePopup();

        // Can return serialization data of inner graphs as well
        virtual GraphSerializationData Serialize() const;
        virtual void Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data);

        const GraphEditor& GetEditor() const { return m_Editor; }
        GraphEditor& GetEditor() { return m_Editor; }

        virtual void DrawPinIcon(const Pin& pin, bool connected, int alpha);

        int GetNextId()
        {
            return m_GraphData.NextId++;
        }

        void SetName(const std::string_view name) { m_GraphData.Name = name; }
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

        Node* FindNode(ed::NodeId id);
        const Node* FindNode(ed::NodeId id) const;
        Link* FindLink(ed::LinkId id);
        Pin* FindPin(ed::PinId id);
        bool IsPinLinked(ed::PinId id);
        bool CanCreateLink(Pin* a, Pin* b);

        void BuildNode(Node& node);
        void BuildNodes();

        Node& AddNode(const std::string_view name, ImColor color = ImColor(255, 255, 255), bool bDeletable = true)
        {
            ed::NodeId id = GetNextId();
            auto inserted = m_GraphData.Nodes.emplace(id, Node{ id, name, color, bDeletable });
            auto& it = inserted.first;
            return it->second;
        }

        Link& AddLink(const Pin* startPin, const Pin* endPin)
        {
            ed::LinkId id = GetNextId();
            auto inserted = m_GraphData.Links.emplace(id, Link{ id, startPin->ID, endPin->ID });
            auto& link = inserted.first->second;
            link.Color = GetIconColor(startPin->Type);
            OnLinkCreated(link);
            return link;
        }

        virtual void OnVariableDeleted(const std::string& var);
        virtual void OnVariableRenamed(const std::string& varName, const std::string& newName);
        virtual void OnNodeAdded(Node& node);
        virtual void OnNodeDeleted(const Node& node);

        virtual Node* GetOutputNode() { return nullptr; };
        virtual ax::NodeEditor::NodeId GetOutputNodeID() { return {}; };

    protected:
        virtual void HandleBPNode(util::BlueprintNodeBuilder& builder, Node& node, Pin* newLinkPin);
        virtual void HandleStateNode(Node& node, Pin* newLinkPin);
        virtual void HandleCommentNode(Node& node, Pin* newLinkPin);
        virtual void HandleNodeCreation(Node& node, ImVec2 pos, Pin* newNodeLinkPin);
        virtual void HandleCreatingDeletion();
        virtual void HandleDragDrop();
        virtual void HandleIfPopupShouldOpen();

        virtual void ProcessPendingDeletion();

        // @Returns true if rejected
        virtual bool ProcessNewLinkRejection(const Pin& startPin, const Pin& endPin);

        // If returns true, variables are allowed in the graph
        virtual bool CanSpawnVariables() const { return true; }

        // If false, existing links will be disconnected
        virtual bool AllowMultipleLinksToInput() const { return false; }

        void ChangeVariableType(const std::string& varName, GraphVariableType newType);
        void DeleteNode(const Node* node);
        bool RenameVariable(const std::string& varName, const std::string& newName);
        void DeleteVariable(const std::string& var); // Deletes var and nodes

        // Make a copy of a 'graphName' instead of a ref, so that we're sure it doesn't change mid execution.
        // For example, if a ref was used, calling `RenameGraph(node.GetName(), newName)` would result in a bug, since `node.GetName()` would change at some point
        // And other graphs would get an updated `graphName` instead of an old one.
        virtual bool RenameGraph(std::string graphName, const std::string& newName);

        virtual void OnLinkCreated(const Link& link);
        virtual void OnLinkDeleted(const Link& link);

        static void ShowLabel(const char* label, ImColor color);

    private:
        void Parse(Node* node, bool bCloneVars, VariablesMap& outVariables);
        void OnStartedRenamingNode(Node* node);

    protected:
        GraphData m_GraphData;
        GraphEditor& m_Editor; // Owner of this UI graph
        const float m_TouchTime = 1.0f;

        std::vector<ed::NodeId> m_NodesWithGraph; // Nodes that have UIGraph

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

        std::string m_RenamingNodeTemp;

        std::map<std::string, std::vector<ax::NodeEditor::NodeId>> m_VarToNodesMapping;

        std::unordered_map<std::string, NodeFactoryMap> m_NodeFactory; // Key: Category
    };
}
