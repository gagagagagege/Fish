#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "Renderer/Shader.h"
#include "VulkanReflection.h"

#include <fstream>
#include <string>
#include <vector>

namespace Fish {
	// SPIR-V 文件与模块。原来是单独一个 VulkanShader.h,而那个文件里其实
	// 没有 VulkanShader 类 —— 和下面那个撞名,所以并到一起。
	class shader
	{
	public:
		static std::vector<char> readFile(const std::string& filename);
		static vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device& device);
	};

	class VulkanContext;
	class DescriptorAllocator;

	// Fish::Shader 在 Vulkan 上的实现。
	// 一个 .spv 里两个入口点,顶点和片元共用同一个 vk::ShaderModule。
	// 构造时反射出顶点属性和描述符绑定:前者留给管线缓存,后者交给 DescriptorAllocator。
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
