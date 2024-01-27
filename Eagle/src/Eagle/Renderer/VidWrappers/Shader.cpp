#include "egpch.h"
#include "Shader.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/YamlUtils.h"

#include "Platform/Vulkan/VulkanShader.h"

namespace Eagle
{
	std::vector<Ref<Shader>>    ShaderManager::m_Shaders;
	std::unordered_map<Path, Ref<Shader>>	ShaderManager::m_ShadersByPath;
	std::unordered_map<Path, std::string>	ShaderManager::m_ShadersSourceCode;
	static bool s_bGame = false;

	namespace Utils
	{
		static bool IsShaderExtension(const Path& path)
		{
			constexpr char* shaderExtensions[] = { ".h", ".comp", ".vert", ".frag" };
			for (const auto& extension : shaderExtensions)
				if (Utils::HasExtension(path, extension))
					return true;

			return false;
		}

		static std::unordered_map<Path, std::string> LoadShadersSourceCode()
		{
			std::unordered_map<Path, std::string> sourceCodes;

			const Path shadersFolder = Application::GetCorePath() / "assets/shaders";
			for (auto& dirEntry : std::filesystem::recursive_directory_iterator(shadersFolder))
			{
				if (dirEntry.is_directory())
					continue;

				Path shaderPath = dirEntry.path();
				if (!IsShaderExtension(shaderPath))
					continue;

				std::string sourceCode = FileSystem::ReadText(shaderPath);
				if (!sourceCode.empty())
					sourceCodes.emplace(std::filesystem::relative(shaderPath, shadersFolder), std::move(sourceCode));
			}
			return sourceCodes;
		}
	}

	Ref<Shader> Shader::Create(const Path& path, const ShaderDefines& defines)
	{
		if (!std::filesystem::exists(path))
		{
			EG_RENDERER_ERROR("Couldn't find shader: {}", path.u8string());
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
			EG_RENDERER_ERROR("Invalid shader extension. Couldn't deduce its type: {}", path.u8string());
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
		ShaderManager::Add(result);
		return result;
	}

	void ShaderManager::Init()
	{
		s_bGame = Application::Get().IsGame();
		if (s_bGame)
			return;

		m_ShadersSourceCode = Utils::LoadShadersSourceCode();
	}

	void ShaderManager::InitGame(const YAML::Node& shadersNode)
	{
		s_bGame = Application::Get().IsGame();
		if (!s_bGame)
			return;

		if (!shadersNode)
		{
			EG_CORE_CRITICAL("Failed to load shaders");
			exit(-1);
		}

		for (auto& baseShaderNode : shadersNode)
		{
			Path path = baseShaderNode["Path"].as<std::string>();
			std::string sourceCode = baseShaderNode["SourceCode"].as<std::string>();

			m_ShadersSourceCode.emplace(std::move(path), std::move(sourceCode));
		}
	}

	void ShaderManager::Add(const Ref<Shader>& shader)
	{
		const Path filepath = shader->GetPath();
		m_Shaders.push_back(shader);
		m_ShadersByPath[filepath] = shader;
	}

	bool ShaderManager::Exists(const Path& filepath)
	{
		return m_ShadersByPath.find((filepath)) != m_ShadersByPath.end();
	}

	const std::string& ShaderManager::GetSource(const Path& filepath)
	{
		static const std::string empty;
		auto it = m_ShadersSourceCode.find(filepath);
		return it != m_ShadersSourceCode.end() ? it->second : empty;
	}

	void ShaderManager::BuildShaderPack(YAML::Emitter& out)
	{
		// Get fresh sources
		auto sourceCodes = Utils::LoadShadersSourceCode();
		
		out << YAML::Key << "Shaders" << YAML::Value << YAML::BeginSeq;

		for (const auto& [path, source] : sourceCodes)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << path.string();
			out << YAML::Key << "SourceCode" << YAML::Value << source;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	void ShaderManager::ReloadAllShaders()
	{
		if (s_bGame)
			return;

		m_ShadersSourceCode = Utils::LoadShadersSourceCode();

		std::vector<Ref<Shader>> shaders;
		shaders.reserve(m_Shaders.size());
		m_ShadersByPath.clear();
		
		// Removing unused shaders
		for (auto& shader : m_Shaders)
		{
			if (shader.use_count() != 1)
			{
				const auto& insertedShader = shaders.emplace_back(std::move(shader));
				m_ShadersByPath[insertedShader->GetPath()] = insertedShader;
			}
		}
		m_Shaders = std::move(shaders);

		RenderManager::Submit([shaders = m_Shaders](Ref<CommandBuffer>&)
		{
			for (auto& shader : shaders)
				shader->Reload();
		});
	}
}