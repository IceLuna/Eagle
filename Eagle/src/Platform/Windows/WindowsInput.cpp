#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Input/Input.h"
#include "WindowsWindow.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
	static bool s_CursorVisible = true;

	bool Input::IsKeyPressed(Key::KeyCode keyCode)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		int state = glfwGetKey(window, keyCode);
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressed(Mouse::MouseButton mouseButton)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		int state = glfwGetMouseButton(window, mouseButton);
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
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

	void Input::SetShowCursor(bool bShow)
	{
		s_CursorVisible = bShow;
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();

		glfwSetInputMode(window, GLFW_CURSOR, bShow ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
	}

	void Input::SetCursorPos(double xPos, double yPos)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		glfwSetCursorPos(window, xPos, yPos);
	}

	bool Input::IsCursorVisible()
	{
		return s_CursorVisible;
	}
	
	void Input::SetWindowTitle(const std::string& title)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();

		WindowsWindow::WindowData& data = *(WindowsWindow::WindowData*)glfwGetWindowUserPointer(window);
		data.Title = title;
		glfwSetWindowTitle(window, title.c_str());
	}
	
	const std::string& Input::GetWindowTitle()
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();

		WindowsWindow::WindowData& data = *(WindowsWindow::WindowData*)glfwGetWindowUserPointer(window);
		return data.Title;
	}
}
