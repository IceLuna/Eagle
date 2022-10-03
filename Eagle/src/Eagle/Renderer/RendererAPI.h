#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	enum class PrimitiveType
	{
		None,
		Lines,
		Triangles
	};

	enum class RendererAPIType
	{
		None = 0,
		Vulkan = 1
	};

	struct RendererCapabilities
	{
		std::string Vendor;
		std::string Device;
		std::string Version;

		uint32_t MaxSamples = 0;
		uint32_t MaxTextureUnits = 0;
		float MaxAnisotropy = 0.f;
	};

	class RendererAPI
	{
	public:

	public:
		virtual ~RendererAPI() = default;

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual RendererCapabilities& GetCapabilities() = 0;

		static RendererAPIType Current() { return s_API; }
		static void SetAPI(RendererAPIType api);

	private:
		static RendererAPIType s_API;
	};
}