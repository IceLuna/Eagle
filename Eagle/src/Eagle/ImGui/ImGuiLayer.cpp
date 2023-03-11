#include "egpch.h"

#include "ImGuiLayer.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/UI/UI.h"
#include "Platform/Vulkan/VulkanImGuiLayer.h"

namespace Eagle
{
	ImGuiLayer::ImGuiLayer(const std::string& name)
		: Layer(name)
	{}

	Ref<ImGuiLayer> ImGuiLayer::Create()
	{
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanImGuiLayer>();
		}
		EG_CORE_ASSERT(false, "Unknown renderer API");
		return nullptr;
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		ImGui::StyleColorsDark();

		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}
	
	bool ImGuiLayer::ShowStyleSelector(const char* label, int* selectedStyleIdx)
	{
		static std::vector<std::string> styleNames = { "Default", "Classic", "Dark", "Light" };
		UI::BeginPropertyGrid("StyleSelector");
		if (UI::Combo(label, *selectedStyleIdx, styleNames, *selectedStyleIdx))
		{
			SelectStyle(*selectedStyleIdx);
			UI::EndPropertyGrid();
			return true;
		}
		UI::EndPropertyGrid();
		return false;
	}

	void ImGuiLayer::SelectStyle(int idx)
	{
		switch (idx)
		{
			case 0: SetDarkThemeColors(); break;
			case 1: ImGui::StyleColorsClassic(); break;
			case 2: ImGui::StyleColorsDark(); break;
			case 3: ImGui::StyleColorsLight(); break;
		}
	}
}
