#pragma once

#include "Eagle/Core.h"

namespace Eagle
{
	class EAGLE_API Input
	{
	public:
		static bool IsKeyPressed(int keyCode) {return s_Instance->IsKeyPressedImpl(keyCode);}

		static bool IsMouseButtonPressed(int keyCode) {return s_Instance->IsMouseButtonPressedImpl(keyCode);}
		static std::pair<float, float> GetMousePosition() {return s_Instance->GetMousePositionImpl();}
		static float GetMouseX() {return s_Instance->GetMouseXImpl();}
		static float GetMouseY() {return s_Instance->GetMouseYImpl();}

	protected:
		virtual bool IsKeyPressedImpl(int keyCode) = 0;
		virtual bool IsMouseButtonPressedImpl(int button) = 0;

		virtual std::pair<float, float> GetMousePositionImpl() = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;

	private:
		static Input* s_Instance;
	};
}