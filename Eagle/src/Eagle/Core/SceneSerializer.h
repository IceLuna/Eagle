#pragma once

#include "Entity.h"
#include "Eagle/Core/Scene.h"

#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class Material;
	class PhysicsMaterial;
	class PublicField;
	class ScriptComponent;
	class StaticMesh;
	class Sound3D;
	struct SoundSettings;

	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		bool Serialize(const std::filesystem::path& filepath);
		bool SerializeBinary(const std::filesystem::path& filepath);

		bool Deserialize(const std::filesystem::path& filepath);
		bool DeserializeBinary(const std::filesystem::path& filepath);
	
	private:
		void SerializeEntity(YAML::Emitter& out, Entity& entity);
		void SerializeSkybox(YAML::Emitter& out);
		void SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform);
		void SerializeMaterial(YAML::Emitter& out, const Ref<Material>& material);
		void SerializePhysicsMaterial(YAML::Emitter& out, const Ref<PhysicsMaterial>& material);
		void SerializeTexture(YAML::Emitter& out, const Ref<Texture>& texture, const std::string& textureName);
		void SerializeStaticMesh(YAML::Emitter& out, const Ref<StaticMesh>& staticMesh);
		void SerializeSound(YAML::Emitter& out, const Ref<Sound3D>& sound, const SoundSettings& settings);

		void DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode);
		void DeserializeSkybox(YAML::Node& node);
		void DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform);
		void DeserializeMaterial(YAML::Node& materialNode, Ref<Material>& material);
		void DeserializePhysicsMaterial(YAML::Node& materialNode, Ref<PhysicsMaterial>& material);
		void DeserializeTexture(YAML::Node& parentNode, Ref<Texture>& texture, const std::string& textureName);
		void DeserializeStaticMesh(YAML::Node& meshNode, Ref<StaticMesh>& staticMesh);
		void DeserializeSound(YAML::Node& audioNode, std::filesystem::path& sound, SoundSettings& settings);

		void SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field);
		void DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent);
		bool HasSerializableType(const PublicField& field);

	private:
		Ref<Scene> m_Scene;
		
		//uint32_t - Entity's ID in *.eagle; Real entity ID; 
		std::unordered_map<uint32_t, Entity> m_AllEntities;

		//uint32_t - entity that has an parent, uint32_t - parent id
		std::unordered_map<uint32_t, uint32_t> m_Childs;
	};
}