#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <vector>

namespace Fish {

	// 顶点属性。location 和 format 能从 SPIR-V 反射出来。
	// binding 号和 stride 反射不出来 —— SPIR-V 里顶点输入只有 Location 装饰,
	// 没有 Binding,也没有每笔顶点的字节数。那两样由调用方补,见 BuildVertexInput。
	struct ReflectedVertexInput
	{
		struct Attribute
		{
			uint32_t   location;
			vk::Format format;
			uint32_t   size;     // 字节。= 标量宽度 × 分量数,推 offset 用
		};
		std::vector<Attribute> attributes;   // 按 location 升序
	};

	// 描述符绑定。descriptorType 是 shader 声明的那个 —— SPIR-V 表达不了
	// "dynamic offset",所以 ConstantBuffer 反射出来是 eUniformBuffer 而不是
	// eUniformBufferDynamic。要不要用 dynamic 由建 layout 的那一方决定。
	struct ReflectedDescriptorBindings
	{
		struct Binding
		{
			uint32_t             set;
			uint32_t             binding;
			vk::DescriptorType   type;
			vk::ShaderStageFlags stages;
			uint32_t             count;      // 数组绑定才是 >1

			// 用来比"后一个 shader 声明的绑定和前一个是否一致"
			bool operator==(const Binding&) const = default;
		};
		std::vector<Binding> bindings;       // 按 set、binding 升序
	};

	// 只取 vertexEntry 这个入口点的 Input 变量。
	//
	// 不能用模块级的 input_variables:字段注释写着 "Uses value(s) from first
	// entry point",而且片元的插值输入也是 Input 存储类 —— 把它当顶点属性,
	// 会多出属性和 location 冲突,验证层报的是 Input 那条 VUID。
	ReflectedVertexInput ReflectVertexInput(const std::vector<uint32_t>& spirv, const char* vertexEntry);

	// 遍历所有入口点后合并。跨入口点合并是必须的:模块级那份只反映第一个
	// 入口点,而 .spv 里顶点入口在前,只取模块级会漏掉片元用的那张贴图。
	ReflectedDescriptorBindings ReflectDescriptorBindings(const std::vector<uint32_t>& spirv);

	struct VulkanVertexInput
	{
		std::vector<vk::VertexInputBindingDescription>   bindings;
		std::vector<vk::VertexInputAttributeDescription> attributes;
	};

	// offset 由 location 顺序做前缀和,stride 由调用方给。
	// 所以属性必须从 offset 0 起紧密排,"宽松"只能宽松在尾部。
	// Playground 的 flat color 就是这种:shader 只声明 location 0,而顶点数据
	// 是 pos3+uv2 共 20 字节 —— 按属性自己加起来会算成 12,读到的位置就是错的。
	VulkanVertexInput BuildVertexInput(const ReflectedVertexInput& reflected, uint32_t stride);

}
