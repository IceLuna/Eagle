#pragma once

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Animation/AnimationGraph.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>
#include <examples/blueprints-example/utilities/builders.h>
#include <examples/blueprints-example/utilities/widgets.h>

namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;
using ax::Widgets::IconType; // TODO: Remove

namespace Eagle
{
    class AnimationGraphNode;
    class AssetAnimationGraph;

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
        Ref<AnimationGraphVariable> DefaultValue;

        Pin(int id, const char* name, PinType type, const Ref<AnimationGraphVariable>& defaultValue = nullptr) :
            ID(id), NodeID(), Name(name), Type(type), Kind(PinKind::Input), Index(0), DefaultValue(defaultValue)
        {
        }
    };

    struct OutputConnectionData
    {
        ed::NodeId NodeID;
        uint32_t PinIndex; // The pin index it's connected to (first pin, or second pin, etc...)
    };

    struct Node
    {
        ed::NodeId ID;
        std::string Name;
        std::vector<Pin> InputPins;
        std::vector<Pin> OutputPins;
        ImColor Color;
        NodeType Type;
        ImVec2 Size;
        Ref<AnimationGraphNode> GraphNode;

        std::vector<ed::NodeId> Inputs;
        std::vector<std::vector<OutputConnectionData>> OutputsPerPin; // One pin-output can be used as an input for multiple nodes.

        std::string State;
        std::string SavedState;
        std::string UserData = "Message";
        bool bDeletable = true;
        bool bEditing = false;

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

	class AnimationGraphEditor
	{
	public:
		AnimationGraphEditor(const Ref<AssetAnimationGraph>& graph);
		~AnimationGraphEditor();

		void OnImGuiRender(bool* pOpen = nullptr);

        const Ref<AssetAnimationGraph>& GetGraphAsset() const { return m_Graph; }

		int GetNextId()
		{
			return m_NextId++;
		}

		ed::LinkId GetNextLinkId()
		{
			return ed::LinkId(GetNextId());
		}

        void TouchNode(ed::NodeId id)
        {
            m_NodeTouchTime[id] = m_TouchTime;
        }

        float GetTouchProgress(ed::NodeId id)
        {
            auto it = m_NodeTouchTime.find(id);
            if (it != m_NodeTouchTime.end() && it->second > 0.0f)
                return (m_TouchTime - it->second) / m_TouchTime;
            else
                return 0.0f;
        }

        void UpdateTouch()
        {
            const auto deltaTime = ImGui::GetIO().DeltaTime;
            for (auto& entry : m_NodeTouchTime)
            {
                if (entry.second > 0.0f)
                    entry.second -= deltaTime;
            }
        }

        Node* FindNode(ed::NodeId id)
        {
            for (auto& node : m_Nodes)
                if (node.ID == id)
                    return &node;

            return nullptr;
        }

        Link* FindLink(ed::LinkId id)
        {
            for (auto& link : m_Links)
                if (link.ID == id)
                    return &link;

            return nullptr;
        }

        Pin* FindPin(ed::PinId id)
        {
            if (!id)
                return nullptr;

            for (auto& node : m_Nodes)
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

            for (auto& link : m_Links)
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

        void BuildNode(Node* node)
        {
            uint32_t idx = 0;
            for (auto& input : node->InputPins)
            {
                input.NodeID = node->ID;
                input.Kind = PinKind::Input;
                input.Index = idx++;
            }

            idx = 0;
            for (auto& output : node->OutputPins)
            {
                output.NodeID = node->ID;
                output.Kind = PinKind::Output;
                output.Index = idx++;
            }

            node->Inputs.resize(node->InputPins.size());
            node->OutputsPerPin.resize(node->OutputPins.size());
        }

        void BuildNodes()
        {
            for (auto& node : m_Nodes)
                BuildNode(&node);
        }

        void Save();

        Node* SpawnInputActionNode();
        Node* SpawnBranchNode();
        Node* SpawnDoNNode();
        Node* SpawnOutputActionNode();
        Node* SpawnPrintStringNode();
        Node* SpawnMessageNode();
        Node* SpawnSetTimerNode();
        Node* SpawnWeirdNode();
        Node* SpawnTraceByChannelNode();
        Node* SpawnTreeSequenceNode();
        Node* SpawnTreeTaskNode();
        Node* SpawnTreeTask2Node();
        Node* SpawnHoudiniTransformNode();
        Node* SpawnHoudiniGroupNode();
        Node* SpawnOutputPoseNode();
        Node* SpawnEntryNode();
        Node* SpawnIntToStringNode();
        Node* SpawnVarNode(const std::string& name, PinType type);

        // Animations
        Node* SpawnAnimBlendNode(const std::string_view name);
        Node* SpawnAnimCalculateAdditiveNode(const std::string_view name);
        Node* SpawnAnimAdditiveBlendNode(const std::string_view name);
        Node* SpawnAnimClipNode(const std::string_view name);
        Node* SpawnSelectPoseByBoolNode(const std::string_view name);

        // Logical
        Node* SpawnAndNode(const std::string_view name);
        Node* SpawnOrNode(const std::string_view name);
        Node* SpawnXorNode(const std::string_view name);
        Node* SpawnNotNode(const std::string_view name);
        Node* SpawnLessNode(const std::string_view name);
        Node* SpawnLessEqNode(const std::string_view name);
        Node* SpawnGreaterNode(const std::string_view name);
        Node* SpawnGreaterEqNode(const std::string_view name);
        Node* SpawnEqualNode(const std::string_view name);
        Node* SpawnNotEqualNode(const std::string_view name);

        // Math
        Node* SpawnAddNode(const std::string_view name);
        Node* SpawnSubNode(const std::string_view name);
        Node* SpawnMulNode(const std::string_view name);
        Node* SpawnDivNode(const std::string_view name);
        Node* SpawnSinNode(const std::string_view name);
        Node* SpawnCosNode(const std::string_view name);
        Node* SpawnToRadNode(const std::string_view name);
        Node* SpawnToDegNode(const std::string_view name);

        // Other
        Node* SpawnComment(const std::string_view name);

        void OnStart();
        void OnStop();

        ImColor GetIconColor(PinType type)
        {
            switch (type)
            {
            default:
            case PinType::Flow:     return ImColor(255, 255, 255);
            case PinType::Bool:     return ImColor(220, 48, 48);
            case PinType::Int:      return ImColor(68, 201, 156);
            case PinType::Float:    return ImColor(147, 226, 74);
            case PinType::String:   return ImColor(124, 21, 153);
            case PinType::Object:   return ImColor(51, 150, 215);
            case PinType::Pose:     return ImColor(255, 150, 25);
            case PinType::Function: return ImColor(218, 0, 183);
            case PinType::Delegate: return ImColor(255, 48, 48);
            }
        };

        void DrawPinIcon(const Pin& pin, bool connected, int alpha)
        {
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
                return;
            }

            ax::Widgets::Icon(ImVec2(static_cast<float>(m_PinIconSize), static_cast<float>(m_PinIconSize)), iconType, connected, color, ImColor(32, 32, 32, alpha));
        };

        void ShowLeftPane(float paneWidth);

        void Compile();

    private:
        void HandleBPNode(util::BlueprintNodeBuilder& builder, Node& node, Pin* newLinkPin);
        void HandleTreeNode(Node& node, Pin* newLinkPin);
        void HandleHoudiniNode(Node& node, Pin* newLinkPin);
        void HandleCommentNode(Node& node, Pin* newLinkPin);
        void DeleteNode(const Node* node);
        void ChangeVariableType(const std::string& varName, GraphVariableType newType);
        bool RenameVariable(const std::string& varName, const std::string& newName);
        void OnLinkCreated(const Link& link);
        void OnLinkDeleted(const Link& link);
        void HandleNodeCreation(Node* node, ImVec2 pos, Pin* newNodeLinkPin);
        void Deserialize();
        void SetupNodeFactory();
        void SelectVariable(const std::string& var);
        void DeleteVariable(const std::string& var); // Deletes var and nodes

        bool CreateVariable(const Ref<AnimationGraphVariable>& var, const std::string& name)
        {
            if (m_Variables.find(name) != m_Variables.end())
                return false;

            m_Variables.emplace(name, var);
            return true;
        }

        bool RemoveVariable(const std::string& name) // Deletes var. Used for temp removal of vars to replace them
        {
            return m_Variables.erase(name) == 1;
        }

        const Ref<AnimationGraphVariable>& GetVariable(const std::string& name) const
        {
            static Ref<AnimationGraphVariable> s_Null;
            auto it = m_Variables.find(name);
            if (it != m_Variables.end())
                return it->second;

            return s_Null;
        }

        const VariablesMap& GetVariables() const { return m_Variables; }

        template <typename T>
        std::string CreateNewVar(const Ref<AnimationGraphVariable>& defaultVal, const std::string& baseName = "NewVar")
        {
            static_assert(std::is_base_of_v<AnimationGraphVariable, T> == true, "Not a var");

            Ref<T> var = MakeRef<T>();
            if (defaultVal)
            {
                if (auto casted = Cast<T>(defaultVal))
                    var->Value = casted->Value;
            }
            std::string name = baseName;
            if (CreateVariable(var, name) == false)
            {
                size_t i = 1;
                do
                {
                    name = baseName + std::to_string(i++);
                } while (CreateVariable(var, name) == false);
            }
            return name;
        }

        std::string CreateNewVarFromType(GraphVariableType type, const Ref<AnimationGraphVariable>& defaultVal, const std::string& baseName = "NewVar")
        {
            switch (type)
            {
                case GraphVariableType::Bool: return CreateNewVar<AnimationGraphVariableBool>(defaultVal, baseName);
                case GraphVariableType::Float: return CreateNewVar<AnimationGraphVariableFloat>(defaultVal, baseName);
                case GraphVariableType::Animation: return CreateNewVar<AnimationGraphVariableAnimation>(defaultVal, baseName);
            }
            EG_CORE_ASSERT(false);
            return "";
        }

        // @bCloneVars. If set to true, vars will be cloned before set to graph
        // @outVariables. All variables that were used
        void Parse(Node* node, bool bCloneVars, VariablesMap& outVariables);

        bool IgnoreChangedEvent() const { return m_bIgnoreChangedEvent; }

	private:
        ax::NodeEditor::Detail::EditorContext* m_Editor = nullptr;

		int                  m_NextId = 1;
		const int            m_PinIconSize = 24;
        std::vector<Node>    m_Nodes; // TODO: Improve by replacing with map?
        std::vector<ax::NodeEditor::NodeId> m_NodesPendingDeletion;
		std::vector<Link>    m_Links;
        Ref<Texture2D>       m_HeaderTexture;
        Ref<Texture2D>       m_SaveTexture;
        Ref<Texture2D>       m_RestoreTexture;
		ImTextureID          m_HeaderBackground = nullptr;
		ImTextureID          m_SaveIcon = nullptr;
		ImTextureID          m_RestoreIcon = nullptr;
		const float          m_TouchTime = 1.0f;
		std::map<ed::NodeId, float, NodeIdLess> m_NodeTouchTime;
        ImVec2 m_CreateNodeOpenPopupPos;
        bool m_bGetPopupPos = false;
        bool m_bShowCreateNewVarInPopup = false;
        ImVec2 m_StartMousePos;

        Ref<AssetAnimationGraph> m_Graph;
        ax::NodeEditor::NodeId m_OutputNodeId;

        VariablesMap m_Variables;
        std::map<std::string, std::vector<ax::NodeEditor::NodeId>> m_VarToNodesMapping;
        std::string m_SelectedVar;
        std::string m_RenamingVarTemp;

        bool m_bIgnoreChangedEvent = true;

        typedef Node* (AnimationGraphEditor::*CreateNodeFunc)(const std::string_view name);

        using NodeFactoryMap = std::unordered_map<std::string, CreateNodeFunc>; // Key: Nod name; Value: Node creating function
        std::unordered_map<std::string, NodeFactoryMap> m_NodeFactory; // Key: Category
	};
}
