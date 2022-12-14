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

		virtual void NextFrame() = 0;

		static Ref<ImGuiLayer> Create();

		static void SetDarkThemeColors();

		static bool ShowStyleSelector(const char* label, int* selectedStyleIdx);
		static void SelectStyle(int idx);

	protected:
		virtual void Begin() = 0;
		virtual void End(Ref<CommandBuffer>& cmd) = 0;
		virtual void UpdatePlatform() = 0;

		friend class Renderer;
	};
}