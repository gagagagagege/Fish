#include "VulkanDescriptorAllocator.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace Fish {
	namespace {

		vk::DescriptorType AsDynamic(vk::DescriptorType type)
		{
			if (type == vk::DescriptorType::eUniformBuffer)
				return vk::DescriptorType::eUniformBufferDynamic;
			if (type == vk::DescriptorType::eStorageBuffer)
				return vk::DescriptorType::eStorageBufferDynamic;
			return type;
		}

	}

	void DescriptorAllocator::AddShaderBindings(VulkanContext* context,
		const std::vector<ReflectedDescriptorBindings::Binding>& bindings)
	{
		m_context = context;

		if (m_layoutFrozen)
			throw std::runtime_error("DescriptorAllocator: layout 已经被引用出去"
				"(分配过描述符集,或者有谁拿走了 layoutHandles),不能再改 —— "
				"shader 必须在第一次绘制之前全部创建");

		bool grew = false;
		for (const ReflectedDescriptorBindings::Binding& incoming : bindings) {
			auto existing = std::ranges::find_if(m_bindings,
				[&](const ReflectedDescriptorBindings::Binding& b) {
					return b.set == incoming.set && b.binding == incoming.binding;
				});

			if (existing == m_bindings.end()) {
				m_bindings.push_back(incoming);
				grew = true;
			}
			else if (existing->type != incoming.type) {
				throw std::runtime_error("DescriptorAllocator: set " + std::to_string(incoming.set) +
					" binding " + std::to_string(incoming.binding) +
					" 被声明成了两种不同的描述符类型");
			}
			// 数组长度必须比。不比的话后声明的那个被静默丢掉,layout 里的数组
			// 偏短,越界读到的是隔壁 set 的内容 —— 建管线、建池子都不报错。
			else if (existing->count != incoming.count) {
				throw std::runtime_error("DescriptorAllocator: set " + std::to_string(incoming.set) +
					" binding " + std::to_string(incoming.binding) + " 被声明成了 " +
					std::to_string(existing->count) + " 和 " + std::to_string(incoming.count) +
					" 两种数组长度");
			}
			else if (existing->stages != incoming.stages) {
				existing->stages |= incoming.stages;
				grew = true;
			}
		}

		if (!grew)
			return;

		std::ranges::sort(m_bindings, [](const auto& a, const auto& b) {
			return a.set != b.set ? a.set < b.set : a.binding < b.binding;
			});

		BuildLayout();
	}

	void DescriptorAllocator::BuildLayout()
	{
		const uint32_t setCount = m_bindings.empty() ? 0 : m_bindings.back().set + 1;

		std::vector<vk::raii::DescriptorSetLayout> layouts;
		layouts.reserve(setCount);

		for (uint32_t set = 0; set < setCount; ++set) {
			std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
			for (const ReflectedDescriptorBindings::Binding& binding : m_bindings) {
				if (binding.set != set)
					continue;
				layoutBindings.push_back({
					.binding = binding.binding,
					.descriptorType = AsDynamic(binding.type),
					.descriptorCount = binding.count,
					.stageFlags = binding.stages
					});
			}

			vk::DescriptorSetLayoutCreateInfo layoutInfo{
				.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
				.pBindings = layoutBindings.data() };
			layouts.emplace_back(m_context->device, layoutInfo);
		}

		m_layouts = std::move(layouts);
	}

	std::vector<vk::DescriptorSetLayout> DescriptorAllocator::layoutHandles()
	{
		m_layoutFrozen = true;

		std::vector<vk::DescriptorSetLayout> handles;
		handles.reserve(m_layouts.size());
		for (const vk::raii::DescriptorSetLayout& layout : m_layouts) {
			handles.push_back(*layout);
		}
		return handles;
	}

	void DescriptorAllocator::BuildPool(uint32_t maxSets)
	{
		std::vector<vk::DescriptorPoolSize> poolSizes;
		for (const ReflectedDescriptorBindings::Binding& binding : m_bindings) {
			const vk::DescriptorType type = AsDynamic(binding.type);
			const uint32_t count = binding.count * maxSets;

			auto existing = std::ranges::find(poolSizes, type, &vk::DescriptorPoolSize::type);
			if (existing != poolSizes.end())
				existing->descriptorCount += count;
			else
				poolSizes.push_back({ .type = type, .descriptorCount = count });
		}

		vk::DescriptorPoolCreateInfo poolInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = maxSets,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data() };
		m_pool = vk::raii::DescriptorPool(m_context->device, poolInfo);
	}

	vk::raii::DescriptorSet DescriptorAllocator::allocateSet(uint32_t set)
	{
		if (m_layouts.empty())
			throw std::runtime_error("DescriptorAllocator: 还没有 shader 登记过绑定,layout 不存在");
		if (set >= m_layouts.size())
			throw std::runtime_error("DescriptorAllocator: set " + std::to_string(set) +
				" 没有任何 shader 声明过");

		if (m_pool == nullptr)
			BuildPool(k_DescriptorSetBudget);

		// 池一建出来就有 set 引用着 layout 了,从此不能再重建
		m_layoutFrozen = true;

		std::vector<vk::DescriptorSetLayout> layouts{ *m_layouts[set] };
		vk::DescriptorSetAllocateInfo        allocInfo{ .descriptorPool = *m_pool,
													   .descriptorSetCount = 1,
													   .pSetLayouts = layouts.data() };

		return std::move(m_context->device.allocateDescriptorSets(allocInfo).front());
	}

	void DescriptorAllocator::writeUniformBuffer(vk::raii::DescriptorSet& set, uint32_t binding,
		const vk::DescriptorBufferInfo& info) const
	{
		vk::WriteDescriptorSet write{
			.dstSet = *set,
			.dstBinding = binding,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
			.pBufferInfo = &info };
		m_context->device.updateDescriptorSets(write, nullptr);
	}

	void DescriptorAllocator::writeCombinedImageSampler(vk::raii::DescriptorSet& set, uint32_t binding,
		const vk::DescriptorImageInfo& info) const
	{
		vk::WriteDescriptorSet write{
			.dstSet = *set,
			.dstBinding = binding,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &info };
		m_context->device.updateDescriptorSets(write, nullptr);
	}

	// 批处理的纹理库用这个:一次写完整个数组绑定,从 firstElement 那格起。
	// 分成 N 次单格写也行,但 dstArrayElement 要自己数,一次写更不容易错位。
	void DescriptorAllocator::writeCombinedImageSamplers(vk::raii::DescriptorSet& set, uint32_t binding,
		uint32_t firstElement, const std::vector<vk::DescriptorImageInfo>& infos) const
	{
		if (infos.empty())
			return;

		vk::WriteDescriptorSet write{
			.dstSet = *set,
			.dstBinding = binding,
			.dstArrayElement = firstElement,
			.descriptorCount = static_cast<uint32_t>(infos.size()),
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = infos.data() };
		m_context->device.updateDescriptorSets(write, nullptr);
	}
}
