#pragma once

#include "Eagle/Renderer/Shader.h"
#include "Vulkan.h"
#include "VulkanAllocator.h"

namespace Eagle
{
	class VulkanShader : public Shader
	{
	public:
		VulkanShader(const std::filesystem::path& path, ShaderType shaderType, const ShaderDefines& defines = {});
		virtual ~VulkanShader();

		const VkPipelineShaderStageCreateInfo& GetPipelineShaderStageInfo() const { return m_PipelineShaderStageCI; }
		const std::vector<VkVertexInputAttributeDescription>& GetInputAttribs() const { return m_VertexAttribs; }

		const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& GetLayoutSetBindings() const { return m_LayoutBindings; }

		void Reload() override;

	private:
		// `LoadBinary` Returns true if was reloaded
		bool LoadBinary();
		void CreateShaderModule();
		void Reflect(const std::vector<uint32_t>& binary);

	private:
		std::vector<VkVertexInputAttributeDescription> m_VertexAttribs;
		std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_LayoutBindings; // Set -> Bindings
		std::vector<uint32_t> m_Binary;
		VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
		VkPipelineShaderStageCreateInfo m_PipelineShaderStageCI;

		static constexpr const char* s_ShaderVersion = "#version 450";
	};
}
