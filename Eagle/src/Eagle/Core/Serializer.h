#pragma once

#include "Eagle/Core/Entity.h"
#include "Eagle/Math/Transform.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Animation/AnimationGraph.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/YamlUtils.h"
#include "Eagle/AI/BehaviorGraph.h"
#include "Eagle/Renderer/TextureCompressor.h"

namespace Eagle
{
	class Material;
	class PhysicsMaterial;
	class PublicField;
	class ReverbComponent;
	class StaticMesh;
	class Sound;
	class Texture;
	class Texture2D;
	class Reverb3D;
	class Font;

	class Asset;
	class AssetTexture2D;
	class AssetTextureCube;
	class AssetStaticMesh;
	class AssetSkeletalMesh;
	class AssetAudio;
	class AssetFont;
	class AssetMaterial;
	class AssetPhysicsMaterial;
	class AssetSoundGroup;
	class AssetEntity;
	class AssetAnimation;
	class AssetAnimationGraph;
	class AssetParticleSystem;
	class AssetAnimationBlendSpace;
	class AssetScene;
	class AssetBehaviorGraph;

	enum class AssetType;
	enum class AssetTexture2DFormat;
	enum class AssetTextureCubeFormat;
	struct SceneRendererSettings;
	struct BoneNode;
	struct SkeletalMeshAnimation;

	struct GraphConnectionData
	{
		GUID NodeID = GUID(0, 0); // NodeID the pin is connected to
		uint32_t PinIndex = uint32_t(-1); // PinIndex the pin is connected to
	};

	enum class GraphNodeType
	{
		Node, Variable, PoseCache, PoseCacheGetter, BlendSpace, AIBehaviorNode
	};

	struct InputPinData
	{
		Ref<GraphVariable> DefaultValue;
		uint32_t Index = 0u; // Index inside `Node::InputPins` or `Node::OutputPins`
	};

	struct GraphNodeSerializationData
	{
		GUID OwnerID; // ID of GraphSerializationData
		std::string Name;
		glm::vec2 Position = glm::vec2(0);
		glm::vec2 Size = glm::vec2{0.f};
		GUID NodeID = GUID(0, 0);
		GUID CachedOwnerID = GUID(0, 0); // ID of GraphSerializationData
		GUID CachedNodeID = GUID(0, 0);
		GraphNodeType Type = GraphNodeType::Node;
		uint32_t AddedCounter = 0u; // Required for serialization so that we know how many times to call "AddPinsCallback" during deserialization
		std::vector<GraphConnectionData> OutputConnections;
		std::vector<InputPinData> InputPins;
		bool bOutputNode = false;

		std::string UserData; // Used by nodes such as "Comment" to save comment

		Ref<AssetAnimationBlendSpace> BlendSpace; // Used if it's a blend space node

		// These are used if it's a behavior graph node
		AIBehaviorClassData BehaviorClassData;
		std::vector<AIBehaviorClassData> AttachedDecorators;
	};

	struct GraphVariableSerializationData
	{
		std::string Name;
		Ref<GraphVariable> Value;
	};

	struct GraphSerializationData
	{
		std::string Name = "Animation Graph";
		std::vector<GraphNodeSerializationData> Nodes;
		glm::vec2 ScrollOffset = glm::vec2{0.f};
		GUID ID;
		float Zoom = 1.f;

		std::vector<GraphSerializationData> Subgraphs;
	};

	struct GraphEditorSerializationData
	{
		std::vector<GraphVariableSerializationData> Variables;
		GraphSerializationData Graph;
	};

