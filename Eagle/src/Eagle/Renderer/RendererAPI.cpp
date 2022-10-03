#include "egpch.h"
#include "RendererAPI.h"

namespace Eagle
{
	RendererAPIType RendererAPI::s_API = RendererAPIType::Vulkan;
	
	void RendererAPI::SetAPI(RendererAPIType api)
	{
		//TODO
		EG_CORE_ASSERT(false, "Not implemented");
		s_API = api;
	}

}