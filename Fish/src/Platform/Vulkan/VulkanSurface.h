#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

namespace Fish {
	class surface {
	public:
		static void createWin32Surface(vk::raii::Instance& instance, vk::raii::SurfaceKHR& surface, GLFWwindow* window);
	};
}
