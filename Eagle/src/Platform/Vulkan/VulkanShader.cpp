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

namespace Eagle
{
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
			if (type.basetype == spirv_cross::SPIRType::Int)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R32_SINT;
				case 2: return VK_FORMAT_R32G32_SINT;
				case 3: return VK_FORMAT_R32G32B32_SINT;
				case 4: return VK_FORMAT_R32G32B32A32_SINT;
				}
			}
			if (type.basetype == spirv_cross::SPIRType::UInt)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R32_UINT;
				case 2: return VK_FORMAT_R32G32_UINT;
				case 3: return VK_FORMAT_R32G32B32_UINT;
				case 4: return VK_FORMAT_R32G32B32A32_UINT;
				}
			}
			if (type.basetype == spirv_cross::SPIRType::Half)
			{
				switch (type.vecsize)
				{
				case 1: return VK_FORMAT_R16_SFLOAT;
				case 2: return VK_FORMAT_R16G16_SFLOAT;
				case 3: return VK_FORMAT_R16G16B16_SFLOAT;
				case 4: return VK_FORMAT_R16G16B16A16_SFLOAT;
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
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });
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

	VulkanShader::VulkanShader(const std::filesystem::path& path, ShaderType shaderType, const ShaderDefines& defines)
		: Shader(path, shaderType, defines)
	{
		m_Hash = std::hash<Path>()(path);
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

	static void ParseIncludes(std::string& source)
	{
		const Path shadersFolder = "../Eagle-Editor/assets/shaders"; // TODO: Fix path
		const std::string include = "#include ";
		const size_t includeSize = include.size();
		std::stringstream buffer;
		std::vector<Path> includedPaths;
		includedPaths.reserve(10);

		size_t pos = source.find(include);
		while (pos != std::string::npos)
		{
			size_t endLinePos = source.find_first_of('\n', pos);

			size_t offset = pos + includeSize + 1;
			std::string filename = source.substr(offset, endLinePos - offset - 1);
			const Path path = shadersFolder / filename;
			buffer << "#line 1 // Include file: " << filename << '\n';

			if (std::find(includedPaths.begin(), includedPaths.end(), path) == includedPaths.end())
			{
				includedPaths.push_back(path);
				std::ifstream fin(path);
				if (!fin)
				{
					EG_RENDERER_CRITICAL("Failed to open shader file: {0}", path);
					return;
				}
				// Adding defines and reading shader code from the file
				buffer << fin.rdbuf();
				fin.close();
			}

			std::string includeSource = buffer.str();
			source.replace(pos, endLinePos - pos + 1, includeSource);

			buffer.str(std::string(""));
			buffer.clear();
			pos = source.find("#include ");
		}
		buffer << "#line 1 // Main file\n";
	}

	bool VulkanShader::LoadBinary(bool bFromDefines)
	{
		auto start = std::chrono::high_resolution_clock::now();

		m_DefinesSource.clear();
		for (auto& define : m_Defines)
			m_DefinesSource += "#define " + define.first + ' ' + define.second + '\n';

		if (!bFromDefines)
		{
			std::ifstream fin(m_Path);
			if (!fin)
			{
				EG_RENDERER_CRITICAL("Failed to open shader file: {0}", m_Path);
				return false;
			}
			std::stringstream buffer;
			buffer << fin.rdbuf();
			fin.close();

			m_Source = buffer.str();
			ParseIncludes(m_Source);
		}

		std::string source = s_ShaderVersion + m_DefinesSource + m_Source;
		const size_t sourceHash = std::hash<std::string>()(source);
		bool bReloaded = m_SourceHash != sourceHash;
		m_SourceHash = sourceHash;

		// Trying to find a cache for the shader
		Path cachePath = GetShaderCacheDir();
		Path cacheFilePath = cachePath / (m_Path.filename().u8string() + "_" + std::to_string(sourceHash) + ".bin");
		bool bLoadedFromCache = false;
		if (std::filesystem::exists(cacheFilePath))
		{
			bLoadedFromCache = true;
			std::ifstream cacheFin(cacheFilePath, std::ios_base::binary);
			std::stringstream cacheBuffer;
			cacheBuffer << cacheFin.rdbuf();
			cacheFin.close();
			std::string cacheBinaryStr = cacheBuffer.str();

			size_t strBinarySize = cacheBinaryStr.size();
			m_Binary.clear();
			// Binary has uint32 type and string is char. So we divide caches size by 4
			m_Binary.reserve(strBinarySize / 4);
			for (size_t i = 0; i < strBinarySize; i += 4)
				m_Binary.push_back(*(uint32_t*)&cacheBinaryStr[i]); // Reading 4 chars as uint32_t
		}

		if (!bLoadedFromCache)
		{
			if (!std::filesystem::exists(cachePath))
				std::filesystem::create_directories(cachePath);

			// 1) Compile
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, Utils::GetShaderCVersion());
			options.SetWarningsAsErrors();
			options.SetGenerateDebugInfo();

			if (!std::filesystem::exists(m_Path))
			{
				EG_CORE_CRITICAL("Shader does not exist: {}", m_Path);
				EG_CORE_ASSERT(false);
			}

			EG_RENDERER_TRACE("Compiling shader: {}", m_Path);
			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::ShaderTypeToShaderC(m_ShaderType), m_Path.u8string().c_str(), options);
			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				EG_RENDERER_CRITICAL("Failed to compile shader at: {0}", m_Path);
				Path filePath = cachePath / (m_Path.filename().u8string() + "_failed.txt");
				std::ofstream fout(filePath);
				if (fout)
				{
					fout << source;
					fout.close();
					EG_RENDERER_TRACE("Outputing shader to: {}", filePath);
				}
				EG_RENDERER_TRACE("Error: \n{0}", module.GetErrorMessage());
				return false;
			}

			m_Binary = std::vector<uint32_t>(module.begin(), module.end());

			// 2) Write to cache
			std::ofstream out(cacheFilePath, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
			out.write((const char*)m_Binary.data(), m_Binary.size() * sizeof(uint32_t));
			out.close();
			EG_RENDERER_TRACE("Compilation took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count());
		}

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

					auto& binding = m_LayoutBindings[setIndex].emplace_back();
					binding.binding = resourceBinding.Binding;
					binding.descriptorCount = type.array.empty() ? 1u : type.array[0]; // TODO: if descriptorCount == 0, assign unbound array size
					binding.pImmutableSamplers = nullptr;
					binding.descriptorType = resourceBinding.DescriptorType;
					binding.stageFlags = vulkanShaderType;
				}
			}
		}

		// Reflect push constants
		if (!resources.push_constant_buffers.empty())
		{
			auto ranges = glsl.get_active_buffer_ranges(resources.push_constant_buffers.front().id);

			PushConstantRange& pushConstantRange = m_PushConstantRanges.emplace_back();
			pushConstantRange.ShaderStage = m_ShaderType;

			for (auto& range : ranges)
				pushConstantRange.Size = std::max(pushConstantRange.Size, uint32_t(range.offset + range.range));
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
			RenderManager::OnShaderReloaded(shared_from_this());
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
		shaderStage.pName = "main";
		shaderStage.stage = ShaderTypeToVulkan(m_ShaderType);
	}

}
