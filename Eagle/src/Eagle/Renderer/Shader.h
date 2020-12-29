#pragma once

#include <string>

#include <glm/glm.hpp>

namespace Eagle
{
	class Shader
	{
	public:
		Shader(const std::string& vertexSource, const std::string& fragmentSource);
		~Shader();

		void Bind();
		void Unbind();

		void SetUniformMat4(const std::string& name, const glm::mat4& matrix);
		void SetUniformFloat(const std::string& name, float value);

	private:
		uint32_t m_RendererID;
	};
}