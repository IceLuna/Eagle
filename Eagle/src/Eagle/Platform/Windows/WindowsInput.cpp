#include "egpch.h"

#include "Eagle/Application.h"
#include "WindowsInput.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
	Input* Input::s_Instance = new WindowsInput();

	bool WindowsInput::IsKeyPressedImpl(Key::KeyCode keyCode)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		int state = glfwGetKey(window, keyCode);
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}
	bool WindowsInput::IsMouseButtonPressedImpl(Mouse::MouseButton mouseButton)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		int state = glfwGetMouseButton(window, mouseButton);
		return state == GLFW_PRESS;
	}
	std::pair<float, float> WindowsInput::GetMousePositionImpl()
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		return {(float)x, (float)y};
	}
	float WindowsInput::GetMouseXImpl()
	{
		auto [x, y] = GetMousePositionImpl();
		return x;
	}
	float WindowsInput::GetMouseYImpl()
	{
		auto [x, y] = GetMousePositionImpl();
		return y;
	}
}