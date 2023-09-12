#pragma once

#include "Entity.h"
#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		bool Serialize(const Path& filepath);
		bool SerializeBinary(const Path& filepath);

		bool Deserialize(const Path& filepath);
		bool DeserializeBinary(const Path& filepath);
	
	private:
		void SerializeEntity(YAML::Emitter& out, Entity& entity);
		void DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode);

		void SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform);
		void DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform);

		void SerializeSkybox(YAML::Emitter& out);
		void DeserializeSkybox(YAML::Node& node);

	private:
		Ref<Scene> m_Scene;
		
		//uint32_t - Entity's ID in *.eagle; Real entity ID; 
		std::unordered_map<uint32_t, Entity> m_AllEntities;

		//uint32_t - entity that has an parent, uint32_t - parent id
		std::unordered_map<uint32_t, uint32_t> m_Childs;
	};
}