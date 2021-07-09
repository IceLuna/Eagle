#pragma once

#include <string>
#include <map>
#include <glm/glm.hpp>

namespace Eagle
{
	class Shader
	{
	public:
		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void Reload() = 0;
		virtual void BindOnReload(void (*func)()) = 0;

		virtual const std::filesystem::path& GetPath() const = 0;
		virtual uint32_t GetID() const = 0;

		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetIntArray(const std::string& name, const int* values, uint32_t count) = 0;

		virtual void SetFloat(const std::string& name, float value) = 0;
		virtual void SetFloat2(const std::string& name, const glm::vec2& value) = 0;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;

		virtual void SetMat3(const std::string& name, const glm::mat3& matrix) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& matrix) = 0;

		static Ref<Shader> Create(const std::filesystem::path& filepath);
	};

	class ShaderLibrary
	{
	public:
		static Ref<Shader> GetOrLoad(const std::filesystem::path& filepath);
		static void Add(const Ref<Shader>& shader);
		static bool Exists(const std::filesystem::path& filepath);

		static void ReloadAllShader();
		static const std::map<std::filesystem::path, Ref<Shader>>& GetAllShaders() { return m_Shaders; }

	private:
		static std::map<std::filesystem::path, Ref<Shader>> m_Shaders;
	};
}