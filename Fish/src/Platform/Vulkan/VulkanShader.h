#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "Renderer/Shader.h"
#include "VulkanReflection.h"

#include <fstream>
#include <string>
#include <vector>

namespace Fish {
	class shader
	{
	public:
		static std::vector<char> readFile(const std::string& filename);
		static vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device& device);
	};

	class VulkanContext;
	class DescriptorAllocator;

	class VulkanShader : public Shader
	{
	public:
		VulkanShader(VulkanContext* context, DescriptorAllocator& descriptorAllocator,
			const std::string& name, const std::string& path);

		const std::string& GetName() const override { return m_name; }

		vk::ShaderModule            module() const { return *m_module; }
		std::string_view            vertexEntry() const { return m_vertexEntry; }
		std::string_view            fragmentEntry() const { return m_fragmentEntry; }
		const ReflectedVertexInput& vertexInput() const { return m_vertexInput; }

	private:
		std::string            m_name;
		vk::raii::ShaderModule m_module = nullptr;
		ReflectedVertexInput   m_vertexInput;

		// 存成 string 而不是 string_view:PipelineDesc 里那两个字段是
		// string_view,指向的必须是长命的内存。
		std::string m_vertexEntry = "vertMain";
		std::string m_fragmentEntry = "fragMain";
	};
}