	class Serializer
	{
	public:
		static void EmitBoneNode(YAML::Emitter& out, const BoneNode& node);
		static void SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb);
		static void SerializeEntity(YAML::Emitter& out, Entity entity);
		static void SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform);
		static void SerializeRendererSettings(YAML::Emitter& out, const SceneRendererSettings& settings);
		static void SerializeProjectCollisionGroupGUIDs(YAML::Emitter& out);
		
		static void ReadBoneNode(const YAML::Node& baseNode, BoneNode& node);
		static void DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb);
		// @outEntityID. Entity ID of the entity that was created
		// @outParentID. Its parent ID
		[[nodiscard]] static Entity DeserializeEntity(const Ref<Scene>& scene, const YAML::Node& entityNode, uint32_t collisionGroupValidMasks, uint32_t* outEntityID, int* outParentID = nullptr); // Doesn't handle parents
		static void DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform);
		static void DeserializeRendererSettings(YAML::Node& node, SceneRendererSettings& settings);
		static uint32_t DeserializeProjectCollisionGroupGUIDs(const YAML::Node& node);

		// Nullptr can be passed to create an empty asset
		static ScopedDataBuffer SerializeAsset(const Ref<Asset>& asset);
		static ScopedDataBuffer SerializeAssetTexture2D(const Ref<AssetTexture2D>& asset);
		static ScopedDataBuffer SerializeAssetTextureCube(const Ref<AssetTextureCube>& asset);
		static ScopedDataBuffer SerializeAssetStaticMesh(const Ref<AssetStaticMesh>& asset);
		static ScopedDataBuffer SerializeAssetSkeletalMesh(const Ref<AssetSkeletalMesh>& asset);
		static ScopedDataBuffer SerializeAssetAudio(const Ref<AssetAudio>& asset);
		static ScopedDataBuffer SerializeAssetFont(const Ref<AssetFont>& asset);
		static ScopedDataBuffer SerializeAssetMaterial(const Ref<AssetMaterial>& asset);
		static ScopedDataBuffer SerializeAssetPhysicsMaterial(const Ref<AssetPhysicsMaterial>& asset);
		static ScopedDataBuffer SerializeAssetSoundGroup(const Ref<AssetSoundGroup>& asset);
		static ScopedDataBuffer SerializeAssetEntity(const Ref<AssetEntity>& asset);
		static ScopedDataBuffer SerializeAssetAnimation(const Ref<AssetAnimation>& asset);
		static ScopedDataBuffer SerializeAssetAnimationGraph(const Ref<AssetAnimationGraph>& asset, const Ref<AssetSkeletalMesh>& meshAsset = nullptr); // `meshAsset` is used if asset is nullptr
		static ScopedDataBuffer SerializeAssetParticleSystem(const Ref<AssetParticleSystem>& asset);
		static ScopedDataBuffer SerializeAssetAnimationBlendSpace(const Ref<AssetAnimationBlendSpace>& asset, const Ref<AssetSkeletalMesh>& meshAsset = nullptr); // `meshAsset` is used if asset is nullptr
		static ScopedDataBuffer SerializeAssetBehaviorGraph(const Ref<AssetBehaviorGraph>& asset);

		static ScopedDataBuffer SerializeAssetTexture2DFromData(const DataBuffer& textureData, const std::vector<ScopedDataBuffer>& compressedDataPerMip, ImageFormat compressedFormat, const GUID& guid,
			const Path& pathToRaw, FilterMode filterMode, AddressMode addressMode, float anisotropy, uint32_t mipsCount, uint32_t width, uint32_t height, AssetTexture2DFormat format,
			TextureCompressor::Quality compression, bool bNormalMap);
		static ScopedDataBuffer SerializeAssetTextureCubeFromData(const DataBuffer& textureData, const GUID& guid, const Path& pathToRaw,
			AssetTextureCubeFormat format, uint32_t layerSize, uint32_t prefilterSize);
		static ScopedDataBuffer SerializeAssetStaticMeshFromMesh(const Ref<StaticMesh>& mesh, const GUID& guid, const Path& pathToRaw);
		static ScopedDataBuffer SerializeAssetSkeletalMeshFromMesh(const Ref<SkeletalMesh>& mesh, const GUID& guid, const Path& pathToRaw);
		static ScopedDataBuffer SerializeAssetAudioFromData(const DataBuffer& audioData, const GUID& guid, const Path& pathToRaw,
			float volume, float pitch, float pan, const Ref<AssetSoundGroup>& soundGroup);
		static ScopedDataBuffer SerializeAssetFontFromData(const DataBuffer& fontData, const GUID& guid, const Path& pathToRaw);
		static ScopedDataBuffer SerializeAssetAnimationFromData(const GUID& guid, const Path& pathToRaw, uint32_t animIndex,
			const SkeletalMeshAnimation& anim, const Ref<AssetSkeletalMesh>& skeletal);

		static Ref<Asset> DeserializeAsset(const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<Asset> DeserializeAsset(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTexture2D> DeserializeAssetTexture2D(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTextureCube> DeserializeAssetTextureCube(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetStaticMesh> DeserializeAssetStaticMesh(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetSkeletalMesh> DeserializeAssetSkeletalMesh(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetAudio> DeserializeAssetAudio(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetFont> DeserializeAssetFont(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetMaterial> DeserializeAssetMaterial(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetPhysicsMaterial> DeserializeAssetPhysicsMaterial(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetSoundGroup> DeserializeAssetSoundGroup(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetEntity> DeserializeAssetEntity(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetAnimation> DeserializeAssetAnimation(const DataBuffer& data, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetAnimationGraph> DeserializeAssetAnimationGraph(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetParticleSystem> DeserializeAssetParticleSystem(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetAnimationBlendSpace> DeserializeAssetAnimationBlendSpace(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetScene> DeserializeAssetScene(const DataBuffer& data, const Path& pathToAsset);
		static Ref<AssetBehaviorGraph> DeserializeAssetBehaviorGraph(const DataBuffer& data, const Path& pathToAsset);

		static AssetType GetAssetType(const DataBuffer& assetData);
		static AssetType GetAssetType(const Path& pathToAsset);
		
		static void SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field);
		static void DeserializePublicFieldValues(YAML::Node& publicFieldsNode, std::vector<PublicField>& publicFields);
		static bool HasSerializableType(const PublicField& field);
	};
}
