#include "egpch.h"
#include "VulkanShader.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Core/Project.h"

#include "shaderc/shaderc.hpp"
#include "spirv-tools/libspirv.h"
#include "spirv_cross/spirv_glsl.hpp"

#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Timer.h"

namespace Eagle
{
	static constexpr const char* s_EntryPoint = "main";
	static constexpr const char* s_ShaderVersion = "#version 460\n";

#ifdef EG_RELEASE
	static constexpr bool s_bGenerateDebugInfo = false;
#else
	static constexpr bool s_bGenerateDebugInfo = true;
#endif

	namespace Utils
	{
		static shaderc_env_version GetShaderCVersion()
		{
			switch (VulkanContext::GetVulkanAPIVersion())
			{
			case VK_API_VERSION_1_0: return shaderc_env_version_vulkan_1_0;
			case VK_API_VERSION_1_1: return shaderc_env_version_vulkan_1_1;
			case VK_API_VERSION_1_2: return shaderc_env_version_vulkan_1_2;
			case VK_API_VERSION_1_3: return shaderc_env_version_vulkan_1_3;
			}
			return shaderc_env_version_vulkan_1_0;
		}

		static shaderc_shader_kind VkShaderStageToShaderC(VkShaderStageFlagBits stage)
		{
			switch (stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:    return shaderc_vertex_shader;
			case VK_SHADER_STAGE_FRAGMENT_BIT:  return shaderc_fragment_shader;
			case VK_SHADER_STAGE_COMPUTE_BIT:   return shaderc_compute_shader;
			case VK_SHADER_STAGE_GEOMETRY_BIT:   return shaderc_geometry_shader;
			}
			EG_CORE_ASSERT(false);
			return (shaderc_shader_kind)0;
		}

		static shaderc_shader_kind ShaderTypeToShaderC(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::Vertex:    return shaderc_vertex_shader;
			case ShaderType::Fragment:  return shaderc_fragment_shader;
			case ShaderType::Geometry:  return shaderc_geometry_shader;
			case ShaderType::Compute:   return shaderc_compute_shader;
			}
			EG_CORE_ASSERT(false);
			return (shaderc_shader_kind)0;
		}

