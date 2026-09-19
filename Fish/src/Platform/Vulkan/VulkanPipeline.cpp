#include "VulkanPipeline.h"

#include <stdexcept>

namespace Fish {
	Pipeline::Pipeline(VulkanContext* context, const std::vector<vk::DynamicState>& dynamicStates,
		vk::Format swapChainImageFormat, vk::raii::DescriptorSetLayout& descriptorSetLayout)
	{
		vk::raii::ShaderModule shaderModule = shader::createShaderModule(shader::readFile("shaders/slang.spv"), context->device);

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shaderModule,
			.pName = "vertMain" };

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shaderModule,
			.pName = "fragMain" };

		vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		//固定功能定义

		//动态状态定义
		vk::PipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		//顶点输入格式定义
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		//顶点组合方法和基元重启定义
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
		inputAssembly.primitiveRestartEnable = vk::False;

		// 动态状态里有 eViewport / eScissor,这两个会被录制时设的值覆盖。
		// 但 pViewports / pScissors 不能为空,所以填占位值 —— 数值本身没有意义。
		vk::Viewport viewport{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
		vk::Rect2D   scissor{ vk::Offset2D{ 0, 0 }, vk::Extent2D{ 1, 1 } };

		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		//光栅器定义
		vk::PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.depthClampEnable = vk::False;
		rasterizer.rasterizerDiscardEnable = vk::False;
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = vk::CullModeFlagBits::eBack;
		rasterizer.frontFace = vk::FrontFace::eCounterClockwise,
		rasterizer.depthBiasEnable = vk::False;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		//多重采样定义
		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sampleShadingEnable = vk::False;
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = vk::False;
		multisampling.alphaToOneEnable = vk::False;

		//颜色混合
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachment.blendEnable = vk::True;
		colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
		colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
		colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.logicOpEnable = vk::False;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		//管线layout
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
			.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0
		};

		m_layout = vk::raii::PipelineLayout(context->device, pipelineLayoutInfo);

		//动态渲染:通过 pNext 指定颜色附件格式,替代 render pass / subpass
		vk::PipelineRenderingCreateInfo renderingCreateInfo{};
		renderingCreateInfo.colorAttachmentCount = 1;
		renderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat;

		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.pNext = &renderingCreateInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = *m_layout;
		pipelineInfo.renderPass = nullptr; // 动态渲染下不绑定 render pass
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = nullptr; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		m_pipeline = vk::raii::Pipeline(context->device, nullptr, pipelineInfo);

		// shaderModule 在作用域结束时由 RAII 自动释放
	}
}
