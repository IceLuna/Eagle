#include "egpch.h"
#include "Cubemap.h"
#include "Renderer.h"

namespace Eagle
{
	Ref<Cubemap> Cubemap::Create(const std::array<Ref<Texture2D>, 6> textures)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
