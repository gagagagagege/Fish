#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "VulkanContext.h"
#include "VulkanReflection.h"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Fish {

	inline vk::PipelineColorBlendAttachmentState DefaultAlphaBlend()
	{
		return {
			.blendEnable         = vk::True,
			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			.colorBlendOp        = vk::BlendOp::eAdd,
			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			.alphaBlendOp        = vk::BlendOp::eAdd,
			.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			                       vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};
	}

	// 不混合。要用 alpha 的管线自己换成 DefaultAlphaBlend。
	inline vk::PipelineColorBlendAttachmentState DefaultOpaqueBlend()
	{
		return {
			.blendEnable    = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};
	}

	// 让一条管线区别于另一条的全部输入,相等就复用
	struct PipelineDesc
	{
		//shader能反映顶点layout和描述符layout,所以只判断shader
		vk::ShaderModule vertexModule   = nullptr;
		vk::ShaderModule fragmentModule = nullptr;
		std::string_view vertexEntry    = "vertMain";
		std::string_view fragmentEntry  = "fragMain";

		uint32_t vertexStride = 0;

		vk::PrimitiveTopology topology  = vk::PrimitiveTopology::eTriangleList;
		vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

		// 附件格式进键:离屏目标和交换链可能同时存在,格式不同就是两条管线
		vk::Format colorFormat = vk::Format::eUndefined;
		vk::Format depthFormat = vk::Format::eUndefined;

		vk::PipelineColorBlendAttachmentState blendState = DefaultOpaqueBlend();
	};

	bool operator==(const PipelineDesc& a, const PipelineDesc& b);

	struct PipelineDescHash
	{
		size_t operator()(const PipelineDesc& desc) const;
	};

	class PipelineCache
	{
	public:
		PipelineCache() = default;
		PipelineCache(VulkanContext* context,
			std::vector<vk::DescriptorSetLayout> setLayouts,
			std::vector<vk::PushConstantRange>   pushConstantRanges);

		PipelineCache(const PipelineCache&) = delete;
		PipelineCache& operator=(const PipelineCache&) = delete;
		PipelineCache(PipelineCache&&) = default;
		PipelineCache& operator=(PipelineCache&&) = default;

		// reflectedVertexInput 只在未命中时才用得上 —— 命中路径只比 PipelineDesc。
		// 返回的是 map 里那条管线的引用,map 扩容会让它悬空,所以只在本帧用完就丢。
		vk::raii::Pipeline& GetOrCreate(const PipelineDesc& desc,
			const ReflectedVertexInput& reflectedVertexInput);

		vk::raii::PipelineLayout& layout() { return m_layout; }

	private:
		std::unordered_map<PipelineDesc, vk::raii::Pipeline, PipelineDescHash> m_pipelines;
		vk::raii::PipelineLayout m_layout = nullptr;

		VulkanContext*                       m_context        = nullptr;
		std::vector<vk::DescriptorSetLayout> m_setLayouts;
		std::vector<vk::PushConstantRange>   m_pushConstants;
	};

}
