#include "egpch.h"
#include "Image.h"

#include "RenderCommandManager.h"
#include "Platform/Vulkan/VulkanImage.h"

namespace Eagle
{
    Ref<Image> Image::Create(ImageSpecifications specs, const std::string& debugName)
    {
        const bool bGenerateMips = specs.MipsCount > 1;
        if (bGenerateMips)
            specs.Usage |= ImageUsage::TransferSrc | ImageUsage::TransferDst;

        Ref<Image> result;
        switch (RendererContext::Current())
        {
            case RendererAPIType::Vulkan: result = MakeRef<VulkanImage>(specs, debugName);
                break;
            default:
            EG_CORE_ASSERT(false, "Unknown renderer API");
            return result;
        }

        // This is here and not inside Image because this call requires fully constructed Ref
        // So it's not possible right now to make this call from inside Image-constructor.
        if (specs.Layout != ImageLayoutType::Unknown || bGenerateMips)
        {
            RenderManager::Submit([result, layout = specs.Layout, bGenerateMips](Ref<CommandBuffer>& cmd) mutable
            {
                if (layout != ImageLayoutType::Unknown)
                    cmd->TransitionLayout(result, ImageLayoutType::Unknown, layout);
                if (bGenerateMips)
                    cmd->GenerateMips(result, layout, layout);
            });
        }

        return result;
    }

    size_t Image::GetMemoryUsage() const
    {
        glm::uvec3 size = m_Specs.Size;
        size_t memUsage = 0;

        for (uint32_t i = 0; i < m_Specs.MipsCount; ++i)
        {
            memUsage += CalculateImageMemorySize(m_Specs.Format, size);
            size >>= 1;
            size = glm::max(size, glm::uvec3(1));
        }

        return memUsage;
    }
}
