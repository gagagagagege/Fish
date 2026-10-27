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
			// 运行时数组两边反射回来的都是 0(哨兵值乘进去了),所以不能只看数字:
			// 0 == 0 会把"一边写 [] 一边写 [64]"放过,而要拿后者去建 layout。
			else if (existing->runtimeArray != incoming.runtimeArray ||
				(!incoming.runtimeArray && existing->count != incoming.count)) {
				throw std::runtime_error("DescriptorAllocator: set " + std::to_string(incoming.set) +
					" binding " + std::to_string(incoming.binding) + " 被声明成了两种数组:一边是 " +
					(existing->runtimeArray ? "[]" : "[" + std::to_string(existing->count) + "]") +
					",一边是 " +
					(incoming.runtimeArray ? "[]" : "[" + std::to_string(incoming.count) + "]"));
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
			const bool updateAfterBind = IsUpdateAfterBindSet(set);

			std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
			std::vector<vk::DescriptorBindingFlags>      bindingFlags;
			for (const ReflectedDescriptorBindings::Binding& binding : m_bindings) {
				if (binding.set != set)
					continue;
				layoutBindings.push_back({
					.binding = binding.binding,
					.descriptorType = AsDynamic(binding.type),
					.descriptorCount = DescriptorCount(binding),
					.stageFlags = binding.stages
					});

				// 表那套:格数由 layout 定,而且是边走边填,所以两个 flag 都要。
				// ePartiallyBound 是给"还没写过的格"留的 —— 贴图是随时创建的,
				// 表里天然有空洞,而验证层证明不了运行期的 texId 指到哪儿。
				vk::DescriptorBindingFlags flags{};
				if (updateAfterBind) {
					flags |= vk::DescriptorBindingFlagBits::eUpdateAfterBind;
					flags |= vk::DescriptorBindingFlagBits::ePartiallyBound;
				}
				bindingFlags.push_back(flags);
			}

			vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
				.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
				.pBindingFlags = bindingFlags.data() };

			vk::DescriptorSetLayoutCreateInfo layoutInfo{
				.pNext = &flagsInfo,
				// VUID-VkDescriptorSetLayoutCreateInfo-flags-03000:binding 上带了
				// eUpdateAfterBind,layout 的 flags 就必须带这个。
				.flags = updateAfterBind
					? vk::DescriptorSetLayoutCreateFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
					: vk::DescriptorSetLayoutCreateFlags{},
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

	// 运行时数组(Texture2D textures[] / SamplerState samplers[])反射回来的 count
	// 是 0 —— 反射把维度那个哨兵值 0 乘进去了。格数只能由 layout 这侧定,而它
	// 同时也是槽位分配器的上界。两种描述符各读各的上限。
	uint32_t DescriptorAllocator::DescriptorCount(const ReflectedDescriptorBindings::Binding& binding)
	{
		if (!binding.runtimeArray)
			return binding.count;

		if (binding.type == vk::DescriptorType::eSampledImage)
			return sampledImageTableSize();
		if (binding.type == vk::DescriptorType::eSampler)
			return samplerTableSize();

		throw std::runtime_error("DescriptorAllocator: set " + std::to_string(binding.set) +
			" binding " + std::to_string(binding.binding) + " 的运行时数组描述符类型是 " +
			std::to_string(static_cast<int>(binding.type)) + ",没有对应的格数来源");
	}

	uint32_t DescriptorAllocator::ComputeTableSize(vk::DescriptorType type)
	{
		const vk::PhysicalDeviceDescriptorIndexingProperties indexing =
			m_context->physicalDevice
				.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorIndexingProperties>()
				.get<vk::PhysicalDeviceDescriptorIndexingProperties>();

		// 表那条 binding 带 updateAfterBind,所以算的是 UAB 的那套 limit,
		// 不是非 UAB 的那几个 —— 两组值可以不一样。
		// per-stage 和 per-set 两条都要满足:binding 的 descriptorCount 不能超过任何一个。
		if (type == vk::DescriptorType::eSampledImage) {
			return std::min({ indexing.maxPerStageDescriptorUpdateAfterBindSampledImages,
							  indexing.maxDescriptorSetUpdateAfterBindSampledImages,
							  k_MaxBindlessTextures });
		}
		if (type == vk::DescriptorType::eSampler) {
			return std::min({ indexing.maxPerStageDescriptorUpdateAfterBindSamplers,
							  indexing.maxDescriptorSetUpdateAfterBindSamplers,
							  k_MaxBindlessSamplers });
		}

		throw std::runtime_error("DescriptorAllocator: 描述符类型 " +
			std::to_string(static_cast<int>(type)) + " 不支持运行时数组");
	}

	uint32_t DescriptorAllocator::sampledImageTableSize()
	{
		if (m_sampledImageTableSize != 0)
			return m_sampledImageTableSize;

		if (m_context == nullptr)
			throw std::runtime_error("DescriptorAllocator: 还没有 shader 登记过绑定,"
				"查不了设备的描述符上限 —— 贴图必须在第一个 shader 之后创建");

		m_sampledImageTableSize = ComputeTableSize(vk::DescriptorType::eSampledImage);
		if (m_sampledImageTableSize == 0)
			throw std::runtime_error("DescriptorAllocator: 设备报的 updateAfterBind 采样图上限是 0");

		return m_sampledImageTableSize;
	}

	uint32_t DescriptorAllocator::samplerTableSize()
	{
		if (m_samplerTableSize != 0)
			return m_samplerTableSize;

		if (m_context == nullptr)
			throw std::runtime_error("DescriptorAllocator: 还没有 shader 登记过绑定,"
				"查不了设备的描述符上限 —— 采样器必须在第一个 shader 之后创建");

		m_samplerTableSize = ComputeTableSize(vk::DescriptorType::eSampler);
		if (m_samplerTableSize == 0)
			throw std::runtime_error("DescriptorAllocator: 设备报的 updateAfterBind 采样器上限是 0");

		return m_samplerTableSize;
	}

	void DescriptorAllocator::BuildPool(uint32_t set, uint32_t maxSets)
	{
		std::vector<vk::DescriptorPoolSize> poolSizes;
		for (const ReflectedDescriptorBindings::Binding& binding : m_bindings) {
			if (binding.set != set)
				continue;

			const vk::DescriptorType type = AsDynamic(binding.type);
			const uint32_t count = DescriptorCount(binding) * maxSets;

			auto existing = std::ranges::find(poolSizes, type, &vk::DescriptorPoolSize::type);
			if (existing != poolSizes.end())
				existing->descriptorCount += count;
			else
				poolSizes.push_back({ .type = type, .descriptorCount = count });
		}

		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		// VUID-VkDescriptorSetAllocateInfo-pSetLayouts-03044:带 eUpdateAfterBindPool
		// 的 layout 只能从带这个 flag 的池里分。反向没约束,不带的 layout 从哪个池
		// 里分都行 —— 所以只给表那个池加。
		if (IsUpdateAfterBindSet(set))
			flags |= vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;

		vk::DescriptorPoolCreateInfo poolInfo{
			.flags = flags,
			.maxSets = maxSets,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data() };
		m_pools[PoolIndex(set)] = vk::raii::DescriptorPool(m_context->device, poolInfo);
	}

	vk::raii::DescriptorSet DescriptorAllocator::allocateSet(uint32_t set)
	{
		if (m_layouts.empty())
			throw std::runtime_error("DescriptorAllocator: 还没有 shader 登记过绑定,layout 不存在");
		if (set >= m_layouts.size())
			throw std::runtime_error("DescriptorAllocator: set " + std::to_string(set) +
				" 没有任何 shader 声明过");

		// 池按 set 分家:表那个池只装一个 set,不能被 k_DescriptorSetBudget 放大。
		const uint32_t poolIndex = PoolIndex(set);
		if (m_pools[poolIndex] == nullptr) {
			BuildPool(set, IsUpdateAfterBindSet(set) ? k_TextureTableSetCount : k_DescriptorSetBudget);
		}

		// 池一建出来就有 set 引用着 layout 了,从此不能再重建
		m_layoutFrozen = true;

		std::vector<vk::DescriptorSetLayout> layouts{ *m_layouts[set] };
		vk::DescriptorSetAllocateInfo        allocInfo{ .descriptorPool = *m_pools[poolIndex],
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

	// 一次写完整个数组绑定,从 firstElement 那格起。分成 N 次单格写也行,
	// 但 dstArrayElement 要自己数,一次写更不容易错位。
	void DescriptorAllocator::writeSampledImages(vk::raii::DescriptorSet& set, uint32_t binding,
		uint32_t firstElement, const std::vector<vk::DescriptorImageInfo>& infos) const
	{
		if (infos.empty())
			return;

		vk::WriteDescriptorSet write{
			.dstSet = *set,
			.dstBinding = binding,
			.dstArrayElement = firstElement,
			.descriptorCount = static_cast<uint32_t>(infos.size()),
			.descriptorType = vk::DescriptorType::eSampledImage,
			.pImageInfo = infos.data() };
		m_context->device.updateDescriptorSets(write, nullptr);
	}

	// 采样器走的是同一个 vk::DescriptorImageInfo —— 里面只填 sampler 那一项。
	void DescriptorAllocator::writeSamplers(vk::raii::DescriptorSet& set, uint32_t binding,
		uint32_t firstElement, const std::vector<vk::DescriptorImageInfo>& infos) const
	{
		if (infos.empty())
			return;

		vk::WriteDescriptorSet write{
			.dstSet = *set,
			.dstBinding = binding,
			.dstArrayElement = firstElement,
			.descriptorCount = static_cast<uint32_t>(infos.size()),
			.descriptorType = vk::DescriptorType::eSampler,
			.pImageInfo = infos.data() };
		m_context->device.updateDescriptorSets(write, nullptr);
	}
}
