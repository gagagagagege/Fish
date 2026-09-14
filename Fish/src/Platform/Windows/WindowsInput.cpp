#include"fspch.h"
#include"Core/Input.h"


namespace Fish{
	GLFWwindow* Input::m_Window = nullptr;

	bool Input::IsKeyPressed(const KeyCode key)
	{
		auto state = glfwGetKey(m_Window, static_cast<int32_t>(key));
		return state == GLFW_PRESS;
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		auto state = glfwGetMouseButton(m_Window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		double xpos, ypos;
		glfwGetCursorPos(m_Window, &xpos, &ypos);

		return { (float)xpos,(float)ypos };
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
}