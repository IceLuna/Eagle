#pragma once

#include "Eagle/Core/Layer.h"
#include "Eagle/Core/GUID.h"

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

namespace Eagle
{
	class CommandBuffer;

	class ImGuiLayer : public Layer
	{
	public:
		enum class Style
		{
			Default,
			Classic,
			Dark,
			Light
		};

	public:
		ImGuiLayer(const std::string& name = "ImGuiLayer");
		virtual ~ImGuiLayer();

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void UpdatePlatform() = 0;
		void OnImGuiRender() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;

		void AddMessage(const std::string& message);

		static Ref<ImGuiLayer> Create();

		static void SetDarkThemeColors();

		static bool ShowStyleSelector(const char* label, Style& outStyle);
		static void SelectStyle(Style style);
		static glm::vec2 GetMousePos();

		void RebuildFonts();

	protected:
		virtual void Render(const Ref<CommandBuffer>& cmd) = 0;
		virtual void UploadFonts() {}

		friend class RenderManager;

	protected:
		std::string m_IniPath;

	private:
		std::vector<std::string> m_PopupMessages;
	};
}