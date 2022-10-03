#include "egpch.h"
#include "Image.h"

#include "RendererAPI.h"
#include "RenderCommandManager.h"
#include "Platform/Vulkan/VulkanImage.h"

namespace Eagle
{
    Ref<Image> Image::Create(const ImageSpecifications& specs, const std::string& debugName)
    {
        Ref<Image> result;
        switch (RendererAPI::Current())
        {
            case RendererAPIType::Vulkan: result = MakeRef<VulkanImage>(specs, debugName);
                break;
            default:
            EG_CORE_ASSERT(false, "Unknown renderer API");
            return nullptr;
        }

        if (specs.Layout != ImageLayoutType::Unknown)
        {
            Renderer::Submit([result, layout = specs.Layout](Ref<CommandBuffer>& cmd) mutable
            {
                cmd->TransitionLayout(result, ImageLayoutType::Unknown, layout);
            });
        }

        return result;
    }
}
