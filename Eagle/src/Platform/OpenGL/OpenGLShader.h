#pragma once

#include "Eagle/Renderer/Shader.h"

namespace Eagle
{

	typedef unsigned int GLenum;

	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::filesystem::path& filepath);
		virtual ~OpenGLShader();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void Reload() override;

		virtual const std::filesystem::path& GetPath() const override { return m_Path; }
		virtual uint32_t GetID() const override { return m_RendererID; }

		virtual void SetInt(const std::string& name, int value) override;
		virtual void SetIntArray(const std::string& name, const int* values, uint32_t count) override;

		virtual void SetFloat(const std::string& name, float value) override;
		virtual void SetFloat2(const std::string& name, const glm::vec2& value) override;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) override;

		virtual void SetMat3(const std::string& name, const glm::mat3& matrix) override;
		virtual void SetMat4(const std::string& name, const glm::mat4& matrix) override;

	private:
		void FreeMemory();
		std::string ReadFile(const std::filesystem::path& filepath);
		std::unordered_map<GLenum, std::string> Preprocess(const std::string& source);
		void CompileAndLink(const std::unordered_map<GLenum, std::string>& shaderSources);
	private:
		uint32_t m_RendererID;
		std::filesystem::path m_Path;
	};
}
