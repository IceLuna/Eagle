#include "egpch.h"
#include "Shader.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Eagle
{
	std::vector<Ref<Shader>>    ShaderLibrary::m_Shaders;
	std::map<Path, Ref<Shader>>	ShaderLibrary::m_ShadersByPath;

	Ref<Shader> Shader::Create(const Path& path, const ShaderDefines& defines)
	{
		if (!std::filesystem::exists(path))
		{
			EG_RENDERER_ERROR("Couldn't find shader: {}", path);
			return nullptr;
		}

		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char c) { return std::tolower(c); });

		ShaderType shaderType = ShaderType::Vertex;
		if (extension == ".vert")
			shaderType = ShaderType::Vertex;
		else if (extension == ".frag")
			shaderType = ShaderType::Fragment;
		else if (extension == ".comp")
			shaderType = ShaderType::Compute;
		else
		{
			EG_RENDERER_ERROR("Invalid shader extension. Couldn't deduce its type: {}", path);
			return nullptr;
		}

		return Create(path, shaderType, defines);
	}

	Ref<Shader> Shader::Create(const Path& path, ShaderType shaderType, const ShaderDefines& defines)
	{
		Ref<Shader> result;
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan:
				result = MakeRef<VulkanShader>(path, shaderType, defines);
				break;
			default:
				EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		}
		ShaderLibrary::Add(result);
		return result;
	}

	Ref<Shader> ShaderLibrary::GetOrLoad(const Path& filepath, ShaderType shaderType)
	{
		if (Exists(filepath))
			return m_ShadersByPath[std::filesystem::absolute(filepath)];

		Ref<Shader> shader = Shader::Create(filepath, shaderType);
		Add(shader);
		return shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		const Path filepath = std::filesystem::absolute(shader->GetPath());
		m_Shaders.push_back(shader);
		m_ShadersByPath[filepath] = shader;
	}

	bool ShaderLibrary::Exists(const Path& filepath)
	{
		return m_ShadersByPath.find(std::filesystem::absolute(filepath)) != m_ShadersByPath.end();
	}

	void ShaderLibrary::ReloadAllShaders()
	{
		for (auto& shader : m_Shaders)
			shader->Reload();
	}
}