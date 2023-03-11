#pragma once

#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	enum class RendererAPIType
	{
		None = 0,
		Vulkan = 1
	};

	struct RendererCapabilities
	{
		std::string Vendor;
		std::string Device;
		std::string DriverVersion;
		std::string ApiVersion;

		uint32_t MaxSamples = 0;
		float MaxAnisotropy = 0.f;
	};

	class RendererContext
	{
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		virtual void WaitIdle() const = 0;
		virtual ImageFormat GetDepthFormat() const = 0;
		
		const RendererCapabilities& GetCapabilities() const { return m_Caps; }

		static RendererAPIType Current() { return s_API; }
		static void SetAPI(RendererAPIType api) { s_API = api; }

		static Ref<RendererContext> Create();

	protected:
		RendererCapabilities m_Caps;
		static RendererAPIType s_API;
	};
}
