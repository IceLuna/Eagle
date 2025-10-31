#pragma once

#include "Eagle/Core/Serializer.h"
#include "Eagle/UI/Graphs/GraphVariables.h"
#include "Eagle/Animation/Nodes/GraphNodeFactory.h"
#include "Eagle/Script/ScriptEngine.h"

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
        Vec4,
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
        PoseCache,
        PoseCacheGetter,
        BlendSpace,
        StateMachine,
        StateMachineState,
        BehaviorTask,
        BehaviorComposite,
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
        ed::PinId DisableInUIWhenPinIndexIsUsed{}; // When specified pin is used, draw this pin as disabled

        Pin(int id, std::string_view name, PinType type, const Ref<GraphVariable>& defaultValue = nullptr, const std::string help = "") :
            ID(id), NodeID(), Name(name), Type(type), Kind(PinKind::Input), Index(0), DefaultValue(defaultValue), HelpMessage(help)
        {
        }
    };

    struct PinConnectionData
    {
        ed::NodeId NodeID;
        uint32_t PinIndex; // The pin index it's connected to (first pin, or second pin, etc...)
    };


    struct CachedNodeData
    {
        CachedNodeData() = default;
        CachedNodeData(UIGraph* owner, ed::NodeId id) : Owner(owner), NodeID(id) {}

        UIGraph* Owner = nullptr;
        ed::NodeId NodeID;

        bool operator==(const CachedNodeData& other) const
        {
            return Owner == other.Owner && NodeID == other.NodeID;
        }
    };

    class AnimationGraphNode;
    // TODO: Simplify this node structure.
    // Split it into multiple structs: BaseNode, AnimationGraphNode, BehaviorTreeNode, etc...
    struct Node
    {
        UIGraph* Owner = nullptr; // Graph that created this Node
        ed::NodeId ID;
        GUID UUID{}; // `ID` above is affected by all graph elements (links, pins). But this is used to uniquely identify the node no matter what
        std::string Name; // Node's name, which can be used for node factory. So user provided names are not stored here, but rather in "UserData"
        std::vector<Pin> InputPins;
        std::vector<Pin> OutputPins;
        ImColor Color;
        NodeType Type;
        ImVec2 Size;
        Ref<AnimationGraphNode> GraphNode; // Used if a node is a function (for example, addition)
        Ref<UIGraph> Graph; // Used if a node is a graph (for example, state machine graph)
        CachedNodeData CachedNode; // Used if a node is a "PoseCacheGetter".
        ImVec2 PrevPosition = ImVec2(0, 0);

        std::vector<std::vector<PinConnectionData>> InputsPerPin;
        std::vector<std::vector<PinConnectionData>> OutputsPerPin; // One pin-output can be used as an input for multiple nodes.

        std::string UserData; // User provided data such as: node name; string, or comment
        bool bDeletable = true;
        bool bEditing = false; // Can be used to indicate that it's in "editing" state (for example, it'll be `true` while renaming a node)

        AIBehaviorNode BehaviorNodeData;
        std::string BehaviorNodeIndex;

        Node(UIGraph* owner, ed::NodeId id, const std::string_view name, ImColor color = ImColor(255, 255, 255), bool bDeletable = true) :
            Owner(owner), ID(id), Name(name), Color(color), bDeletable(bDeletable), Type(NodeType::Blueprint), Size(0, 0)
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

        bool CanAddPins() const { return m_CanAddPinsCallback ? m_CanAddPinsCallback(*this) : true; }
        bool CanRemovePins() const { return m_CanRemovePinsCallback ? m_CanRemovePinsCallback(*this) : true; }

        bool HasAddPinsCallback() const { return m_AddPinsCallback.operator bool(); }
        bool HasRemovePinsCallback() const { return m_RemovePinsCallback.operator bool(); }

        void OnAddPins() { m_AddPinsCallback(*this); ++m_AddedCounter; }
        void OnRemovePins() { m_RemovePinsCallback(*this); if (m_AddedCounter > 0) --m_AddedCounter; }

        template <typename Func>
        void SetAddPinsCallback(Func&& func)
        {
            m_AddPinsCallback = std::move(func);
        }

        template <typename Func>
        void SetRemovePinsCallback(Func&& func)
        {
            m_RemovePinsCallback = std::move(func);
        }

        template <typename Func>
        void SetCanAddPinsCallback(Func&& func)
        {
            m_CanAddPinsCallback = std::move(func);
        }

        template <typename Func>
        void SetCanRemovePinsCallback(Func&& func)
        {
            m_CanRemovePinsCallback = std::move(func);
        }

        uint32_t GetAddedCounter() const { return m_AddedCounter; }

    private:
        std::function<void(Node& node)> m_AddPinsCallback;
        std::function<bool(const Node& node)> m_CanAddPinsCallback;
        std::function<void(Node& node)> m_RemovePinsCallback;
        std::function<bool(const Node& node)> m_CanRemovePinsCallback;
        uint32_t m_AddedCounter = 0u; // Required for serialization so that we know how many times to call "AddPinsCallback" during deserialization
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
        case PinType::Vec4:      return ImColor(247, 226,  74);
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

    class GraphEditor;

    // TODO: Shouldn't exist on Engine side. Move to editor.
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
        Ref<GraphNode> Compile(VariablesMap& outUsedVars);

        // @node. Node to parse from.
        // @compiledGraphs. Required to prevent infinite recursion when processing pose caches.
        virtual Ref<GraphNode> Compile_Internal(Node* node, VariablesMap& outUsedVars, std::unordered_set<UIGraph*>& compiledGraphs);

        void SetupNodeFactory();

        virtual void DrawNodes();
        virtual void DrawLinks();
        virtual void DrawNodeContextPopup();
        virtual void DrawPinContextPopup();
        virtual void DrawLinkContextPopup();
        virtual void DrawCreateNewNodePopup();

        // Can return serialization data of inner graphs as well
        virtual GraphSerializationData Serialize() const;
        void Deserialize(const GraphEditorSerializationData& editorData, const GraphSerializationData& data);

        const GraphEditor& GetEditor() const { return m_Editor; }
        GraphEditor& GetEditor() { return m_Editor; }

        virtual void DrawPinIcon(const Pin& pin, bool connected, int alpha);

        int GetNextId()
        {
            return m_GraphData.NextId++;
        }

        ed::LinkId GetNextLinkId()
        {
            return ed::LinkId(m_GraphData.NextId++);
        }

        void SetName(const std::string_view name) { m_GraphData.Name = name; }
        const std::string& GetName() const { return m_GraphData.Name; }

        const GraphData& GetGraphData() const { return m_GraphData; }

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
        Node* FindNodeByGUID(const GUID& id);
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
            auto inserted = m_GraphData.Nodes.emplace(id, Node{ this, id, name, color, bDeletable });
            auto& it = inserted.first;
            return it->second;
        }

        Link& AddLink(const Pin* startPin, const Pin* endPin)
        {
            ed::LinkId id = GetNextLinkId();
            auto inserted = m_GraphData.Links.emplace(id, Link{ id, startPin->ID, endPin->ID });
            auto& link = inserted.first->second;
            link.Color = GetIconColor(startPin->Type);
            OnLinkCreated(link);
            return link;
        }

        void RemovePinLinks(ed::PinId pin)
        {
            for (auto it = m_GraphData.Links.begin(); it != m_GraphData.Links.end(); )
            {
                const auto& link = it->second;
                if (link.StartPinID == pin || link.EndPinID == pin)
                {
                    OnLinkDeleted(link);
                    it = m_GraphData.Links.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        virtual void OnVariableDeleted(const std::string& var);
        virtual void OnVariableRenamed(const std::string& varName, const std::string& newName);
        virtual void OnNodeAdded(Node& node);
        virtual void OnNodeDeleted(const Node& node);

        virtual Node* GetOutputNode() { return nullptr; };
        virtual ax::NodeEditor::NodeId GetOutputNodeID() const { return {}; };

        void SetID(GUID id) { m_ID = id; }
        GUID GetID() const { return m_ID; }

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
        virtual bool AllowRenaming() const { return true; }

        void ChangeVariableType(const std::string& varName, GraphVariableType newType);
        void DeleteNode(const Node* node);
        bool RenameVariable(const std::string& varName, const std::string& newName);
        void DeleteVariable(const std::string& var); // Deletes var and nodes

        // Make a copy of a 'graphName' instead of a ref, so that we're sure it doesn't change mid execution.
        // For example, if a ref was used, calling `RenameGraph(node.GetName(), newName)` would result in a bug, since `node.GetName()` would change at some point
        // And other graphs would get an updated `graphName` instead of an old one.
        virtual bool RenameGraph(std::string graphName, const std::string& newName);

        // Handles renaming
        virtual void OnNodeRenamingFinished(Node& node, const std::string& newName);

        virtual void OnLinkCreated(const Link& link);
        virtual void OnLinkDeleted(const Link& link);

        static void ShowLabel(const char* label, ImColor color);

        struct PoseCacheGetterDeserializationData
        {
            GUID Owner = GUID(0, 0);
            GUID ID = GUID(0, 0);
            GUID CacheNodeID = GUID(0, 0);
            GUID CacheNodeOwner = GUID(0, 0);
        };

        // @deserializedGraphs. Graphs that were deserialized during the process
        // @poseCacheGetterData. All pose cache getter nodes are stored here. We need to postpone their creation because we need to wait till all graphs are deserialized and have CachePose nodes ready.
        virtual void Deserialize_Internal(const GraphEditorSerializationData& editorData, const GraphSerializationData& data, std::vector<UIGraph*>& deserializedGraphs, std::vector<PoseCacheGetterDeserializationData>& poseCacheGetterData);

    protected:
        void Parse(Node* node, bool bCloneVars, VariablesMap& outVariables, std::unordered_set<UIGraph*>& compiledGraphs);
        void OnStartedRenamingNode(Node* node);

    protected:
        GUID m_ID;

        GraphData m_GraphData;
        GraphEditor& m_Editor; // Owner of this UI graph. TODO: Storing ref is sus
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
