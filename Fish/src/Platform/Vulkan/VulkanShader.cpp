#include "VulkanShader.h"

#include <stdexcept>
#include <cstring>

namespace Fish {
	std::vector<char> shader::readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	[[nodiscard]] vk::raii::ShaderModule shader::createShaderModule(const std::vector<char>& code, vk::raii::Device& device)
	{
		vk::ShaderModuleCreateInfo createInfo{};
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		return vk::raii::ShaderModule(device, createInfo);
	}
}
