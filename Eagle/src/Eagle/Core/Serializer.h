#pragma once

#include "Eagle/Core/Transform.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/YamlUtils.h"

namespace Eagle
{
	class Material;
	class PhysicsMaterial;
	class PublicField;
	class ScriptComponent;
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
	class AssetMesh;
	class AssetAudio;
	class AssetFont;
	class AssetMaterial;
	class AssetPhysicsMaterial;
	enum class AssetType;

	class Serializer
	{
	public:
		static void SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb);

		static void SerializeAsset(YAML::Emitter& out, const Ref<Asset>& asset);
		static void SerializeAssetTexture2D(YAML::Emitter& out, const Ref<AssetTexture2D>& asset);
		static void SerializeAssetTextureCube(YAML::Emitter& out, const Ref<AssetTextureCube>& asset);
		static void SerializeAssetMesh(YAML::Emitter& out, const Ref<AssetMesh>& asset);
		static void SerializeAssetAudio(YAML::Emitter& out, const Ref<AssetAudio>& asset);
		static void SerializeAssetFont(YAML::Emitter& out, const Ref<AssetFont>& asset);
		static void SerializeAssetMaterial(YAML::Emitter& out, const Ref<AssetMaterial>& asset);
		static void SerializeAssetPhysicsMaterial(YAML::Emitter& out, const Ref<AssetPhysicsMaterial>& asset);

		static void DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb);

		static Ref<Asset> DeserializeAsset(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTexture2D> DeserializeAssetTexture2D(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTextureCube> DeserializeAssetTextureCube(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetMesh> DeserializeAssetMesh(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetAudio> DeserializeAssetAudio(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetFont> DeserializeAssetFont(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetMaterial> DeserializeAssetMaterial(YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetPhysicsMaterial> DeserializeAssetPhysicsMaterial(YAML::Node& baseNode, const Path& pathToAsset);
		static AssetType GetAssetType(const Path& pathToAsset);
		
		static void SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field);
		static void DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent);
		static bool HasSerializableType(const PublicField& field);
	};
}
