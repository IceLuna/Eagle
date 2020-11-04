#pragma once

#include "Eagle/Input.h"

namespace Eagle
{
	class WindowsInput : public Input
	{
	protected:
		virtual bool IsKeyPressedImpl(Key::KeyCode keyCode) override;
		virtual bool IsMouseButtonPressedImpl(Mouse::MouseButton mouseButton) override;

		virtual std::pair<float, float> GetMousePositionImpl() override;
		virtual float GetMouseXImpl() override;
		virtual float GetMouseYImpl() override;
	};
}