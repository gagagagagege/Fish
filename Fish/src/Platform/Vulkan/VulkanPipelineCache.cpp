#include "VulkanPipelineCache.h"

#include <string>

namespace Fish {
	namespace {

		// vulkan-hpp 没给句柄做 hash 特化。非分派句柄在 64 位上就是一个指针,
		// reinterpret_cast 两条路都吃得下。
		uint64_t HandleKey(vk::ShaderModule handle)
		{
			return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<VkShaderModule>(handle)));
		}

		void HashCombine(size_t& seed, size_t value)
		{
			seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
		}

		// 逐字段比,不用 memcmp:结构体里可能有填充字节。
		// 不写成 operator== —— vulkan-hpp 给结构体也生成了 operator==,会撞在 ADL 上。
		bool SameBlendState(const vk::PipelineColorBlendAttachmentState& a,
			const vk::PipelineColorBlendAttachmentState& b)
		{
			return a.blendEnable == b.blendEnable
				&& a.srcColorBlendFactor == b.srcColorBlendFactor
				&& a.dstColorBlendFactor == b.dstColorBlendFactor
				&& a.colorBlendOp == b.colorBlendOp
				&& a.srcAlphaBlendFactor == b.srcAlphaBlendFactor
				&& a.dstAlphaBlendFactor == b.dstAlphaBlendFactor
				&& a.alphaBlendOp == b.alphaBlendOp
				&& a.colorWriteMask == b.colorWriteMask;
		}

	}

	bool operator==(const PipelineDesc& a, const PipelineDesc& b)
	{
		return a.vertexModule == b.vertexModule
			&& a.fragmentModule == b.fragmentModule
			&& a.vertexEntry == b.vertexEntry
			&& a.fragmentEntry == b.fragmentEntry
			&& a.vertexStride == b.vertexStride
			&& a.topology == b.topology
			&& a.cullMode == b.cullMode
			&& a.frontFace == b.frontFace
			&& a.polygonMode == b.polygonMode
			&& a.lineWidth == b.lineWidth
			&& a.samples == b.samples
			&& SameBlendState(a.blendState, b.blendState);
	}

	size_t PipelineDescHash::operator()(const PipelineDesc& desc) const
	{
		size_t seed = 0;
		HashCombine(seed, std::hash<uint64_t>{}(HandleKey(desc.vertexModule)));
		HashCombine(seed, std::hash<uint64_t>{}(HandleKey(desc.fragmentModule)));
		HashCombine(seed, std::hash<std::string_view>{}(desc.vertexEntry));
		HashCombine(seed, std::hash<std::string_view>{}(desc.fragmentEntry));
		HashCombine(seed, std::hash<uint32_t>{}(desc.vertexStride));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.topology)));
		HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(static_cast<VkCullModeFlags>(desc.cullMode))));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.frontFace)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.polygonMode)));
		HashCombine(seed, std::hash<float>{}(desc.lineWidth));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.samples)));
		HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.blendState.blendEnable)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.blendState.srcColorBlendFactor)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.blendState.dstColorBlendFactor)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.blendState.colorBlendOp)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.blendState.srcAlphaBlendFactor)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.blendState.dstAlphaBlendFactor)));
		HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.blendState.alphaBlendOp)));
		HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(desc.blendState.colorWriteMask)));
		return seed;
	}

	PipelineCache::PipelineCache(VulkanContext* context,
		std::vector<vk::DescriptorSetLayout> setLayouts,
		std::vector<vk::PushConstantRange>   pushConstantRanges,
		vk::Format swapChainFormat)
		: m_context(context)
		, m_setLayouts(std::move(setLayouts))
		, m_pushConstants(std::move(pushConstantRanges))
		, m_swapChainFormat(swapChainFormat)
	{
		vk::PipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.setLayoutCount = static_cast<uint32_t>(m_setLayouts.size());
		layoutInfo.pSetLayouts = m_setLayouts.data();
		layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstants.size());
		layoutInfo.pPushConstantRanges = m_pushConstants.data();

		m_layout = vk::raii::PipelineLayout(context->device, layoutInfo);
	}

	vk::raii::Pipeline& PipelineCache::GetOrCreate(const PipelineDesc& desc,
		const ReflectedVertexInput& reflectedVertexInput)
	{
		auto existing = m_pipelines.find(desc);
		if (existing != m_pipelines.end())
			return existing->second;

		const VulkanVertexInput vertexInput = BuildVertexInput(reflectedVertexInput, desc.vertexStride);

		// pName 要的是以 '\0' 结尾的 C 串,string_view 不保证。
		// 拷成 string 只在建新管线时走一次,不在查找路径上。
		const std::string vertexEntry(desc.vertexEntry);
		const std::string fragmentEntry(desc.fragmentEntry);

		vk::PipelineShaderStageCreateInfo shaderStages[] = {
			{ .stage = vk::ShaderStageFlagBits::eVertex,   .module = desc.vertexModule,   .pName = vertexEntry.c_str() },
			{ .stage = vk::ShaderStageFlagBits::eFragment, .module = desc.fragmentModule, .pName = fragmentEntry.c_str() }
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInput.bindings.size());
		vertexInputInfo.pVertexBindingDescriptions = vertexInput.bindings.data();
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInput.attributes.size());
		vertexInputInfo.pVertexAttributeDescriptions = vertexInput.attributes.data();

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.topology = desc.topology;
		inputAssembly.primitiveRestartEnable = vk::False;

		// 动态状态里有 eViewport / eScissor,这两个会被录制时设的值覆盖。
		// 但 pViewports / pScissors 不能为空,所以填占位值。
		vk::Viewport viewport{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
		vk::Rect2D   scissor{ vk::Offset2D{ 0, 0 }, vk::Extent2D{ 1, 1 } };

		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		vk::PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.depthClampEnable = vk::False;
		rasterizer.rasterizerDiscardEnable = vk::False;
		rasterizer.polygonMode = desc.polygonMode;
		rasterizer.lineWidth = desc.lineWidth;
		rasterizer.cullMode = desc.cullMode;
		rasterizer.frontFace = desc.frontFace;
		rasterizer.depthBiasEnable = vk::False;

		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sampleShadingEnable = vk::False;
		multisampling.rasterizationSamples = desc.samples;
		multisampling.minSampleShading = 1.0f;

		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.logicOpEnable = vk::False;
		colorBlending.logicOp = vk::LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &desc.blendState;

		const std::vector<vk::DynamicState> dynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		vk::PipelineRenderingCreateInfo renderingCreateInfo{};
		renderingCreateInfo.colorAttachmentCount = 1;
		renderingCreateInfo.pColorAttachmentFormats = &m_swapChainFormat;

		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.pNext = &renderingCreateInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = *m_layout;
		pipelineInfo.renderPass = nullptr;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = nullptr;
		pipelineInfo.basePipelineIndex = -1;

		// 最后一个参数是驱动级持久化缓存,和这个类的内存缓存是两回事。
		vk::raii::Pipeline pipeline(m_context->device, nullptr, pipelineInfo);

		auto inserted = m_pipelines.emplace(desc, std::move(pipeline));
		return inserted.first->second;
	}

	void PipelineCache::Invalidate(vk::Format newFormat)
	{
		m_pipelines.clear();
		m_swapChainFormat = newFormat;
	}

}
