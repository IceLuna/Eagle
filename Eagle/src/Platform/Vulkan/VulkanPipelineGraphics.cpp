#include "egpch.h"
#include "VulkanPipelineGraphics.h"
#include "VulkanContext.h"
#include "VulkanShader.h"
#include "VulkanUtils.h"
#include "VulkanPipelineCache.h"

namespace Eagle
{
	static const VkPipelineDepthStencilStateCreateInfo s_DefaultDepthStencilCI
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // sType
		nullptr, // pNext
		0, // flags
		VK_FALSE, // depthTestEnable
		VK_FALSE, // depthWriteEnable
		VK_COMPARE_OP_LESS_OR_EQUAL, // depthCompareOp
		VK_FALSE, // depthBoundsTestEnable
		VK_FALSE, // stencilTestEnable
		{}, // front
		{},	// back
		0.f, // minDepthBounds
		0.f	 // maxDepthBounds
	};

	static void MergeDescriptorSetLayoutBindings(std::vector<VkDescriptorSetLayoutBinding>& dstBindings,
		const std::vector<VkDescriptorSetLayoutBinding>& srcBindings)
	{
		std::vector<VkDescriptorSetLayoutBinding> newBindings;
		newBindings.reserve(srcBindings.size());

		auto cmp = [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
		{
			return a.binding < b.binding;
		};

		auto dstIt = dstBindings.begin();
		for (auto& srcBinding : srcBindings)
		{
			dstIt = std::lower_bound(dstIt, dstBindings.end(), srcBinding, cmp);

			if (dstIt == dstBindings.end() || dstIt->binding > srcBinding.binding)
				newBindings.push_back(srcBinding);
			else
				dstIt->stageFlags |= srcBinding.stageFlags;
		}

		dstBindings.insert(dstBindings.end(), newBindings.begin(), newBindings.end());
		std::sort(dstBindings.begin(), dstBindings.end(), cmp);
	}

	static void MergeDescriptorSetLayoutBindings(std::vector<std::vector<VkDescriptorSetLayoutBinding>>& dstSetBindings,
		const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& srcSetBindings)
	{
		if (srcSetBindings.size() > dstSetBindings.size())
			dstSetBindings.resize(srcSetBindings.size());

		for (std::size_t i = 0; i < srcSetBindings.size(); i++)
			MergeDescriptorSetLayoutBindings(dstSetBindings[i], srcSetBindings[i]);
	}

	VulkanPipelineGraphics::VulkanPipelineGraphics(const PipelineGraphicsState& state, const Ref<PipelineGraphics>& parentPipeline)
		: PipelineGraphics(state)
	{
		VkPipeline vkParentPipeline = parentPipeline ? (VkPipeline)parentPipeline->GetPipelineHandle() : VK_NULL_HANDLE;
		Create(vkParentPipeline);
	}

	VulkanPipelineGraphics::~VulkanPipelineGraphics()
	{
		Renderer::SubmitResourceFree([pipeline = m_GraphicsPipeline, renderPass = m_RenderPass, fb = m_Framebuffer, pipelineLayout = m_PipelineLayout]()
		{
			VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyRenderPass(device, renderPass, nullptr);
			vkDestroyFramebuffer(device, fb, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		});
	}

	void VulkanPipelineGraphics::Recreate()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		VkPipeline prevPipeline = m_GraphicsPipeline;
		VkRenderPass prevRenderPass = m_RenderPass;
		VkFramebuffer prevFramebuffer = m_Framebuffer;
		VkPipelineLayout prevPipelineLayout = m_PipelineLayout;

		Create(prevPipeline);
		Renderer::SubmitResourceFree([device, pipeline = prevPipeline, rp = prevRenderPass, fb = prevFramebuffer, pipLayout = prevPipelineLayout]()
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyRenderPass(device, rp, nullptr);
			vkDestroyFramebuffer(device, fb, nullptr);
			vkDestroyPipelineLayout(device, pipLayout, nullptr);
		});
	}

	void VulkanPipelineGraphics::Create(VkPipeline parentPipeline)
	{
		EG_CORE_ASSERT(m_State.VertexShader->GetType() == ShaderType::Vertex);
		EG_CORE_ASSERT(!m_State.FragmentShader || (m_State.FragmentShader->GetType() == ShaderType::Fragment));
		EG_CORE_ASSERT(!m_State.GeometryShader || (m_State.GeometryShader->GetType() == ShaderType::Geometry));
		m_DescriptorSets.clear();

		// Mark each descriptor as dirty so that there's no need to call
		// `pipeline->Set*` (for example, pipeline->SetBuffer) after pipeline reloading
		for (auto& data : m_DescriptorSetData)
		{
			data.second.MakeDirty();
		}

		m_Width = m_State.Size.x;
		m_Height = m_State.Size.y;

		Ref<VulkanShader> vertexShader = Cast<VulkanShader>(m_State.VertexShader);
		Ref<VulkanShader> fragmentShader = m_State.FragmentShader ? Cast<VulkanShader>(m_State.FragmentShader) : nullptr;
		Ref<VulkanShader> geometryShader = m_State.GeometryShader ? Cast<VulkanShader>(m_State.GeometryShader) : nullptr;

		const VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		const size_t colorAttachmentsCount = m_State.ColorAttachments.size();
		const bool bDeviceSupportsConservativeRasterization = VulkanContext::GetDevice()->GetPhysicalDevice()->GetExtensionSupport().SupportsConservativeRasterization;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = TopologyToVulkan(m_State.Topology);

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.scissorCount = 1;
		viewportState.viewportCount = 1;

		VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeRasterizationCI{};
		conservativeRasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
		conservativeRasterizationCI.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT; // TODO: Test different modes

		VkPipelineRasterizationStateCreateInfo rasterization{};
		rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization.rasterizerDiscardEnable = VK_FALSE; // No geometry passes rasterization stage if set to TRUE
		rasterization.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization.lineWidth = m_State.LineWidth;
		rasterization.cullMode = CullModeToVulkan(m_State.CullMode);
		rasterization.frontFace = FrontFaceToVulkan(m_State.FrontFace);
		rasterization.pNext = (m_State.bEnableConservativeRasterization && bDeviceSupportsConservativeRasterization) ? &conservativeRasterizationCI : nullptr;

		if (m_State.bEnableConservativeRasterization && !bDeviceSupportsConservativeRasterization)
			EG_RENDERER_WARN("Conservation rasterization was requested but device doesn't support it. So it was not enabled");

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = GetVulkanSamplesCount(m_State.GetSamplesCount());

		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(colorAttachmentsCount);
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		for (size_t i = 0; i < colorAttachmentsCount; ++i)
		{
			colorBlendAttachment.blendEnable = m_State.ColorAttachments[i].bBlendEnabled;

			const BlendState& blendState = m_State.ColorAttachments[i].BlendingState;
			colorBlendAttachment.colorBlendOp = (VkBlendOp)blendState.BlendOp;
			colorBlendAttachment.alphaBlendOp = (VkBlendOp)blendState.BlendOpAlpha;
			colorBlendAttachment.srcColorBlendFactor = (VkBlendFactor)blendState.BlendSrc;
			colorBlendAttachment.dstColorBlendFactor = (VkBlendFactor)blendState.BlendDst;
			colorBlendAttachment.srcAlphaBlendFactor = (VkBlendFactor)blendState.BlendSrcAlpha;
			colorBlendAttachment.dstAlphaBlendFactor = (VkBlendFactor)blendState.BlendDstAlpha;

			colorBlendAttachmentStates[i] = colorBlendAttachment;
		}

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = (uint32_t)colorBlendAttachmentStates.size();
		colorBlending.pAttachments = colorBlendAttachmentStates.data();

		// Pipeline layout
		{
			m_SetBindings = vertexShader->GetLayoutSetBindings();
			if (geometryShader)
				MergeDescriptorSetLayoutBindings(m_SetBindings, geometryShader->GetLayoutSetBindings());
			if (fragmentShader)
				MergeDescriptorSetLayoutBindings(m_SetBindings, fragmentShader->GetLayoutSetBindings());
			const uint32_t setsCount = (uint32_t)m_SetBindings.size();

			ClearSetLayouts();
			m_SetLayouts.resize(setsCount);
			for (uint32_t i = 0; i < setsCount; ++i)
			{
				VkDescriptorSetLayoutCreateInfo layoutInfo{};
				layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutInfo.bindingCount = (uint32_t)m_SetBindings[i].size();
				layoutInfo.pBindings = m_SetBindings[i].data();

				VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_SetLayouts[i]));
			}

			std::vector<VkPushConstantRange> pushConstants;
			for (auto& range : vertexShader->GetPushConstantRanges())
			{
				VkPushConstantRange vkRange{};
				vkRange.stageFlags = ShaderTypeToVulkan(range.ShaderStage);
				vkRange.offset = range.Offset;
				vkRange.size = range.Size;
				if (vkRange.size)
					pushConstants.push_back(vkRange);
			}

			if (geometryShader)
				for (auto& range : geometryShader->GetPushConstantRanges())
				{
					VkPushConstantRange vkRange{};
					vkRange.stageFlags = ShaderTypeToVulkan(range.ShaderStage);
					vkRange.offset = range.Offset;
					vkRange.size = range.Size;
					if (vkRange.size)
						pushConstants.push_back(vkRange);
				}

			if (fragmentShader)
				for (auto& range : fragmentShader->GetPushConstantRanges())
				{
					VkPushConstantRange vkRange{};
					vkRange.stageFlags = ShaderTypeToVulkan(range.ShaderStage);
					vkRange.offset = range.Offset;
					vkRange.size = range.Size;
					if (vkRange.size)
						pushConstants.push_back(vkRange);
				}

			VkPipelineLayoutCreateInfo pipelineLayoutCI{};
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = setsCount;
			pipelineLayoutCI.pSetLayouts = m_SetLayouts.data();
			pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pushConstants.size();
			pipelineLayoutCI.pPushConstantRanges = pushConstants.data();

			VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_PipelineLayout));
		}

		// Render pass
		std::vector<VkAttachmentDescription> attachmentDescs;
		std::vector<VkAttachmentReference> colorRefs(colorAttachmentsCount);
		std::vector<VkAttachmentReference> resolveRefs;
		std::vector<VkAttachmentReference> depthRef;
		std::vector<VkImageView> attachmentsImageViews;
		attachmentsImageViews.reserve(colorAttachmentsCount);

		if (m_State.ResolveAttachments.size())
		{
			resolveRefs.resize(colorAttachmentsCount);
			for (auto& ref : resolveRefs)
			{
				ref.attachment = VK_ATTACHMENT_UNUSED;
				ref.layout = VK_IMAGE_LAYOUT_UNDEFINED;
			}
		}

		for (size_t i = 0; i < colorAttachmentsCount; ++i)
		{
			const Ref<Image>& renderTarget = m_State.ColorAttachments[i].Image;
			if (!renderTarget)
			{
				colorRefs[i] = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
				continue;
			}

			assert(renderTarget->HasUsage(ImageUsage::ColorAttachment));

			uint32_t attachmentIndex = (uint32_t)attachmentDescs.size();
			const bool bClearEnabled = m_State.ColorAttachments[i].bClearEnabled;
			auto& desc = attachmentDescs.emplace_back();
			desc.samples = GetVulkanSamplesCount(renderTarget->GetSamplesCount());
			desc.loadOp = bClearEnabled ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.format = ImageFormatToVulkan(renderTarget->GetFormat());
			desc.initialLayout = bClearEnabled ? VK_IMAGE_LAYOUT_UNDEFINED : ImageLayoutToVulkan(m_State.ColorAttachments[i].InitialLayout);
			desc.finalLayout = ImageLayoutToVulkan(m_State.ColorAttachments[i].FinalLayout);

			colorRefs[i] = { attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			attachmentsImageViews.push_back((VkImageView)renderTarget->GetImageViewHandle());
		}

		for (size_t i = 0; i < m_State.ResolveAttachments.size(); ++i)
		{
			const Ref<Image>& renderTarget = m_State.ResolveAttachments[i].Image;
			if (!renderTarget)
			{
				colorRefs[i] = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
				continue;
			}

			assert(renderTarget->HasUsage(ImageUsage::ColorAttachment));

			uint32_t attachmentIndex = (uint32_t)attachmentDescs.size();
			attachmentDescs.push_back({});
			auto& desc = attachmentDescs.back();
			desc.samples = GetVulkanSamplesCount(renderTarget->GetSamplesCount());
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.format = ImageFormatToVulkan(renderTarget->GetFormat());
			desc.initialLayout = ImageLayoutToVulkan(m_State.ResolveAttachments[i].InitialLayout);
			desc.finalLayout = ImageLayoutToVulkan(m_State.ResolveAttachments[i].FinalLayout);

			resolveRefs[i] = { attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			attachmentsImageViews.push_back((VkImageView)renderTarget->GetImageViewHandle());
		}

		VkPipelineDepthStencilStateCreateInfo depthStencilCI = s_DefaultDepthStencilCI;
		const Ref<Image>& depthStencilImage = m_State.DepthStencilAttachment.Image;
		if (depthStencilImage)
		{
			assert(depthStencilImage->HasUsage(ImageUsage::DepthStencilAttachment));

			const bool bClearEnabled = m_State.DepthStencilAttachment.bClearEnabled;
			std::uint32_t attachmentIndex = static_cast<std::uint32_t>(attachmentDescs.size());
			auto& desc = attachmentDescs.emplace_back();
			desc.samples = GetVulkanSamplesCount(depthStencilImage->GetSamplesCount());
			desc.loadOp = bClearEnabled ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			desc.stencilLoadOp = bClearEnabled ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.format = ImageFormatToVulkan(depthStencilImage->GetFormat());
			desc.initialLayout = bClearEnabled ? VK_IMAGE_LAYOUT_UNDEFINED : ImageLayoutToVulkan(m_State.DepthStencilAttachment.InitialLayout);
			desc.finalLayout = ImageLayoutToVulkan(m_State.DepthStencilAttachment.FinalLayout);

			const bool bDepthTestEnabled = (m_State.DepthStencilAttachment.DepthCompareOp != CompareOperation::Never);
			depthStencilCI.depthTestEnable = bDepthTestEnabled;
			depthStencilCI.depthCompareOp = bDepthTestEnabled ? CompareOpToVulkan(m_State.DepthStencilAttachment.DepthCompareOp) : VK_COMPARE_OP_NEVER;
			depthStencilCI.depthWriteEnable = m_State.DepthStencilAttachment.bWriteDepth;

			depthRef.push_back({ attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
			attachmentsImageViews.push_back((VkImageView)depthStencilImage->GetImageViewHandle());
		}

		assert(depthRef.size() == 0 || depthRef.size() == 1);

		VkSubpassDescription subpassDesc{};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = (uint32_t)colorAttachmentsCount;
		subpassDesc.pColorAttachments = colorRefs.data();
		subpassDesc.pResolveAttachments = m_State.ResolveAttachments.empty() ? nullptr : resolveRefs.data();
		subpassDesc.pDepthStencilAttachment = depthRef.empty() ? nullptr : depthRef.data();

		std::array<VkSubpassDependency, 2> dependencies{};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


		uint32_t viewMask = 0;
		uint32_t& correlationMask = viewMask; // same as view mask
		VkRenderPassMultiviewCreateInfo multiviewCI{};
		multiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		multiviewCI.subpassCount = 1;
		multiviewCI.correlationMaskCount = 1;
		multiviewCI.pViewMasks = &viewMask;
		multiviewCI.pCorrelationMasks = &correlationMask;

		VkRenderPassCreateInfo renderPassCI{};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = (uint32_t)attachmentDescs.size();
		renderPassCI.pAttachments = attachmentDescs.data();
		renderPassCI.dependencyCount = (uint32_t)dependencies.size();
		renderPassCI.pDependencies = dependencies.data();
		renderPassCI.pSubpasses = &subpassDesc;
		renderPassCI.subpassCount = 1;
		if (m_State.bEnableMultiViewRendering)
		{
			for (uint32_t i = 0; i < m_State.MultiViewPasses; ++i)
			{
				viewMask = viewMask << 1;
				viewMask++;
			}
			renderPassCI.pNext = &multiviewCI;
		}
		VK_CHECK(vkCreateRenderPass(device, &renderPassCI, nullptr, &m_RenderPass));

		// If size is 0, try to get size of an attachment
		if (m_Width == 0 || m_Height == 0)
		{
			if (colorAttachmentsCount)
			{
				auto& size = m_State.ColorAttachments[0].Image->GetSize();
				m_Width = size.x;
				m_Height = size.y;
			}
			else if (depthStencilImage)
			{
				auto& size = depthStencilImage->GetSize();
				m_Width = size.x;
				m_Height = size.y;
			}
		}

		uint32_t imagelessLayerCount = 1;
		VkFormat imagelessFormat = VK_FORMAT_UNDEFINED;
		VkImageUsageFlags imagelessUsage = VK_INSTANCE_CREATE_FLAG_BITS_MAX_ENUM;

		if (m_State.bImagelessFramebuffer)
		{
			if (m_State.ColorAttachments.size())
			{
				imagelessLayerCount = m_State.ColorAttachments[0].Image->GetLayersCount();
				imagelessFormat = ImageFormatToVulkan(m_State.ColorAttachments[0].Image->GetFormat());
				imagelessUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
			else if (depthStencilImage)
			{
				imagelessLayerCount = depthStencilImage->GetLayersCount();
				imagelessFormat = ImageFormatToVulkan(depthStencilImage->GetFormat());
				imagelessUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			}
		}

		VkFramebufferAttachmentImageInfo imagelessImageInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO };
		imagelessImageInfo.width = m_Width;
		imagelessImageInfo.height = m_Height;
		imagelessImageInfo.layerCount = imagelessLayerCount;
		imagelessImageInfo.usage = imagelessUsage;
		imagelessImageInfo.viewFormatCount = 1;
		imagelessImageInfo.pViewFormats = &imagelessFormat;

		VkFramebufferAttachmentsCreateInfo imagelessCI{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO };
		imagelessCI.attachmentImageInfoCount = 1;
		imagelessCI.pAttachmentImageInfos = &imagelessImageInfo;
		
		VkFramebufferCreateInfo framebufferCI{};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.attachmentCount = (uint32_t)attachmentsImageViews.size();
		framebufferCI.renderPass = m_RenderPass;
		framebufferCI.width = m_Width;
		framebufferCI.height = m_Height;
		framebufferCI.layers = 1;
		framebufferCI.pAttachments = attachmentsImageViews.data();
		framebufferCI.flags = m_State.bImagelessFramebuffer ? VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT : 0;
		framebufferCI.pNext = m_State.bImagelessFramebuffer ? &imagelessCI : nullptr;
		VK_CHECK(vkCreateFramebuffer(device, &framebufferCI, nullptr, &m_Framebuffer));

		// Shaders
		std::vector<VkPipelineShaderStageCreateInfo> stages;
		stages.reserve(3);
		stages.push_back(vertexShader->GetPipelineShaderStageInfo());
		if (fragmentShader)
			stages.push_back(fragmentShader->GetPipelineShaderStageInfo());
		if (geometryShader)
			stages.push_back(geometryShader->GetPipelineShaderStageInfo());

		VkSpecializationInfo vertexSpecializationInfo{};
		VkSpecializationInfo fragmentSpecializationInfo{};
		std::vector<VkSpecializationMapEntry> vertexMapEntries;
		std::vector<VkSpecializationMapEntry> fragmentMapEntries;

		if (m_State.VertexSpecializationInfo.Data && (m_State.VertexSpecializationInfo.Size > 0))
		{
			const size_t mapEntriesCount = m_State.VertexSpecializationInfo.MapEntries.size();
			vertexMapEntries.reserve(mapEntriesCount);
			for (auto& entry : m_State.VertexSpecializationInfo.MapEntries)
				vertexMapEntries.emplace_back(VkSpecializationMapEntry{ entry.ConstantID, entry.Offset, entry.Size });

			vertexSpecializationInfo.pData = m_State.VertexSpecializationInfo.Data;
			vertexSpecializationInfo.dataSize = m_State.VertexSpecializationInfo.Size;
			vertexSpecializationInfo.mapEntryCount = (uint32_t)mapEntriesCount;
			vertexSpecializationInfo.pMapEntries = vertexMapEntries.data();
			stages[0].pSpecializationInfo = &vertexSpecializationInfo;
		}
		if (fragmentShader && m_State.FragmentSpecializationInfo.Data && (m_State.FragmentSpecializationInfo.Size > 0))
		{
			const size_t mapEntriesCount = m_State.FragmentSpecializationInfo.MapEntries.size();
			fragmentMapEntries.reserve(mapEntriesCount);
			for (auto& entry : m_State.FragmentSpecializationInfo.MapEntries)
				fragmentMapEntries.emplace_back(VkSpecializationMapEntry{ entry.ConstantID, entry.Offset, entry.Size });

			fragmentSpecializationInfo.pData = m_State.FragmentSpecializationInfo.Data;
			fragmentSpecializationInfo.dataSize = m_State.FragmentSpecializationInfo.Size;
			fragmentSpecializationInfo.mapEntryCount = (uint32_t)mapEntriesCount;
			fragmentSpecializationInfo.pMapEntries = fragmentMapEntries.data();
			stages[1].pSpecializationInfo = &fragmentSpecializationInfo;
		}

		// Vertex input
		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		std::array<VkVertexInputBindingDescription, 2> vertexInputBindings;
		vertexInputBindings[0].binding = 0;
		vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		vertexInputBindings[0].stride = 0;

		vertexInputBindings[1].binding = 1;
		vertexInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		vertexInputBindings[1].stride = 0;

		auto vertexAttribs = vertexShader->GetInputAttribs();
		if (vertexAttribs.size() > 0)
		{
			for (auto& attrib : m_State.PerInstanceAttribs)
			{
				auto it = std::find_if(vertexAttribs.begin(), vertexAttribs.end(), [&attrib](const auto& it)
					{
						return it.location == attrib.Location;
					});

				if (it != vertexAttribs.end())
					it->binding = 1;
			}

			for (auto& attrib : vertexAttribs)
			{
				uint32_t size = attrib.offset;
				attrib.offset = vertexInputBindings[attrib.binding].stride;
				vertexInputBindings[attrib.binding].stride += size;
			}

			vertexInput.vertexBindingDescriptionCount = m_State.PerInstanceAttribs.empty() ? 1 : 2;
			vertexInput.pVertexBindingDescriptions = vertexInputBindings.data();
			vertexInput.vertexAttributeDescriptionCount = (uint32_t)vertexAttribs.size();
			vertexInput.pVertexAttributeDescriptions = vertexAttribs.data();
		}

		// Dynamic states
		std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStatesCI{};
		dynamicStatesCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStatesCI.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicStatesCI.pDynamicStates = dynamicStates.data();

		// Graphics Pipeline
		VkGraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.basePipelineIndex = -1;
		pipelineCI.basePipelineHandle = parentPipeline;
		pipelineCI.layout = m_PipelineLayout;
		pipelineCI.stageCount = (uint32_t)stages.size();
		pipelineCI.pStages = stages.data();
		pipelineCI.renderPass = m_RenderPass;
		pipelineCI.pVertexInputState = &vertexInput;
		pipelineCI.pInputAssemblyState = &inputAssembly;
		pipelineCI.pRasterizationState = &rasterization;
		pipelineCI.pColorBlendState = &colorBlending;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilCI;
		pipelineCI.pMultisampleState = &multisampling;
		pipelineCI.pDynamicState = &dynamicStatesCI;

		VK_CHECK(vkCreateGraphicsPipelines(device, VulkanPipelineCache::GetCache(), 1, &pipelineCI, nullptr, &m_GraphicsPipeline));
	}

	void VulkanPipelineGraphics::Resize(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;

		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

		if (m_Framebuffer)
			vkDestroyFramebuffer(device, m_Framebuffer, nullptr);

		std::vector<VkImageView> attachmentsImageViews;
		attachmentsImageViews.reserve(m_State.ColorAttachments.size());
		for (size_t i = 0; i < m_State.ColorAttachments.size(); ++i)
		{
			const Ref<Image>& renderTarget = m_State.ColorAttachments[i].Image;
			if (!renderTarget)
				continue;

			attachmentsImageViews.push_back((VkImageView)renderTarget->GetImageViewHandle());
		}

		for (size_t i = 0; i < m_State.ResolveAttachments.size(); ++i)
		{
			const Ref<Image>& renderTarget = m_State.ResolveAttachments[i].Image;
			if (!renderTarget)
				continue;

			attachmentsImageViews.push_back((VkImageView)renderTarget->GetImageViewHandle());
		}

		VkPipelineDepthStencilStateCreateInfo depthStencilCI = s_DefaultDepthStencilCI;
		const Ref<Image>& depthStencilImage = m_State.DepthStencilAttachment.Image;
		if (depthStencilImage)
			attachmentsImageViews.push_back((VkImageView)depthStencilImage->GetImageViewHandle());

		const VkFormat imagelessFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		VkFramebufferAttachmentImageInfo imagelessImageInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO };
		imagelessImageInfo.width = m_Width;
		imagelessImageInfo.height = m_Height;
		imagelessImageInfo.layerCount = 1;
		imagelessImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imagelessImageInfo.viewFormatCount = 1;
		imagelessImageInfo.pViewFormats = &imagelessFormat;

		VkFramebufferAttachmentsCreateInfo imagelessCI{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO };
		imagelessCI.attachmentImageInfoCount = 1;
		imagelessCI.pAttachmentImageInfos = &imagelessImageInfo;

		VkFramebufferCreateInfo framebufferCI{};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.attachmentCount = (uint32_t)attachmentsImageViews.size();
		framebufferCI.renderPass = m_RenderPass;
		framebufferCI.width = m_Width;
		framebufferCI.height = m_Height;
		framebufferCI.layers = 1;
		framebufferCI.pAttachments = attachmentsImageViews.data();
		framebufferCI.flags = m_State.bImagelessFramebuffer ? VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT : 0;
		framebufferCI.pNext = m_State.bImagelessFramebuffer ? &imagelessCI : nullptr;
		VK_CHECK(vkCreateFramebuffer(device, &framebufferCI, nullptr, &m_Framebuffer));
	}

}
