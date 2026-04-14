#include "egpch.h"

#include "ImGuiLayer.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Input/Input.h"
#include "Platform/Vulkan/VulkanImGuiLayer.h"

#include <implot.h>
#include <ImGuizmo.h>

namespace Eagle
{
	static ImGuiStyle s_BaseStyle;

	static void ApplyScaling()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		const float dpi = Application::Get().GetWindow().GetDPIScale();

		style = s_BaseStyle;
		style.ScaleAllSizes(dpi);
	}

	void ImGuiLayer::RebuildFonts()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Reload fonts with the new DPI and rebuild the atlas
		io.Fonts->Clear();
		UI::LoadFonts();
		io.Fonts->Build();

		ApplyScaling();
		UploadFonts();
	}

	ImGuiLayer::ImGuiLayer(const std::string& name)
		: Layer(name)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  //Enable Keyboard controls 
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //Enable Gamepad controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	   //Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	   //Enable Multi-Viewport
		//io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // Re-rasterize fonts on DPI change
		//io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // Scale the actual window size
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.ConfigDebugHighlightIdConflicts = false;
		io.ConfigDebugHighlightIdConflictsShowItemPicker = false;

		m_IniPath = (Application::GetCorePath() / "imgui.ini").u8string();
		io.IniFilename = m_IniPath.c_str();

		UI::LoadFonts();

		ImGuiStyle& style = ImGui::GetStyle();
		style.TabRounding = 8.f;
		style.FrameRounding = 8.f;
		style.GrabRounding = 8.f;
		style.WindowRounding = 8.f;
		style.PopupRounding = 8.f;

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			//style.WindowRounding = 0.f;
			style.Colors[ImGuiCol_WindowBg].w = 1.f;
		}

		SetDarkThemeColors();

		// ImGuizmo style
		{
			ImGuizmo::Style& style = ImGuizmo::GetStyle();
			style.RotationLineThickness = 6.f;
			style.RotationOuterLineThickness = 6.f;
			style.TranslationLineArrowSize = 12.f;
			style.Colors[ImGuizmo::DIRECTION_X] = ImGui::ColorConvertU32ToFloat4(0xFF715ED8);
			style.Colors[ImGuizmo::DIRECTION_Y] = ImGui::ColorConvertU32ToFloat4(0xFF25AA25);
			style.Colors[ImGuizmo::DIRECTION_Z] = ImGui::ColorConvertU32ToFloat4(0xFFCC532C);
			style.Colors[ImGuizmo::PLANE_X] = ImGui::ColorConvertU32ToFloat4(0xFF7A68D8);
			style.Colors[ImGuizmo::PLANE_Y] = ImGui::ColorConvertU32ToFloat4(0xFF55AB55);
			style.Colors[ImGuizmo::PLANE_Z] = ImGui::ColorConvertU32ToFloat4(0xFFD96742);
			style.Colors[ImGuizmo::SELECTION] = ImGui::ColorConvertU32ToFloat4(0xFF20AACC);
			ImGuizmo::SetGizmoSizeClipSpace(0.15f);
		}

		// Base style has been initialized, save it before scaling
		s_BaseStyle = style;
		ApplyScaling();
	}

	ImGuiLayer::~ImGuiLayer()
	{
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnImGuiRender()
	{
		if (m_PopupMessages.empty())
			return;

		// Draw only one at the time
		const auto& message = m_PopupMessages.back();
		
		ImGui::PushID(message.c_str());

		if (UI::ShowMessage("Eagle Editor", message, UI::ButtonType::OK) != UI::ButtonType::None)
			m_PopupMessages.pop_back();

		ImGui::PopID();
	}

	void ImGuiLayer::OnUpdate(Timestep ts)
	{
		const bool bEnableMouseInput = Input::IsMouseVisible();
		if (bEnableMouseInput)
		{
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}
		else
		{
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (EventType::WindowContentScale == e.GetEventType())
		{
			WindowContentScaleEvent& scaleEvent = (WindowContentScaleEvent&)e;
			RebuildFonts();
		}
	}

	void ImGuiLayer::AddMessage(const std::string& message)
	{
		if (Application::Get().IsGame() == false)
			m_PopupMessages.push_back(message);
	}

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
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f } + ImVec4(0.1f, 0.1f, 0.1f, 0.0f);
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f } + ImVec4(0.1f, 0.1f, 0.1f, 0.0f);
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
	
	bool ImGuiLayer::ShowStyleSelector(const char* label, Style& outStyle)
	{
		if (UI::ComboEnum<Style>(label, outStyle))
		{
			SelectStyle(outStyle);
			return true;
		}
		return false;
	}

	void ImGuiLayer::SelectStyle(Style style)
	{
		switch (style)
		{
			case Style::Default: SetDarkThemeColors(); break;
			case Style::Classic: ImGui::StyleColorsClassic(); break;
			case Style::Dark:    ImGui::StyleColorsDark(); break;
			case Style::Light:   ImGui::StyleColorsLight(); break;
		}
	}
	
	glm::vec2 ImGuiLayer::GetMousePos()
	{
		auto [x, y] = ImGui::GetMousePos();
		return glm::vec2(x, y);
	}
}
