#pragma once
#include "VulkanContext.h"
#include "VulkanShader.h"
#include "VulkanBuffer.h"

#include <vector>

namespace Fish {
	// 图形管线 + 它的 pipelineLayout。两者由同一次构造产生,由同一个对象持有 ——
	// 分开声明的话,layout 先销毁而 pipeline 还活着就是错的。
	//
	// 管线不随交换链重建。extent 变化靠动态 viewport/scissor 吸收
	//(见 TriangleApp 的 dynamicStates),所以它只依赖交换链的 format。
	class Pipeline
	{
	public:
		Pipeline() = default;
		Pipeline(VulkanContext* context, const std::vector<vk::DynamicState>& dynamicStates,
			vk::Format swapChainImageFormat, vk::raii::DescriptorSetLayout& descriptorSetLayout);

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;
		Pipeline(Pipeline&&) = default;
		Pipeline& operator=(Pipeline&&) = default;

		vk::raii::Pipeline&       handle() { return m_pipeline; }
		vk::raii::PipelineLayout& layout() { return m_layout; }

	private:
		// 声明顺序 = 销毁逆序:pipeline 引用 layout 的句柄,所以 pipeline 先声明、后销毁
		vk::raii::Pipeline       m_pipeline = nullptr;
		vk::raii::PipelineLayout m_layout = nullptr;
	};

}
