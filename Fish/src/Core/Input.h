#pragma once
#include"macro.h"
#include"MouseCodes.h"
#include"KeyCodes.h"
#include"GLFW/glfw3.h"


namespace Fish {

	class /*FS_API*/ Input {
	public:
		static void Init(GLFWwindow* Window) { m_Window = Window; }

		static bool IsKeyPressed(KeyCode key);
		static bool IsMouseButtonPressed(MouseCode button);
		static std::pair<float, float>GetMousePosition();
		/*static glm::vec2 GetMousePosition();*/
		static float GetMouseX();
		static float GetMouseY();

	private:
		static GLFWwindow* m_Window ;
	};
}