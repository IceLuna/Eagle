#pragma once

#include "Entity.h"
#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class SceneSerializer
	{
	private:
		SceneSerializer() = default;

	public:
		static bool Serialize(const Ref<Scene>& scene, const Path& filepath);
		static bool Serialize(const Ref<Scene>& scene, YAML::Emitter& out);

		static bool Deserialize(const Ref<Scene>& scene, const Path& filepath);
		static bool Deserialize(const Ref<Scene>& scene, const DataBuffer& data);
		static bool Deserialize(const Ref<Scene>& scene, YAML::Node& baseNode);

		// Just saves a scene with a given yaml description
		static bool SerializeWithYaml(const Path& filepath, const std::string& yaml);

	private:
		static void SerializeSkybox(const Ref<Scene>& scene, YAML::Emitter& out);
		static void DeserializeSkybox(const Ref<Scene>& scene, YAML::Node& node);
	};
}