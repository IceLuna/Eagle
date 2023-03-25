#pragma once

#include "Eagle/Core/Layer.h"

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

namespace Eagle
{
	class CommandBuffer;

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(const std::string& name = "ImGuiLayer");
		virtual ~ImGuiLayer() = default;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void UpdatePlatform() = 0;

		static Ref<ImGuiLayer> Create();

		static void SetDarkThemeColors();

		static bool ShowStyleSelector(const char* label, int* selectedStyleIdx);
		static void SelectStyle(int idx);

	protected:
		virtual void Render(Ref<CommandBuffer>& cmd) = 0;

		friend class RenderManager;
	};
}