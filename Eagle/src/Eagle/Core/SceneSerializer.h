#pragma once

#include "Entity.h"
#include "Eagle/Core/Scene.h"

#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		bool Serialize(const std::string& filepath);
		bool SerializeBinary(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeBinary(const std::string& filepath);
	
	private:
		void SerializeEntity(YAML::Emitter& out, Entity& entity);
		void DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode);

	private:
		Ref<Scene> m_Scene;
		
		//Entity - Created entity; uint32_t - its ID
		std::unordered_map<uint32_t, Entity> m_AllEntities;

		//Entity - entity that has an owner, uint32_t - owners id
		std::unordered_map<Entity, uint32_t> m_Childs;
	};
}