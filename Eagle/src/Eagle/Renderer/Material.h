#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	class Material
	{
	public:
		glm::vec4 Diffuse = glm::vec4(1.f);
		glm::vec3 Ambient = glm::vec3(0.1f);
		glm::vec3 Specular = glm::vec3(0.5f);
		float Shininess = 32.f;
	};
}
