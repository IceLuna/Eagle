#pragma once

#include "Eagle/ImGui/ImGuiLayer.h"

namespace Eagle
{
	class VulkanImGuiLayer : public ImGuiLayer
	{
	public:
		void OnAttach() override;
		void OnDetach() override;

		void NextFrame() override { Begin(); }

	private:
		void Begin() override;
		void End(Ref<CommandBuffer>& cmd) override;

	private:
		void* m_PersistantDescriptorPool; // Used to init resources during ImGui initialization.
		std::vector<void*> m_Pools; // Per frame pools to init and reset our resources
	};
}
