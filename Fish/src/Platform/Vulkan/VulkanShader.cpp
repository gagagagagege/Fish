#include "VulkanShader.h"

#include "VulkanContext.h"
#include "VulkanDescriptorAllocator.h"

#include <cstring>
#include <stdexcept>

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

	VulkanShader::VulkanShader(VulkanContext* context, DescriptorAllocator& descriptorAllocator,
		const std::string& name, const std::string& path)
		: m_name(name)
	{
		const std::vector<char> code = shader::readFile(path);
		if (code.empty())
			throw std::runtime_error("VulkanShader: 读不到 " + path);
		if (code.size() % sizeof(uint32_t) != 0)
			throw std::runtime_error("VulkanShader: " + path + " 的大小不是 4 的倍数,不是 SPIR-V");

		// 反射要的是 uint32_t 的指令流,而 readFile 给的是字节
		std::vector<uint32_t> words(code.size() / sizeof(uint32_t));
		std::memcpy(words.data(), code.data(), code.size());

		descriptorAllocator.AddShaderBindings(context, ReflectDescriptorBindings(words).bindings);

		m_vertexInput = ReflectVertexInput(words, m_vertexEntry.c_str());

		m_module = shader::createShaderModule(code, context->device);
	}
}
