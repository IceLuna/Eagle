#pragma once

#include "Eagle/Input/KeyCodes.h"
#include "Eagle/Input/MouseButtonCodes.h"

namespace Eagle
{
	class Input
	{
	protected:
		Input() = default;

	public:
		Input(const Input&) = delete;
		Input& operator= (const Input&) = delete;

	public:
		static bool IsKeyPressed(Key::KeyCode keyCode);

		static bool IsMouseButtonPressed(Mouse::MouseButton mouseButton);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();

		static void SetShowCursor(bool bShow);
		static void SetCursorPos(double xPos, double yPos);

		static bool IsCursorVisible();
	};
}