		static VkFormat GLSLTypeToVulkan(const spirv_cross::SPIRType& type)
		{
			if (type.basetype == spirv_cross::SPIRType::Float)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R32_SFLOAT;
				case 2: return VK_FORMAT_R32G32_SFLOAT;
				case 3: return VK_FORMAT_R32G32B32_SFLOAT;
				case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
				}
			}
			else if (type.basetype == spirv_cross::SPIRType::Int)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R32_SINT;
				case 2: return VK_FORMAT_R32G32_SINT;
				case 3: return VK_FORMAT_R32G32B32_SINT;
				case 4: return VK_FORMAT_R32G32B32A32_SINT;
				}
			}
			else if (type.basetype == spirv_cross::SPIRType::UInt)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R32_UINT;
				case 2: return VK_FORMAT_R32G32_UINT;
				case 3: return VK_FORMAT_R32G32B32_UINT;
				case 4: return VK_FORMAT_R32G32B32A32_UINT;
				}
			}
			else if (type.basetype == spirv_cross::SPIRType::Half)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R16_SFLOAT;
				case 2: return VK_FORMAT_R16G16_SFLOAT;
				case 3: return VK_FORMAT_R16G16B16_SFLOAT;
				case 4: return VK_FORMAT_R16G16B16A16_SFLOAT;
				}
			}
			else if (type.basetype == spirv_cross::SPIRType::UShort)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R16_UINT;
				case 2: return VK_FORMAT_R16G16_UINT;
				case 3: return VK_FORMAT_R16G16B16_UINT;
				case 4: return VK_FORMAT_R16G16B16A16_UINT;
				}
			}
			EG_CORE_ASSERT(!"Unknown type");
			return VK_FORMAT_UNDEFINED;
		}

		struct GlslResource
		{
			const spirv_cross::Resource* Resource = nullptr;
			uint32_t Binding = 0;
			VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
			bool bBindless = false;

			bool operator< (const GlslResource& other) const
			{
				return Binding < other.Binding;
			}
		};

		// Set -> Resource
		static std::map<uint32_t, std::set<GlslResource>> GetDescriptorSets(const spirv_cross::Compiler& glsl, const spirv_cross::ShaderResources& resources)
		{
			std::map<uint32_t, std::set<GlslResource>> sets;

			for (auto& resource : resources.uniform_buffers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER });
			}
			for (auto& resource : resources.storage_buffers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
			}
			for (auto& resource : resources.storage_images)
			{
				auto type = glsl.get_type(resource.type_id);
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				if (type.image.dim == spv::Dim::DimBuffer)
					sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER });
				else
					sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE });
			}
			for (auto& resource : resources.sampled_images)
			{
				auto type = glsl.get_type(resource.type_id);
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				
				const bool bBindless = type.array.size() > 0 && type.array[0] == 0;
				sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bBindless });
			}
			for (auto& resource : resources.separate_images)
			{
				auto type = glsl.get_type(resource.type_id);
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				if (type.image.dim == spv::Dim::DimBuffer)
					sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER });
				else
					sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
			}
			for (auto& resource : resources.separate_samplers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_SAMPLER });
			}
			for (auto& resource : resources.acceleration_structures)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR });
			}

			return sets;
		}
	}

	static Path GetShaderCacheDir()
	{
		return Project::GetRendererCachePath() / "Shaders/Vulkan";
	}

	VulkanShader::VulkanShader(const Path& path, ShaderType shaderType, const ShaderDefines& defines)
		: Shader(path, shaderType, defines)
	{
		// If new binary was loaded
		if (LoadBinary(false))
		{
			Reflect(m_Binary);
			CreateShaderModule();
		}
	}

	VulkanShader::~VulkanShader()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		if (m_ShaderModule)
		{
			vkDestroyShaderModule(device, m_ShaderModule, nullptr);
			m_ShaderModule = VK_NULL_HANDLE;
		}
	}

	void VulkanShader::Reload()
	{
		ReloadInternal(false);
	}

	// Resolves `#include`s for shaderc. Includes are resolved by the compiler's preprocessor.
	// Each file is included only once per shader
	class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
	{
	public:
		// @outIncludedFiles. Optional. Receives every file that was included in the order of inclusion
		ShaderIncluder(std::vector<std::string>* outIncludedFiles = nullptr)
			: m_IncludedFiles(outIncludedFiles) {}

		shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type, const char* requestingSource, size_t includeDepth) override
		{
			IncludeData* data = new IncludeData();
			data->Name = requestedSource;

			const Path filename = requestedSource;
			const bool bFirstInclude = std::find(m_IncludedPaths.begin(), m_IncludedPaths.end(), filename) == m_IncludedPaths.end();
			if (bFirstInclude)
			{
				const std::string* source = ShaderManager::FindSource(filename);
				if (!source)
				{
					data->Content = "Shader file not found: " + data->Name;
					data->Name.clear();
				}
				else
				{
					m_IncludedPaths.push_back(filename);
					data->Content = *source;
					if (m_IncludedFiles)
						m_IncludedFiles->push_back(data->Name);
				}
			}

			data->Result.source_name = data->Name.c_str();
			data->Result.source_name_length = data->Name.size();
			data->Result.content = data->Content.c_str();
			data->Result.content_length = data->Content.size();
			data->Result.user_data = data;
			return &data->Result;
		}

		void ReleaseInclude(shaderc_include_result* result) override
		{
			delete (IncludeData*)result->user_data;
		}

	private:
		struct IncludeData
		{
			shaderc_include_result Result{};
			std::string Name;
			std::string Content;
		};

		std::vector<Path> m_IncludedPaths;
		std::vector<std::string>* m_IncludedFiles = nullptr;
	};

	// It's safe to use it from multiple threads
	static shaderc::Compiler& GetShaderCompiler()
	{
		static shaderc::Compiler compiler;
		return compiler;
	}

	// So that build with different settings doesn't load binaries from the cache that were compiled differently
	static const std::string& GetCompileSettingsKey()
	{
		static const std::string key = "VulkanEnv=" + std::to_string((int)Utils::GetShaderCVersion()) +
			";DebugInfo=" + std::to_string((int)s_bGenerateDebugInfo) + '\n';
		return key;
	}

	static shaderc::CompileOptions CreateShaderCompileOptions(std::vector<std::string>* outIncludedFiles = nullptr)
	{
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, Utils::GetShaderCVersion());
		options.SetWarningsAsErrors();
		if constexpr (s_bGenerateDebugInfo)
			options.SetGenerateDebugInfo();
		options.SetIncluder(std::make_unique<ShaderIncluder>(outIncludedFiles));
		return options;
	}

	// Hash of the shader and everything it includes, so that a change in any header changes the hash too (which invalidates the cache and reloads the shader).
	static size_t HashShaderSource(const std::string& source, const std::vector<std::string>& includedFiles)
	{
		std::string hashSource = GetCompileSettingsKey();
		hashSource += source;
		for (const auto& file : includedFiles)
		{
			hashSource += '\0';
			hashSource += file;
			hashSource += '\0';
			// A deleted file must change the hash even if it was empty, so that the next reload reports the missing include.
			if (const std::string* fileSource = ShaderManager::FindSource(Path(file)))
				hashSource += *fileSource;
			else
				hashSource += '\0';
		}
		return std::hash<std::string>()(hashSource);
	}

	static bool ReadShaderCache(const Path& cacheFilePath, std::vector<uint32_t>& outBinary)
	{
		if (!std::filesystem::exists(cacheFilePath))
			return false;

		std::ifstream cacheFin(cacheFilePath, std::ios_base::binary);
		std::stringstream cacheBuffer;
		cacheBuffer << cacheFin.rdbuf();
		cacheFin.close();
		std::string cacheBinaryStr = cacheBuffer.str();

		size_t strBinarySize = cacheBinaryStr.size();
		outBinary.clear();
		// Binary has uint32 type and string is char. So we divide caches size by 4
		outBinary.reserve(strBinarySize / 4);
		for (size_t i = 0; i < strBinarySize; i += 4)
			outBinary.push_back(*(uint32_t*)&cacheBinaryStr[i]); // Reading 4 chars as uint32_t
		return true;
	}

	// The list of included files is cached next to the binaries, so that loading a shader from the cache doesn't need to run the preprocessor
	static bool ReadIncludedFilesCache(const Path& path, std::vector<std::string>& outIncludedFiles)
	{
		std::ifstream fin(path);
		if (!fin)
			return false;

		outIncludedFiles.clear();
		std::string line;
		while (std::getline(fin, line))
			if (!line.empty())
				outIncludedFiles.push_back(std::move(line));
		return true;
	}

	static void WriteIncludedFilesCache(const Path& path, const std::vector<std::string>& includedFiles)
	{
		std::ofstream fout(path, std::ios_base::out | std::ios_base::trunc);
		for (const auto& file : includedFiles)
			fout << file << '\n';
	}

	bool VulkanShader::LoadBinary(bool bFromDefines)
	{
		Timer timer;

		m_DefinesSource.clear();
		for (auto& define : m_Defines)
			m_DefinesSource += "#define " + define.first + ' ' + define.second + '\n';

		switch (m_ShaderType)
		{
			case ShaderType::Vertex:
				m_DefinesSource += "#define EG_SHADER_VERTEX\n";
				break;
			case ShaderType::Fragment:
				m_DefinesSource += "#define EG_SHADER_FRAGMENT\n";
				break;
			case ShaderType::Geometry:
				m_DefinesSource += "#define EG_SHADER_GEOMETRY\n";
				break;
			case ShaderType::Compute:
				m_DefinesSource += "#define EG_SHADER_COMPUTE\n";
				break;
		}

		if (!bFromDefines)
			m_Source = ShaderManager::GetSource(m_Path);

		const std::string source = s_ShaderVersion + m_DefinesSource + m_Source;
		const std::string shaderName = Utils::AsString(m_Path);
		const std::string fileName = Utils::AsString(m_Path.filename());
		const shaderc_shader_kind shaderKind = Utils::ShaderTypeToShaderC(m_ShaderType);
		const Path cachePath = GetShaderCacheDir();
		auto getCacheFilePath = [&](size_t sourceHash) { return cachePath / Utils::AsPath(fileName + "_" + std::to_string(sourceHash) + ".bin"); };
		// Keyed by the hash of the source without includes
		const Path includedFilesCachePath = cachePath / Utils::AsPath(fileName + "_" + std::to_string(std::hash<std::string>()(source)) + ".includes");

		// 1) Fast path. Using the files that were included the last time (by this shader, or by any shader with the same source & defines),
		// without running the preprocessor. If the hash matches the current binary, nothing has changed.
		// If there's a binary for this hash in the cache, it's up to date too
		{
			std::vector<std::string> includedFiles;
			bool bHasIncludedFiles = false;
			if (m_SourceHash != 0 && source == m_LoadedSource)
			{
				includedFiles = m_IncludedFiles;
				bHasIncludedFiles = true;
			}
			else
				bHasIncludedFiles = ReadIncludedFilesCache(includedFilesCachePath, includedFiles);

			if (bHasIncludedFiles)
			{
				const size_t sourceHash = HashShaderSource(source, includedFiles);
				if (sourceHash == m_SourceHash)
					return false; // Nothing has changed

				if (ReadShaderCache(getCacheFilePath(sourceHash), m_Binary))
				{
					m_SourceHash = sourceHash;
					m_LoadedSource = source;
					m_IncludedFiles = std::move(includedFiles);
					return true;
				}
			}
		}

		// 2) Something has changed (or it's the first time this shader is loaded)
		shaderc::Compiler& compiler = GetShaderCompiler();
		auto onFailed = [&](const std::string& failedSource, const std::string& error)
		{
			// So that the next reload tries again (and reports the error again) even if nothing changes
			m_SourceHash = 0;

			EG_RENDERER_CRITICAL("Failed to compile shader at: {0}", m_Path);
			if (!std::filesystem::exists(cachePath))
				std::filesystem::create_directories(cachePath);

			Path filePath = cachePath / Utils::AsPath(fileName + "_failed.txt");
			std::ofstream fout(filePath);
			if (fout)
			{
				fout << failedSource;
				fout.close();
				EG_RENDERER_TRACE("Outputing shader to: {}", filePath);
			}
			EG_RENDERER_TRACE("Error: \n{0}", error);
		};

		std::vector<std::string> includedFiles;
		shaderc::PreprocessedSourceCompilationResult preprocessed = compiler.PreprocessGlsl(source, shaderKind, shaderName.c_str(), CreateShaderCompileOptions(&includedFiles));
		if (preprocessed.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			onFailed(source, preprocessed.GetErrorMessage());
			return false;
		}

		const size_t sourceHash = HashShaderSource(source, includedFiles);
		const bool bReloaded = m_SourceHash != sourceHash;

		if (!std::filesystem::exists(cachePath))
			std::filesystem::create_directories(cachePath);

		if (bReloaded)
		{
			// 3) Trying to find a cache for the shader
			const Path cacheFilePath = getCacheFilePath(sourceHash);
			if (!ReadShaderCache(cacheFilePath, m_Binary))
			{
				// 4) Compile
				EG_RENDERER_TRACE("Compiling shader: {}", m_Path);
				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, shaderKind, shaderName.c_str(), CreateShaderCompileOptions());
				if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					// Outputing the preprocessed source, since it has all the includes expanded
					onFailed(std::string(preprocessed.begin(), preprocessed.end()), module.GetErrorMessage());
					return false;
				}

				m_Binary = std::vector<uint32_t>(module.begin(), module.end());

				// 5) Write to cache
				std::ofstream out(cacheFilePath, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
				out.write((const char*)m_Binary.data(), m_Binary.size() * sizeof(uint32_t));
				out.close();
				EG_RENDERER_TRACE("Compilation took {} ms", timer.GetMilliseconds());
			}
		}

		WriteIncludedFilesCache(includedFilesCachePath, includedFiles);
		m_SourceHash = sourceHash;
		m_LoadedSource = source;
		m_IncludedFiles = std::move(includedFiles);

		return bReloaded;
	}

	void VulkanShader::Reflect(const std::vector<uint32_t>& binary)
	{
		spirv_cross::Compiler glsl(binary);
		spirv_cross::ShaderResources resources = glsl.get_shader_resources();
		VkShaderStageFlags vulkanShaderType = ShaderTypeToVulkan(m_ShaderType);

		m_VertexAttribs.clear();
		m_LayoutBindings.clear();
		m_PushConstantRanges.clear();

		// Reflect vertex inputs
		if (m_ShaderType == ShaderType::Vertex)
		{
			for (auto& input : resources.stage_inputs)
			{
				spirv_cross::SPIRType type = glsl.get_type(input.type_id);
				assert(type.width % 8 == 0);

				uint32_t size = (type.width / 8) * type.vecsize;

				uint32_t location = glsl.get_decoration(input.id, spv::DecorationLocation);
				VkFormat vkType = Utils::GLSLTypeToVulkan(type);
				for (uint32_t i = 0; i < type.columns; ++i)
				{
					auto& attrib = m_VertexAttribs.emplace_back();
					attrib.binding = 0;
					attrib.location = location + i;
					attrib.offset = size;
					attrib.format = vkType;
				}
			}
			std::sort(m_VertexAttribs.begin(), m_VertexAttribs.end(), [](const auto& a, const auto& b) {
					return a.location < b.location;
				});
		}

		// Reflect descriptor sets
		auto sets = Utils::GetDescriptorSets(glsl, resources);
		size_t setsCount = sets.size();
		if (setsCount > 0)
		{
			m_LayoutBindings.resize((--sets.end())->first + 1); // Extracting max value and resizing array
			for (auto& set : sets)
			{
				const uint32_t setIndex = set.first;
				for (auto& resourceBinding : set.second)
				{
					auto& resource = resourceBinding.Resource;
					spirv_cross::SPIRType type = glsl.get_type(resource->type_id);

					auto& setData = m_LayoutBindings[setIndex];
					setData.bBindless |= resourceBinding.bBindless;

					auto& binding = setData.Bindings.emplace_back();
					binding.binding = resourceBinding.Binding;
					binding.descriptorCount = type.array.empty() ? 1u : type.array[0];
					binding.pImmutableSamplers = nullptr;
					binding.descriptorType = resourceBinding.DescriptorType;
					binding.stageFlags = vulkanShaderType;

					auto& bindingFlags = setData.BindingsFlags.emplace_back(0u);
					if (resourceBinding.bBindless)
						bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

					if (binding.descriptorCount == 0) // if it's a bindless resource, set descriptor count to max
						binding.descriptorCount = RendererConfig::MaxTextures;
				}
			}
		}

		// Reflect push constants
		if (!resources.push_constant_buffers.empty())
		{
			const auto& push_constant = resources.push_constant_buffers.front();

			PushConstantRange& pushConstantRange = m_PushConstantRanges.emplace_back();
			pushConstantRange.ShaderStage = m_ShaderType;

			const spirv_cross::SPIRType& type =
				glsl.get_type(resources.push_constant_buffers.front().base_type_id);
			pushConstantRange.Size = std::uint32_t(glsl.get_declared_struct_size(type));
			pushConstantRange.Offset =
				glsl.get_decoration(resources.push_constant_buffers.front().id, spv::DecorationOffset);
		}
	
		if (m_ShaderType == ShaderType::Compute)
		{
			const spirv_cross::SPIREntryPoint& entry = glsl.get_entry_point(s_EntryPoint, spv::ExecutionModelGLCompute);
			m_WorkGroupSize = glm::uvec3(
				entry.workgroup_size.x,
				entry.workgroup_size.y,
				entry.workgroup_size.z
			);
		}
	}

	void VulkanShader::ReloadInternal(bool bFromDefines)
	{
		// If new binary was loaded
		if (LoadBinary(bFromDefines))
		{
			Reflect(m_Binary);
			CreateShaderModule();
			OnReloaded();
			RenderManager::OnShaderReloaded(this);
		}
	}

	void VulkanShader::CreateShaderModule()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		if (m_ShaderModule)
		{
			vkDestroyShaderModule(device, m_ShaderModule, nullptr);
			m_ShaderModule = VK_NULL_HANDLE;
		}

		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = m_Binary.size() * sizeof(uint32_t);
		ci.pCode = m_Binary.data();

		VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &m_ShaderModule));

		VkPipelineShaderStageCreateInfo& shaderStage = m_PipelineShaderStageCI;
		shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.module = m_ShaderModule;
		shaderStage.pName = s_EntryPoint;
		shaderStage.stage = ShaderTypeToVulkan(m_ShaderType);
	}

}
