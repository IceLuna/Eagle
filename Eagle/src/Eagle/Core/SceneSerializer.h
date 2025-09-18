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
		bool Serialize(YAML::Emitter& out);

		bool Deserialize(const Path& filepath);
		bool Deserialize(YAML::Node& baseNode);

	private:
		void DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode, uint32_t collisionGroupValidMasks);

		void SerializeSkybox(YAML::Emitter& out);
		void DeserializeSkybox(YAML::Node& node);

	private:
		Ref<Scene> m_Scene;
	};
}