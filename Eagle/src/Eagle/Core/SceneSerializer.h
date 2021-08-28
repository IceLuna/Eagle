#pragma once

#include "Entity.h"
#include "Eagle/Core/Scene.h"

#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class Material;

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
		void SerializeTexture(YAML::Emitter& out, const Ref<Texture>& texture, const std::string& textureName);

		void DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode);
		void DeserializeSkybox(YAML::Node& node);
		void DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform);
		void DeserializeMaterial(YAML::Node& materialNode, Ref<Material>& material);
		void DeserializeTexture(YAML::Node& parentNode, Ref<Texture>& texture, const std::string& textureName);

	private:
		Ref<Scene> m_Scene;
		
		//uint32_t - Entity's ID in *.eagle; Real entity ID; 
		std::unordered_map<uint32_t, Entity> m_AllEntities;

		//uint32_t - entity that has an parent, uint32_t - parent id
		std::unordered_map<uint32_t, uint32_t> m_Childs;
	};
}