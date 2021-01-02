#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Input/KeyCodes.h"
#include "Eagle/Input/MouseButtonCodes.h"

namespace Eagle
{
	class EAGLE_API Input
	{
	public:
		static bool IsKeyPressed(Key::KeyCode keyCode) {return s_Instance->IsKeyPressedImpl(keyCode);}

		static bool IsMouseButtonPressed(Mouse::MouseButton mouseButton) {return s_Instance->IsMouseButtonPressedImpl(mouseButton);}
		static std::pair<float, float> GetMousePosition() {return s_Instance->GetMousePositionImpl();}
		static float GetMouseX() {return s_Instance->GetMouseXImpl();}
		static float GetMouseY() {return s_Instance->GetMouseYImpl();}

	protected:
		virtual bool IsKeyPressedImpl(Key::KeyCode keyCode) = 0;
		virtual bool IsMouseButtonPressedImpl(Mouse::MouseButton mouseButton) = 0;

		virtual std::pair<float, float> GetMousePositionImpl() = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;

	private:
		static Input* s_Instance;
	};
}