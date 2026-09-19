#pragma once

#include <vector>
#include <array>
#include <fstream>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

namespace Fish {
	class shader
	{
	public:
		static std::vector<char> readFile(const std::string& filename);
		static vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device& device);
	};
}
