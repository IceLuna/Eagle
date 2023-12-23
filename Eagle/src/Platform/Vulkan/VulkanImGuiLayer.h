#pragma once

#include "Eagle/ImGui/ImGuiLayer.h"

namespace Eagle
{
	class VulkanImGuiLayer : public ImGuiLayer
	{
	public:
		void OnAttach() override;
		void OnDetach() override;

		void BeginFrame() override;
		void EndFrame() override;

	private:
		void Render(Ref<CommandBuffer>& cmd) override;
		void UpdatePlatform() override;

	private:
		void* m_PersistantDescriptorPool; // Used to init resources during ImGui initialization.
		std::vector<void*> m_Pools; // Per frame pools to init and reset our resources
	};
}
