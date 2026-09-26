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

	// 让一条管线区别于另一条的全部输入,相等就复用
	struct PipelineDesc
	{
		//shader能反映顶点layout和描述符layout,所以只判断shader
		vk::ShaderModule vertexModule   = nullptr;
		vk::ShaderModule fragmentModule = nullptr;
		std::string_view vertexEntry    = "vertMain";
		std::string_view fragmentEntry  = "fragMain";

		uint32_t vertexStride = 0;

		vk::PrimitiveTopology topology    = vk::PrimitiveTopology::eTriangleList;
		// 2D 一律不剔除
		vk::CullModeFlags     cullMode    = vk::CullModeFlagBits::eNone;
		vk::FrontFace         frontFace   = vk::FrontFace::eCounterClockwise;
		vk::PolygonMode       polygonMode = vk::PolygonMode::eFill;
		float                 lineWidth   = 1.0f;
		vk::SampleCountFlagBits samples   = vk::SampleCountFlagBits::e1;

		vk::PipelineColorBlendAttachmentState blendState = DefaultAlphaBlend();
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
			std::vector<vk::PushConstantRange>   pushConstantRanges,
			vk::Format swapChainFormat);

		PipelineCache(const PipelineCache&) = delete;
		PipelineCache& operator=(const PipelineCache&) = delete;
		PipelineCache(PipelineCache&&) = default;
		PipelineCache& operator=(PipelineCache&&) = default;

		// reflectedVertexInput 只在未命中时才用得上 —— 命中路径只比 PipelineDesc。
		// 返回的是 map 里那条管线的引用,map 扩容会让它悬空,所以只在本帧用完就丢。
		vk::raii::Pipeline& GetOrCreate(const PipelineDesc& desc,
			const ReflectedVertexInput& reflectedVertexInput);

		vk::raii::PipelineLayout& layout() { return m_layout; }

		vk::Format swapChainFormat() const { return m_swapChainFormat; }

		// 交换链 format 变了就整体作废 —— 旧的永远命不中,留着只占显存。
		// 调用点在 VulkanRendererAPI::EnsureRenderState。
		void Invalidate(vk::Format newFormat);

	private:
		// 声明顺序 = 销毁逆序:pipeline 引用 layout 的句柄,所以 map 在 layout 前面。
		std::unordered_map<PipelineDesc, vk::raii::Pipeline, PipelineDescHash> m_pipelines;
		vk::raii::PipelineLayout m_layout = nullptr;

		VulkanContext*                       m_context        = nullptr;
		std::vector<vk::DescriptorSetLayout> m_setLayouts;
		std::vector<vk::PushConstantRange>   m_pushConstants;
		vk::Format                           m_swapChainFormat = vk::Format::eUndefined;
	};

}
