#include "VulkanDescriptorAllocator.h"

#include <array>
#include <chrono>
#include <vector>

namespace Fish {
	DescriptorAllocator::DescriptorAllocator(VulkanContext* context, uint32_t maxSets)
		: m_context(context)
	{
		// layout 定形状:两个 binding,UBO 给顶点阶段,贴图给片元阶段
		std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
			{{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex},
			 {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}} };

		vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };
		m_layout = vk::raii::DescriptorSetLayout(context->device, layoutInfo);

		// 池的配额必须覆盖 layout 里出现的每一种类型,否则分配时
		// vkAllocateDescriptorSets 报 VK_ERROR_OUT_OF_POOL_MEMORY
		std::array<vk::DescriptorPoolSize, 2> poolSize{ {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = maxSets},
												{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = maxSets}} };
		vk::DescriptorPoolCreateInfo          poolInfo{ .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
													   .maxSets = maxSets,
													   .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
													   .pPoolSizes = poolSize.data() };
		m_pool = vk::raii::DescriptorPool(context->device, poolInfo);
	}

	vk::raii::DescriptorSet DescriptorAllocator::allocate(Buffer& ubo,
		vk::raii::ImageView& textureView, vk::raii::Sampler& textureSampler) const
	{
		std::vector<vk::DescriptorSetLayout> layouts{ *m_layout };
		vk::DescriptorSetAllocateInfo        allocInfo{ .descriptorPool = *m_pool,
													   .descriptorSetCount = 1,
													   .pSetLayouts = layouts.data() };

		// 分配 1 个再 move 出来。vk::raii::DescriptorSets 只是 std::vector 的子类、
		// 没有析构函数,被搬走的那个元素在 vector 里变成空句柄,临时对象析构时跳过它。
		vk::raii::DescriptorSet descriptorSet = std::move(m_context->device.allocateDescriptorSets(allocInfo).front());

		vk::DescriptorBufferInfo bufferInfo{ .buffer = ubo.getHandle(), .offset = 0, .range = sizeof(UniformBufferObject) };
		vk::DescriptorImageInfo  imageInfo{ .sampler = textureSampler, .imageView = textureView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };
		std::array<vk::WriteDescriptorSet, 2> descriptorWrites{ {{.dstSet = descriptorSet,
															 .dstBinding = 0,
															 .dstArrayElement = 0,
															 .descriptorCount = 1,
															 .descriptorType = vk::DescriptorType::eUniformBuffer,
															 .pBufferInfo = &bufferInfo},
															{.dstSet = descriptorSet,
															 .dstBinding = 1,
															 .dstArrayElement = 0,
															 .descriptorCount = 1,
															 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
															 .pImageInfo = &imageInfo}} };
		m_context->device.updateDescriptorSets(descriptorWrites, {});

		return descriptorSet;
	}

	void updateUniformBuffer(const vk::Extent2D& extent, Buffer& ubo)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject data{};
		data.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		data.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		data.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 10.0f);
		data.proj[1][1] *= -1;
		memcpy(ubo.map(0, sizeof(data)), &data, sizeof(data));
	}
}
