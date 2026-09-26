#include "VulkanReflection.h"

#include "spirv_reflect.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace Fish {
	namespace {

		// SpvReflectFormat / SpvReflectDescriptorType / SpvReflectShaderStageFlagBits
		// 的枚举值就是对应的 Vk 值 —— SPIRV-Reflect 的头文件里逐条注着
		// "= VK_FORMAT_..." / "= VK_DESCRIPTOR_TYPE_..."。
		template <typename T>
		T ToVk(int value)
		{
			return static_cast<T>(value);
		}

		void Check(SpvReflectResult result, const char* what)
		{
			if (result != SPV_REFLECT_RESULT_SUCCESS)
				throw std::runtime_error(std::string("SPIRV-Reflect: ") + what +
					" 失败,错误码 " + std::to_string(static_cast<int>(result)));
		}

		// 把 spvReflectDestroyShaderModule 挂到析构上 —— 中途抛异常也不会漏。
		class ReflectedModule
		{
		public:
			explicit ReflectedModule(const std::vector<uint32_t>& spirv)
			{
				Check(spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t),
					spirv.data(), &m_module), "创建模块");
			}
			~ReflectedModule() { spvReflectDestroyShaderModule(&m_module); }

			ReflectedModule(const ReflectedModule&) = delete;
			ReflectedModule& operator=(const ReflectedModule&) = delete;

			const SpvReflectShaderModule* get() const { return &m_module; }

		private:
			SpvReflectShaderModule m_module{};
		};

	}

	ReflectedVertexInput ReflectVertexInput(const std::vector<uint32_t>& spirv, const char* vertexEntry)
	{
		const ReflectedModule module(spirv);

		uint32_t count = 0;
		Check(spvReflectEnumerateEntryPointInputVariables(module.get(), vertexEntry, &count, nullptr),
			"输入变量计数");
		if (count == 0)
			throw std::runtime_error(std::string("SPIRV-Reflect: 入口点 ") + vertexEntry + " 没有输入变量");

		std::vector<SpvReflectInterfaceVariable*> variables(count);
		Check(spvReflectEnumerateEntryPointInputVariables(module.get(), vertexEntry, &count, variables.data()),
			"枚举输入变量");

		ReflectedVertexInput result;
		result.attributes.reserve(count);
		for (const SpvReflectInterfaceVariable* variable : variables) {
			// builtin(gl_Position / gl_VertexIndex 之类)不占 location,不是顶点属性。
			if (variable->built_in != -1)
				continue;

			if (variable->numeric.matrix.column_count != 0)
				throw std::runtime_error("SPIRV-Reflect: 顶点属性 " + std::string(variable->name) +
					" 是矩阵,还没支持");

			const uint32_t components = std::max(1u, variable->numeric.vector.component_count);
			const uint32_t size = (variable->numeric.scalar.width / 8) * components;

			result.attributes.push_back({ variable->location, ToVk<vk::Format>(variable->format), size });
		}

		std::ranges::sort(result.attributes, {}, &ReflectedVertexInput::Attribute::location);
		return result;
	}

	ReflectedDescriptorBindings ReflectDescriptorBindings(const std::vector<uint32_t>& spirv)
	{
		const ReflectedModule module(spirv);
		const SpvReflectShaderModule* reflected = module.get();

		ReflectedDescriptorBindings result;
		for (uint32_t i = 0; i < reflected->entry_point_count; ++i) {
			const SpvReflectEntryPoint& entryPoint = reflected->entry_points[i];
			const vk::ShaderStageFlags stages = ToVk<vk::ShaderStageFlags>(entryPoint.shader_stage);

			for (uint32_t j = 0; j < entryPoint.descriptor_set_count; ++j) {
				const SpvReflectDescriptorSet& set = entryPoint.descriptor_sets[j];
				for (uint32_t k = 0; k < set.binding_count; ++k) {
					const SpvReflectDescriptorBinding& binding = *set.bindings[k];
					const vk::DescriptorType type = ToVk<vk::DescriptorType>(binding.descriptor_type);

					// 同一个 binding 被两个 stage 都用到时,stage 位取并集。
					// 数量很小,线性找够了。
					auto existing = std::ranges::find_if(result.bindings,
						[&](const ReflectedDescriptorBindings::Binding& b) {
							return b.set == set.set && b.binding == binding.binding && b.type == type;
						});
					if (existing != result.bindings.end()) {
						existing->stages |= stages;
						existing->count = std::max(existing->count, binding.count);
					}
					else {
						result.bindings.push_back({ set.set, binding.binding, type, stages, binding.count });
					}
				}
			}
		}

		std::ranges::sort(result.bindings, [](const auto& a, const auto& b) {
			return a.set != b.set ? a.set < b.set : a.binding < b.binding;
			});
		return result;
	}

	VulkanVertexInput BuildVertexInput(const ReflectedVertexInput& reflected, uint32_t stride)
	{
		VulkanVertexInput result;
		if (reflected.attributes.empty())
			return result;

		result.bindings.push_back({
			.binding = 0,
			.stride = stride,
			.inputRate = vk::VertexInputRate::eVertex
			});

		// 一个 attribute 最多 4 个分量,而 vk::Format 已经把分量数定死了;
		// offset 只能靠前一串属性的字节数累加推出来。
		uint32_t offset = 0;
		result.attributes.reserve(reflected.attributes.size());
		for (const ReflectedVertexInput::Attribute& attribute : reflected.attributes) {
			result.attributes.push_back({
				.location = attribute.location,
				.binding = 0,
				.format = attribute.format,
				.offset = offset
				});
			offset += attribute.size;
		}

		// stride 是调用方给的,属性加起来的字节数可能比它小(尾部有余量,正常),
		// 但不可能比它大 —— 那说明顶点数据装不下 shader 要的属性。
		if (offset > stride)
			throw std::runtime_error("SPIRV-Reflect: 顶点属性共 " + std::to_string(offset) +
				" 字节,超过调用方给的 stride " + std::to_string(stride));

		return result;
	}

}
