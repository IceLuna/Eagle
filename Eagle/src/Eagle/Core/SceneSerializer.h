#pragma once

#include "Eagle/Core/Scene.h"

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
		Ref<Scene> m_Scene;
	};
}