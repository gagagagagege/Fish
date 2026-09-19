#include "VulkanSurface.h"
#include <stdexcept>

namespace Fish {
	void surface::createWin32Surface(vk::raii::Instance& instance, vk::raii::SurfaceKHR& surface, GLFWwindow* window)
	{
		VkSurfaceKHR cSurface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &cSurface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		surface = vk::raii::SurfaceKHR(instance, cSurface);
	}

}
