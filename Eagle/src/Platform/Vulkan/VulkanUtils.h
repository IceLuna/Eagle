#pragma once

#include "Vulkan.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	inline const char* VulkanVendorIDToString(uint32_t vendorID)
	{
		switch (vendorID)
		{
			case 0x10DE: return "NVIDIA";
			case 0x1002: return "AMD";
			case 0x8086: return "INTEL";
			case 0x13B5: return "ARM";
		}
		return "Unknown";
	}

	inline VkShaderStageFlagBits ShaderTypeToVulkan(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::Vertex:    return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderType::Fragment:  return VK_SHADER_STAGE_FRAGMENT_BIT;
		case ShaderType::Geometry:  return VK_SHADER_STAGE_GEOMETRY_BIT;
		case ShaderType::Compute:   return VK_SHADER_STAGE_COMPUTE_BIT;
		}
		EG_CORE_ASSERT(false);
		return VK_SHADER_STAGE_ALL;
	}

	inline VkPrimitiveTopology TopologyToVulkan(Topology topology)
	{
		switch (topology)
		{
		case Topology::Triangles:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case Topology::Lines:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case Topology::Points:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		default:
			assert(false);
			break;
		}

		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	}

	inline VkImageLayout ImageLayoutToVulkan(ImageLayout imageLayout)
	{
		switch (imageLayout.LayoutType)
		{
		case ImageLayoutType::ReadOnly:
		{
			ImageReadAccess readAccessFlags = imageLayout.ReadAccessFlags;
			if (readAccessFlags == ImageReadAccess::None)
			{
				assert(!"Unknown read access");
				return VK_IMAGE_LAYOUT_UNDEFINED;
			}
			if (HasFlags(ImageReadAccess::DepthStencilRead, readAccessFlags))
			{
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			if (HasFlags(ImageReadAccess::PixelShaderRead | ImageReadAccess::NonPixelShaderRead, readAccessFlags))
			{
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			if (HasFlags(ImageReadAccess::CopySource, readAccessFlags))
			{
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			EG_CORE_ASSERT(!"Unknown read access");
			return VK_IMAGE_LAYOUT_UNDEFINED;
		}
		case ImageLayoutType::CopyDest:				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case ImageLayoutType::RenderTarget:			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case ImageLayoutType::StorageImage:			return VK_IMAGE_LAYOUT_GENERAL;
		case ImageLayoutType::DepthStencilWrite:	return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case ImageLayoutType::Present:				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		case ImageLayoutType::Unknown:				return VK_IMAGE_LAYOUT_UNDEFINED;
		default: EG_CORE_ASSERT(!"Unknown layout");			return VK_IMAGE_LAYOUT_UNDEFINED;
		}
	}

	inline VkSampleCountFlagBits GetVulkanSamplesCount(SamplesCount samples)
	{
		switch (samples)
		{
		case SamplesCount::Samples1:
			return VK_SAMPLE_COUNT_1_BIT;
		case SamplesCount::Samples2:
			return VK_SAMPLE_COUNT_2_BIT;
		case SamplesCount::Samples4:
			return VK_SAMPLE_COUNT_4_BIT;
		case SamplesCount::Samples8:
			return VK_SAMPLE_COUNT_8_BIT;
		case SamplesCount::Samples16:
			return VK_SAMPLE_COUNT_16_BIT;
		case SamplesCount::Samples32:
			return VK_SAMPLE_COUNT_32_BIT;
		case SamplesCount::Samples64:
			return VK_SAMPLE_COUNT_64_BIT;
		default:
			EG_CORE_ASSERT(!"Unknown samples");
			return VK_SAMPLE_COUNT_1_BIT;
		}
	}

	inline VkImageUsageFlags ImageUsageToVulkan(ImageUsage usage)
	{
		VkImageUsageFlags res = 0;

		if (HasFlags(usage, ImageUsage::TransferSrc))
		{
			res |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			usage &= ~ImageUsage::TransferSrc;
		}

		if (HasFlags(usage, ImageUsage::TransferDst))
		{
			res |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			usage &= ~ImageUsage::TransferDst;
		}

		if (HasFlags(usage, ImageUsage::Sampled))
		{
			res |= VK_IMAGE_USAGE_SAMPLED_BIT;
			usage &= ~ImageUsage::Sampled;
		}

		if (HasFlags(usage, ImageUsage::Storage))
		{
			res |= VK_IMAGE_USAGE_STORAGE_BIT;
			usage &= ~ImageUsage::Storage;
		}

		if (HasFlags(usage, ImageUsage::ColorAttachment))
		{
			res |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			usage &= ~ImageUsage::ColorAttachment;
		}

		if (HasFlags(usage, ImageUsage::DepthStencilAttachment))
		{
			res |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			usage &= ~ImageUsage::DepthStencilAttachment;
		}

		if (HasFlags(usage, ImageUsage::TransientAttachment))
		{
			res |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			usage &= ~ImageUsage::TransientAttachment;
		}

		if (HasFlags(usage, ImageUsage::InputAttachment))
		{
			res |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			usage &= ~ImageUsage::InputAttachment;
		}

		EG_CORE_ASSERT(usage == ImageUsage::None);

		return res;
	}

	inline VkFormat ImageFormatToVulkan(ImageFormat format)
	{
		switch (format)
		{
		case ImageFormat::Unknown: return VK_FORMAT_UNDEFINED;
		case ImageFormat::R32G32B32A32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ImageFormat::R32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
		case ImageFormat::R32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
		case ImageFormat::R32G32B32_Float: return VK_FORMAT_R32G32B32_SFLOAT;
		case ImageFormat::R32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
		case ImageFormat::R32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
		case ImageFormat::R16G16B16A16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case ImageFormat::R16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
		case ImageFormat::R16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
		case ImageFormat::R16G16B16A16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
		case ImageFormat::R16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
		case ImageFormat::R32G32_Float: return VK_FORMAT_R32G32_SFLOAT;
		case ImageFormat::R32G32_UInt: return VK_FORMAT_R32G32_UINT;
		case ImageFormat::R32G32_SInt: return VK_FORMAT_R32G32_SINT;
		case ImageFormat::D32_Float_S8X24_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case ImageFormat::R10G10B10A2_UNorm: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case ImageFormat::R10G10B10A2_UInt: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		case ImageFormat::R11G11B10_Float: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case ImageFormat::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case ImageFormat::R8G8B8A8_UNorm_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
		case ImageFormat::R8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
		case ImageFormat::R8G8B8A8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
		case ImageFormat::R8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
		case ImageFormat::R8G8B8_UNorm: return VK_FORMAT_R8G8B8_UNORM;
		case ImageFormat::R8G8B8_UNorm_SRGB: return VK_FORMAT_R8G8B8_SRGB;
		case ImageFormat::R8G8B8_UInt: return VK_FORMAT_R8G8B8_UINT;
		case ImageFormat::R8G8B8_SNorm: return VK_FORMAT_R8G8B8_SNORM;
		case ImageFormat::R8G8B8_SInt: return VK_FORMAT_R8G8B8_SINT;
		case ImageFormat::R16G16_Float: return VK_FORMAT_R16G16_SFLOAT;
		case ImageFormat::R16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
		case ImageFormat::R16G16_UInt: return VK_FORMAT_R16G16_UINT;
		case ImageFormat::R16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
		case ImageFormat::R16G16_SInt: return VK_FORMAT_R16G16_SINT;
		case ImageFormat::D32_Float: return VK_FORMAT_D32_SFLOAT;
		case ImageFormat::R32_Float: return VK_FORMAT_R32_SFLOAT;
		case ImageFormat::R32_UInt: return VK_FORMAT_R32_UINT;
		case ImageFormat::R32_SInt: return VK_FORMAT_R32_SINT;
		case ImageFormat::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
		case ImageFormat::R8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
		case ImageFormat::R8G8_UNorm_SRGB: return VK_FORMAT_R8G8_SRGB;
		case ImageFormat::R8G8_UInt: return VK_FORMAT_R8G8_UINT;
		case ImageFormat::R8G8_SNorm: return VK_FORMAT_R8G8_SNORM;
		case ImageFormat::R8G8_SInt: return VK_FORMAT_R8G8_SINT;
		case ImageFormat::R16_Float: return VK_FORMAT_R16_SFLOAT;
		case ImageFormat::D16_UNorm: return VK_FORMAT_D16_UNORM;
		case ImageFormat::R16_UNorm: return VK_FORMAT_R16_UNORM;
		case ImageFormat::R16_UInt: return VK_FORMAT_R16_UINT;
		case ImageFormat::R16_SNorm: return VK_FORMAT_R16_SNORM;
		case ImageFormat::R16_SInt: return VK_FORMAT_R16_SINT;
		case ImageFormat::R8_UNorm_SRGB: return VK_FORMAT_R8_SRGB;
		case ImageFormat::R8_UNorm: return VK_FORMAT_R8_UNORM;
		case ImageFormat::R8_UInt: return VK_FORMAT_R8_UINT;
		case ImageFormat::R8_SNorm: return VK_FORMAT_R8_SNORM;
		case ImageFormat::R8_SInt: return VK_FORMAT_R8_SINT;
		case ImageFormat::R9G9B9E5_SharedExp: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		case ImageFormat::R8G8_B8G8_UNorm: return VK_FORMAT_B8G8R8G8_422_UNORM;
		case ImageFormat::G8R8_G8B8_UNorm: return VK_FORMAT_G8B8G8R8_422_UNORM;
		case ImageFormat::BC1_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case ImageFormat::BC1_UNorm_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case ImageFormat::BC2_UNorm: return VK_FORMAT_BC2_UNORM_BLOCK;
		case ImageFormat::BC2_UNorm_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
		case ImageFormat::BC3_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case ImageFormat::BC3_UNorm_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
		case ImageFormat::BC4_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
		case ImageFormat::BC4_SNorm: return VK_FORMAT_BC4_SNORM_BLOCK;
		case ImageFormat::BC5_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
		case ImageFormat::BC5_SNorm: return VK_FORMAT_BC5_SNORM_BLOCK;
		case ImageFormat::B5G6R5_UNorm: return VK_FORMAT_B5G6R5_UNORM_PACK16;
		case ImageFormat::B5G5R5A1_UNorm: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		case ImageFormat::B8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
		case ImageFormat::B8G8R8A8_UNorm_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
		case ImageFormat::BC6H_UFloat16: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case ImageFormat::BC6H_SFloat16: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case ImageFormat::BC7_UNorm: return VK_FORMAT_BC7_UNORM_BLOCK;
		case ImageFormat::BC7_UNorm_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
		}
		EG_CORE_ASSERT(!"Unknown format");
		return VK_FORMAT_UNDEFINED;
	}

	inline ImageFormat VulkanToImageFormat(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_UNDEFINED: return ImageFormat::Unknown;
		case VK_FORMAT_R32G32B32A32_SFLOAT: return ImageFormat::R32G32B32A32_Float;
		case VK_FORMAT_R32G32B32A32_UINT: return ImageFormat::R32G32B32A32_UInt;
		case VK_FORMAT_R32G32B32A32_SINT: return  ImageFormat::R32G32B32A32_SInt;
		case VK_FORMAT_R32G32B32_SFLOAT: return ImageFormat::R32G32B32_Float;
		case VK_FORMAT_R32G32B32_UINT: return ImageFormat::R32G32B32_UInt;
		case VK_FORMAT_R32G32B32_SINT: return ImageFormat::R32G32B32_SInt;
		case VK_FORMAT_R16G16B16A16_SFLOAT: return ImageFormat::R16G16B16A16_Float;
		case VK_FORMAT_R16G16B16A16_UNORM: return ImageFormat::R16G16B16A16_UNorm;
		case VK_FORMAT_R16G16B16A16_UINT: return ImageFormat::R16G16B16A16_UInt;
		case VK_FORMAT_R16G16B16A16_SNORM: return ImageFormat::R16G16B16A16_SNorm;
		case VK_FORMAT_R16G16B16A16_SINT: return ImageFormat::R16G16B16A16_SInt;
		case VK_FORMAT_R32G32_SFLOAT: return ImageFormat::R32G32_Float;
		case VK_FORMAT_R32G32_UINT: return ImageFormat::R32G32_UInt;
		case VK_FORMAT_R32G32_SINT: return ImageFormat::R32G32_SInt;
		case VK_FORMAT_D32_SFLOAT_S8_UINT: return ImageFormat::D32_Float_S8X24_UInt;
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return ImageFormat::R10G10B10A2_UNorm;
		case VK_FORMAT_A2B10G10R10_UINT_PACK32: return ImageFormat::R10G10B10A2_UInt;
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return ImageFormat::R11G11B10_Float;
		case VK_FORMAT_R8G8B8A8_UNORM: return ImageFormat::R8G8B8A8_UNorm;
		case VK_FORMAT_R8G8B8A8_SRGB: return ImageFormat::R8G8B8A8_UNorm_SRGB;
		case VK_FORMAT_R8G8B8A8_UINT: return ImageFormat::R8G8B8A8_UInt;
		case VK_FORMAT_R8G8B8A8_SNORM: return ImageFormat::R8G8B8A8_SNorm;
		case VK_FORMAT_R8G8B8A8_SINT: return ImageFormat::R8G8B8A8_SInt;
		case VK_FORMAT_R8G8B8_UNORM: return ImageFormat::R8G8B8_UNorm;
		case VK_FORMAT_R8G8B8_SRGB: return ImageFormat::R8G8B8_UNorm_SRGB;
		case VK_FORMAT_R8G8B8_UINT: return ImageFormat::R8G8B8_UInt;
		case VK_FORMAT_R8G8B8_SNORM: return ImageFormat::R8G8B8_SNorm;
		case VK_FORMAT_R8G8B8_SINT: return ImageFormat::R8G8B8_SInt;
		case VK_FORMAT_R16G16_SFLOAT: return ImageFormat::R16G16_Float;
		case VK_FORMAT_R16G16_UNORM: return ImageFormat::R16G16_UNorm;
		case VK_FORMAT_R16G16_UINT: return ImageFormat::R16G16_UInt;
		case VK_FORMAT_R16G16_SNORM: return ImageFormat::R16G16_SNorm;
		case VK_FORMAT_R16G16_SINT: return ImageFormat::R16G16_SInt;
		case VK_FORMAT_D32_SFLOAT: return ImageFormat::D32_Float;
		case VK_FORMAT_R32_SFLOAT: return ImageFormat::R32_Float;
		case VK_FORMAT_R32_UINT: return ImageFormat::R32_UInt;
		case VK_FORMAT_R32_SINT: return ImageFormat::R32_SInt;
		case VK_FORMAT_D24_UNORM_S8_UINT: return ImageFormat::D24_UNorm_S8_UInt;
		case VK_FORMAT_R8G8_UNORM: return ImageFormat::R8G8_UNorm;
		case VK_FORMAT_R8G8_SRGB: return ImageFormat::R8G8_UNorm_SRGB;
		case VK_FORMAT_R8G8_UINT: return ImageFormat::R8G8_UInt;
		case VK_FORMAT_R8G8_SNORM: return ImageFormat::R8G8_SNorm;
		case VK_FORMAT_R8G8_SINT: return ImageFormat::R8G8_SInt;
		case VK_FORMAT_R16_SFLOAT: return ImageFormat::R16_Float;
		case VK_FORMAT_D16_UNORM: return ImageFormat::D16_UNorm;
		case VK_FORMAT_R16_UNORM: return ImageFormat::R16_UNorm;
		case VK_FORMAT_R16_UINT: return ImageFormat::R16_UInt;
		case VK_FORMAT_R16_SNORM: return ImageFormat::R16_SNorm;
		case VK_FORMAT_R16_SINT: return ImageFormat::R16_SInt;
		case VK_FORMAT_R8_SRGB: return ImageFormat::R8_UNorm_SRGB;
		case VK_FORMAT_R8_UNORM: return ImageFormat::R8_UNorm;
		case VK_FORMAT_R8_UINT: return ImageFormat::R8_UInt;
		case VK_FORMAT_R8_SNORM: return ImageFormat::R8_SNorm;
		case VK_FORMAT_R8_SINT: return ImageFormat::R8_SInt;
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return ImageFormat::R9G9B9E5_SharedExp;
		case VK_FORMAT_B8G8R8G8_422_UNORM: return ImageFormat::R8G8_B8G8_UNorm;
		case VK_FORMAT_G8B8G8R8_422_UNORM: return ImageFormat::G8R8_G8B8_UNorm;
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return ImageFormat::BC1_UNorm;
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return ImageFormat::BC1_UNorm_SRGB;
		case VK_FORMAT_BC2_UNORM_BLOCK: return ImageFormat::BC2_UNorm;
		case VK_FORMAT_BC2_SRGB_BLOCK: return ImageFormat::BC2_UNorm_SRGB;
		case VK_FORMAT_BC3_UNORM_BLOCK: return ImageFormat::BC3_UNorm;
		case VK_FORMAT_BC3_SRGB_BLOCK: return ImageFormat::BC3_UNorm_SRGB;
		case VK_FORMAT_BC4_UNORM_BLOCK: return ImageFormat::BC4_UNorm;
		case VK_FORMAT_BC4_SNORM_BLOCK: return ImageFormat::BC4_SNorm;
		case VK_FORMAT_BC5_UNORM_BLOCK: return ImageFormat::BC5_UNorm;
		case VK_FORMAT_BC5_SNORM_BLOCK: return ImageFormat::BC5_SNorm;
		case VK_FORMAT_B5G6R5_UNORM_PACK16: return ImageFormat::B5G6R5_UNorm;
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return ImageFormat::B5G5R5A1_UNorm;
		case VK_FORMAT_B8G8R8A8_UNORM: return ImageFormat::B8G8R8A8_UNorm;
		case VK_FORMAT_B8G8R8A8_SRGB: return ImageFormat::B8G8R8A8_UNorm_SRGB;
		case VK_FORMAT_BC6H_UFLOAT_BLOCK: return ImageFormat::BC6H_UFloat16;
		case VK_FORMAT_BC6H_SFLOAT_BLOCK: return ImageFormat::BC6H_SFloat16;
		case VK_FORMAT_BC7_UNORM_BLOCK: return ImageFormat::BC7_UNorm;
		case VK_FORMAT_BC7_SRGB_BLOCK: return		 ImageFormat::BC7_UNorm_SRGB;
		}
		EG_CORE_ASSERT(!"Unknown format");
		return ImageFormat::Unknown;
	}

	inline VkImageType ImageTypeToVulkan(ImageType type)
	{
		switch (type)
		{
		case ImageType::Type1D: return VK_IMAGE_TYPE_1D;
		case ImageType::Type2D: return VK_IMAGE_TYPE_2D;
		case ImageType::Type3D: return VK_IMAGE_TYPE_3D;
		}
		EG_CORE_ASSERT(!"Unknown type");
		return VK_IMAGE_TYPE_MAX_ENUM;
	}

	inline VkImageViewType ImageTypeToVulkanImageViewType(ImageType type, bool bIsCube)
	{
		if (bIsCube)
			return VK_IMAGE_VIEW_TYPE_CUBE;

		switch (type)
		{
		case ImageType::Type1D: return VK_IMAGE_VIEW_TYPE_1D;
		case ImageType::Type2D: return VK_IMAGE_VIEW_TYPE_2D;
		case ImageType::Type3D: return VK_IMAGE_VIEW_TYPE_3D;
		}

		EG_CORE_ASSERT(!"Unknown type");
		return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	}

	inline bool HasDepth(VkFormat format)
	{
		static const std::vector<VkFormat> formats =
		{
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};

		return std::find(formats.begin(), formats.end(), format) != std::end(formats);
	}

	inline bool HasStencil(VkFormat format)
	{
		static const std::vector<VkFormat> formats =
		{
			VK_FORMAT_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};

		return std::find(formats.begin(), formats.end(), format) != std::end(formats);
	}

	inline VkImageAspectFlags GetImageAspectFlags(VkFormat format)
	{
		const bool bHasDepth = HasDepth(format);
		const bool bHasStencil = HasStencil(format);

		if (bHasDepth && bHasStencil) return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		else if (bHasDepth) return VK_IMAGE_ASPECT_DEPTH_BIT;
		else if (bHasStencil) return VK_IMAGE_ASPECT_STENCIL_BIT;
		else return VK_IMAGE_ASPECT_COLOR_BIT;
	}

	inline void GetTransitionStagesAndAccesses(
		VkImageLayout oldLayout, VkQueueFlags oldQueueFlags,
		VkImageLayout newLayout, VkQueueFlags newQueueFlags,
		VkPipelineStageFlags* outSrcStage, VkAccessFlags* outSrcAccess,
		VkPipelineStageFlags* outDstStage, VkAccessFlags* outDstAccess)
	{
		// Old
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			*outSrcAccess = 0;
			*outSrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			*outSrcAccess = 0;
			*outSrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			*outSrcAccess = VK_ACCESS_SHADER_READ_BIT;
			*outSrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			*outSrcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			*outSrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			*outSrcAccess = VK_ACCESS_TRANSFER_READ_BIT;
			*outSrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
		{
			*outSrcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			*outSrcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			*outSrcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			*outSrcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			*outSrcAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			*outSrcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else EG_CORE_ASSERT(!"Unsupported layout transition!");

		// New
		if (newLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			*outDstAccess = 0;
			*outDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			*outDstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			*outDstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			*outDstAccess = VK_ACCESS_SHADER_READ_BIT;
			*outDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			*outDstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			*outDstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			*outDstAccess = VK_ACCESS_TRANSFER_READ_BIT;
			*outDstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
		{
			*outDstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			*outDstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			*outDstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			*outDstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			*outDstAccess = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
			*outDstStage = VK_PIPELINE_STAGE_HOST_BIT;
		}
		else EG_CORE_ASSERT(!"Unsupported layout transition!");

		// Dst access exception case
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
			*outDstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

		// Src stage exception cases
		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
			*outSrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // VK_PIPELINE_STAGE_TRANSFER_BIT should be OK

		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
			*outSrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // VK_PIPELINE_STAGE_TRANSFER_BIT should be OK

		if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			*outSrcStage = 0;
			if (oldQueueFlags & VK_QUEUE_GRAPHICS_BIT)
				*outSrcStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (oldQueueFlags & VK_QUEUE_COMPUTE_BIT)
				*outSrcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}

		if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			*outSrcStage = 0;
			if (oldQueueFlags & VK_QUEUE_GRAPHICS_BIT)
				*outSrcStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (oldQueueFlags & VK_QUEUE_COMPUTE_BIT)
				*outSrcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}

		// Dst stage exception cases
		if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			*outDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // VK_PIPELINE_STAGE_TRANSFER_BIT should be OK

		if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			*outDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // VK_PIPELINE_STAGE_TRANSFER_BIT should be OK

		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			*outDstStage = 0;
			if (newQueueFlags & VK_QUEUE_GRAPHICS_BIT)
				*outDstStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (newQueueFlags & VK_QUEUE_COMPUTE_BIT)
				*outDstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}

		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			*outDstStage = 0;
			if (newQueueFlags & VK_QUEUE_GRAPHICS_BIT)
				*outDstStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (newQueueFlags & VK_QUEUE_COMPUTE_BIT)
				*outDstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
	}

	inline VkBufferUsageFlags BufferUsageToVulkan(BufferUsage usage)
	{
		VkBufferUsageFlags res = 0;

		if ((usage & BufferUsage::TransferSrc) == BufferUsage::TransferSrc)
		{
			res |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			usage &= ~BufferUsage::TransferSrc;
		}

		if ((usage & BufferUsage::TransferDst) == BufferUsage::TransferDst)
		{
			res |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			usage &= ~BufferUsage::TransferDst;
		}

		if ((usage & BufferUsage::UniformTexelBuffer) == BufferUsage::UniformTexelBuffer)
		{
			res |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
			usage &= ~BufferUsage::UniformTexelBuffer;
		}

		if ((usage & BufferUsage::StorageTexelBuffer) == BufferUsage::StorageTexelBuffer)
		{
			res |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
			usage &= ~BufferUsage::StorageTexelBuffer;
		}

		if ((usage & BufferUsage::UniformBuffer) == BufferUsage::UniformBuffer)
		{
			res |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			usage &= ~BufferUsage::UniformBuffer;
		}

		if ((usage & BufferUsage::StorageBuffer) == BufferUsage::StorageBuffer)
		{
			res |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			usage &= ~BufferUsage::StorageBuffer;
		}

		if ((usage & BufferUsage::IndexBuffer) == BufferUsage::IndexBuffer)
		{
			res |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			usage &= ~BufferUsage::IndexBuffer;
		}

		if ((usage & BufferUsage::VertexBuffer) == BufferUsage::VertexBuffer)
		{
			res |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			usage &= ~BufferUsage::VertexBuffer;
		}

		if ((usage & BufferUsage::IndirectBuffer) == BufferUsage::IndirectBuffer)
		{
			res |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			usage &= ~BufferUsage::IndirectBuffer;
		}

		if ((usage & BufferUsage::RayTracing) == BufferUsage::RayTracing)
		{
			res |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
			usage &= ~BufferUsage::RayTracing;
		}

		if ((usage & BufferUsage::AccelerationStructure) == BufferUsage::AccelerationStructure)
		{
			res |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
			usage &= ~BufferUsage::AccelerationStructure;
		}

		if ((usage & BufferUsage::AccelerationStructureBuildInput) == BufferUsage::AccelerationStructureBuildInput)
		{
			res |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			usage &= ~BufferUsage::AccelerationStructureBuildInput;
		}

		EG_CORE_ASSERT(usage == BufferUsage::None);
		return res;
	}

	inline void GetStageAndAccess(BufferLayout layout, VkQueueFlags queueFlags, VkPipelineStageFlags* outStage, VkAccessFlags* outAccess)
	{
		*outStage = 0;
		*outAccess = 0;

		switch (layout.LayoutType)
		{
		case BufferLayoutType::ReadOnly:
		{
			BufferReadAccess readAccessFlags = layout.ReadAccessFlags;
			if (HasFlags(readAccessFlags, BufferReadAccess::CopySource))
			{
				EG_CORE_ASSERT(queueFlags & VK_QUEUE_TRANSFER_BIT);
				*outStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
				*outAccess |= VK_ACCESS_TRANSFER_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::CopySource;
			}
			if (HasFlags(readAccessFlags, BufferReadAccess::Vertex))
			{
				EG_CORE_ASSERT(queueFlags & VK_QUEUE_GRAPHICS_BIT);
				*outStage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
				*outAccess |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::Vertex;
			}
			if (HasFlags(readAccessFlags, BufferReadAccess::Index))
			{
				EG_CORE_ASSERT(queueFlags & VK_QUEUE_GRAPHICS_BIT);
				*outStage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
				*outAccess |= VK_ACCESS_INDEX_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::Index;
			}
			if (HasFlags(readAccessFlags, BufferReadAccess::Uniform))
			{
				EG_CORE_ASSERT((queueFlags & VK_QUEUE_GRAPHICS_BIT) || (queueFlags & VK_QUEUE_COMPUTE_BIT));
				if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					*outStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				if (queueFlags & VK_QUEUE_COMPUTE_BIT)
				{
					*outStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				}
				*outAccess |= VK_ACCESS_UNIFORM_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::Uniform;
			}
			if (HasFlags(readAccessFlags, BufferReadAccess::IndirectArgument))
			{
				EG_CORE_ASSERT((queueFlags & VK_QUEUE_GRAPHICS_BIT) || (queueFlags & VK_QUEUE_COMPUTE_BIT));
				*outStage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
				*outAccess |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::IndirectArgument;
			}
			if (HasFlags(readAccessFlags, BufferReadAccess::PixelShaderRead))
			{
				EG_CORE_ASSERT(queueFlags & VK_QUEUE_GRAPHICS_BIT);
				*outStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				*outAccess |= VK_ACCESS_SHADER_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::PixelShaderRead;
			}
			if (HasFlags(readAccessFlags, BufferReadAccess::NonPixelShaderRead))
			{
				EG_CORE_ASSERT((queueFlags & VK_QUEUE_GRAPHICS_BIT) || (queueFlags & VK_QUEUE_COMPUTE_BIT));
				if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					*outStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				}
				if (queueFlags & VK_QUEUE_COMPUTE_BIT)
				{
					*outStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				}
				*outAccess |= VK_ACCESS_SHADER_READ_BIT;
				readAccessFlags &= ~BufferReadAccess::NonPixelShaderRead;
			}
			EG_CORE_ASSERT(readAccessFlags == BufferReadAccess::None);
			return;
		}
		case BufferLayoutType::CopyDest:
		{
			EG_CORE_ASSERT(queueFlags & VK_QUEUE_TRANSFER_BIT);
			*outStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			*outAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
			return;
		}
		case BufferLayoutType::StorageBuffer:
		{
			EG_CORE_ASSERT((queueFlags & VK_QUEUE_GRAPHICS_BIT) || (queueFlags & VK_QUEUE_COMPUTE_BIT));
			if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				*outStage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			if (queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				*outStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			}
			*outAccess |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			return;
		}
		case BufferLayoutType::Unknown:
		{
			*outAccess = 0;
			*outStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			return;
		}
		default: EG_CORE_ASSERT(!"Unsupported BufferLayoutType");
		}
	}

	inline VkSamplerAddressMode AddressModeToVulkan(AddressMode addressMode)
	{
		switch (addressMode)
		{
		case AddressMode::Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case AddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case AddressMode::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case AddressMode::ClampToOpaqueBlack:
		case AddressMode::ClampToOpaqueWhite: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case AddressMode::MirrorOnce: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		default:
			EG_CORE_ASSERT(!"Unsupported address mode");
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	inline VkBorderColor BorderColorForAddressMode(AddressMode addressMode)
	{
		switch (addressMode)
		{
		case AddressMode::Wrap:
		case AddressMode::Mirror:
		case AddressMode::Clamp:
		case AddressMode::MirrorOnce: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		case AddressMode::ClampToOpaqueBlack: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		case AddressMode::ClampToOpaqueWhite: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		default:
			EG_CORE_ASSERT(!"Unsupported address mode");
			return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		}
	}

	inline VkCompareOp CompareOpToVulkan(CompareOperation compareOp)
	{
		switch (compareOp)
		{
		case CompareOperation::Never: return VK_COMPARE_OP_NEVER;
		case CompareOperation::Less: return VK_COMPARE_OP_LESS;
		case CompareOperation::Equal: return VK_COMPARE_OP_EQUAL;
		case CompareOperation::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOperation::Greater: return VK_COMPARE_OP_GREATER;
		case CompareOperation::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOperation::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOperation::Always: return VK_COMPARE_OP_ALWAYS;
		default:
			EG_CORE_ASSERT(!"Unsupported compare op");
			return VK_COMPARE_OP_NEVER;
		}
	}

	inline VkCullModeFlags CullModeToVulkan(CullMode mode)
	{
		switch (mode)
		{
		case CullMode::None: return VK_CULL_MODE_NONE;
		case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
		case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
		default:
			EG_CORE_ASSERT(!"Unsupported cull mode");
			return VK_CULL_MODE_NONE;
		}
	}

	inline void FilterModeToVulkan(FilterMode filterMode, VkFilter* outMinFilter, VkFilter* outMagFilter, VkSamplerMipmapMode* outMipmapMode)
	{
		switch (filterMode)
		{
		case FilterMode::Point:
			*outMinFilter = VK_FILTER_NEAREST;
			*outMagFilter = VK_FILTER_NEAREST;
			*outMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case FilterMode::Bilinear:
			*outMinFilter = VK_FILTER_LINEAR;
			*outMagFilter = VK_FILTER_LINEAR;
			*outMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case FilterMode::Trilinear:
			*outMinFilter = VK_FILTER_LINEAR;
			*outMagFilter = VK_FILTER_LINEAR;
			*outMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case FilterMode::Anisotropic:
			*outMinFilter = VK_FILTER_LINEAR;
			*outMagFilter = VK_FILTER_LINEAR;
			*outMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		default:
			EG_CORE_ASSERT(!"Unsupported filter mode");
			*outMinFilter = VK_FILTER_NEAREST;
			*outMagFilter = VK_FILTER_NEAREST;
			break;
		}
	}

	inline bool IsBufferType(VkDescriptorType type)
	{
		return
			type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
			type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
			type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
			type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
			type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	}

	inline bool IsImageType(VkDescriptorType type)
	{
		return
			type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
			type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
			type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}

	inline bool IsSamplerType(VkDescriptorType type)
	{
		return type == VK_DESCRIPTOR_TYPE_SAMPLER;
	}
}
