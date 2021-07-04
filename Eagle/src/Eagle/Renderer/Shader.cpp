#include "egpch.h"
#include "Shader.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Eagle
{
	std::map<std::filesystem::path, Ref<Shader>> ShaderLibrary::m_Shaders;

	Ref<Shader> Shader::Create(const std::filesystem::path& filepath)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
			return nullptr;

		case RendererAPI::API::OpenGL:
			return MakeRef<OpenGLShader>(filepath);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Shader> ShaderLibrary::GetOrLoad(const std::filesystem::path& filepath)
	{
		if (Exists(filepath))
			return m_Shaders[filepath];

		Ref<Shader> shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		const std::filesystem::path& filepath = shader->GetPath();
		m_Shaders[filepath] = shader;
	}

	bool ShaderLibrary::Exists(const std::filesystem::path& filepath)
	{
		return m_Shaders.find(filepath) != m_Shaders.end();
	}

	void ShaderLibrary::ReloadAllShader()
	{
		for (auto& it : m_Shaders)
			it.second->Reload();
	}
}