#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Input/Input.h"
#include "WindowsWindow.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
	static bool s_CursorVisible = true;

	bool Input::IsKeyPressed(Key keyCode)
	{
		GLFWwindow* window = Application::Get().GetWindow().GetGLFWWindow();
		int state = glfwGetKey(window, int(keyCode));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressed(Mouse mouseButton)
	{
		GLFWwindow* window = Application::Get().GetWindow().GetGLFWWindow();
		int state = glfwGetMouseButton(window, int(mouseButton));
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		GLFWwindow* window = Application::Get().GetWindow().GetGLFWWindow();
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		return {(float)x, (float)y};
	}

	float Input::GetMouseX()
	{
		auto [x, y] = GetMousePosition();
		return x;
	}

	float Input::GetMouseY()
	{
		auto [x, y] = GetMousePosition();
		return y;
	}

	void Input::SetShowMouse(bool bShow)
	{
		s_CursorVisible = bShow;
		GLFWwindow* window = Application::Get().GetWindow().GetGLFWWindow();

		glfwSetInputMode(window, GLFW_CURSOR, bShow ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
	}

	void Input::SetMousePos(double xPos, double yPos)
	{
		GLFWwindow* window = Application::Get().GetWindow().GetGLFWWindow();
		glfwSetCursorPos(window, xPos, yPos);
	}

	bool Input::IsMouseVisible()
	{
		return s_CursorVisible;
	}
}
