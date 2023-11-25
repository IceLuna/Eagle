#pragma once

#include "Eagle/Renderer/VidWrappers/Shader.h"
#include "Vulkan.h"
#include "VulkanAllocator.h"

namespace Eagle
{
	struct LayoutSetData
	{
		std::vector<VkDescriptorSetLayoutBinding> Bindings{};
		std::vector<VkDescriptorBindingFlags> BindingsFlags{};
		bool bBindless = false;
	};

	class VulkanShader : public Shader
	{
	public:
		VulkanShader(const std::filesystem::path& path, ShaderType shaderType, const ShaderDefines& defines = {});
		virtual ~VulkanShader();

		const VkPipelineShaderStageCreateInfo& GetPipelineShaderStageInfo() const { return m_PipelineShaderStageCI; }
		const std::vector<VkVertexInputAttributeDescription>& GetInputAttribs() const { return m_VertexAttribs; }

		const std::vector<LayoutSetData>& GetLayoutSetBindings() const { return m_LayoutBindings; }

		void Reload() override;
		void SetDefines(const ShaderDefines& defines) override
		{
			m_Defines = defines;
			ReloadInternal(true);
		}

	private:
		// `LoadBinary` Returns true if was reloaded
		bool LoadBinary(bool bFromDefines);

		void CreateShaderModule();
		void Reflect(const std::vector<uint32_t>& binary);

		// if bFromDefines is true, that means we just changed defines.
		// So no need to parse all files & includes
		// We can just update defines & compile
		void ReloadInternal(bool bFromDefines);

	private:
		std::vector<VkVertexInputAttributeDescription> m_VertexAttribs;
		std::vector<LayoutSetData> m_LayoutBindings; // Set -> Bindings
		std::vector<uint32_t> m_Binary;
		VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
		VkPipelineShaderStageCreateInfo m_PipelineShaderStageCI;
		size_t m_SourceHash = 0;

		std::string m_Source; // Source of the shader without passed defines
		std::string m_DefinesSource;

		static constexpr const char* s_ShaderVersion = "#version 450\n";
	};
}
