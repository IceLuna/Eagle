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
        // So it's not poosible right now to make this call from inside Image-constructor.
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
}
