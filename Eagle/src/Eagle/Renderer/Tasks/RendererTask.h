#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	class CommandBuffer;
	class SceneRenderer;
	struct SceneRendererSettings;

	class RendererTask : virtual public std::enable_shared_from_this<RendererTask>
	{
	public:
		RendererTask(SceneRenderer& renderer) : m_Renderer(renderer) {}
		virtual ~RendererTask() = default;

		RendererTask(const RendererTask&) = delete;
		RendererTask(RendererTask&&) = delete;

		RendererTask& operator=(const RendererTask&) = delete;
		RendererTask& operator=(RendererTask&&) = delete;

		virtual void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) = 0;
		virtual void OnResize(const glm::uvec2 size) {}

		virtual void InitWithOptions(const SceneRendererSettings&) {}

	protected:
		SceneRenderer& m_Renderer;
	};
}
