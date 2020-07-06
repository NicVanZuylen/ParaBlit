#include "PipelineCache.h"
#include "ParaBlitDebug.h"
#include "Device.h"
#include "DynamicArray.h"
#include "PBUtil.h"
#include "MurmurHash3.h"

namespace PB
{
	bool PipelineDesc::operator==(const PipelineDesc& other) const
	{
		bool stageModulesEqual = true;
		for (u32 i = 0; i < PB_SHADER_STAGE_COUNT; ++i)
			stageModulesEqual &= (m_shaderModules[i] == other.m_shaderModules[i]);
		return m_renderPass == other.m_renderPass && m_subpass == other.m_subpass && stageModulesEqual;
	}

	u64 PipelineDescHasher::operator()(const PipelineDesc& desc) const
	{
		return MurmurHash3_x64_64(&desc, sizeof(PipelineDesc), 0);
	}

	void PipelineCache::Init(Device* device)
	{
		m_device = device;
	}

	void PipelineCache::Destroy()
	{
		for (auto& pipeline : m_pipelineCache)
		{
			vkDestroyPipelineLayout(m_device->GetHandle(), pipeline.second.m_layout, nullptr);
			vkDestroyPipeline(m_device->GetHandle(), pipeline.second.m_pipeline, nullptr);
		}
	}

	Pipeline PipelineCache::GetPipeline(const PipelineDesc& desc)
	{
		auto it = m_pipelineCache.find(desc);
		if (it == m_pipelineCache.end())
		{
			auto newPipeline = CreatePipeline(desc);
			m_pipelineCache[desc] = newPipeline;
			return reinterpret_cast<Pipeline>(newPipeline.m_pipeline);
		}
		else
			return reinterpret_cast<Pipeline>(it->second.m_pipeline);
	}

	PipelineData PipelineCache::CreatePipeline(const PipelineDesc& desc)
	{
		VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr };
		layoutInfo.flags = 0;
		layoutInfo.pushConstantRangeCount = 0;
		layoutInfo.pPushConstantRanges = nullptr;
		layoutInfo.setLayoutCount = 0;
		layoutInfo.pSetLayouts = nullptr;

		VkPipelineLayout layout = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreatePipelineLayout(m_device->GetHandle(), &layoutInfo, nullptr, &layout));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(layout);

		VkRect2D scissor = { 0, 0, 0, 0 };
		VkViewport viewPort;
		viewPort.minDepth = 0.0f;
		viewPort.maxDepth = 1.0f;
		viewPort.x = 0.0f;
		viewPort.y = 0.0f;
		viewPort.width = 0.0f;
		viewPort.height = 0.0f;

		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr };
		viewportState.flags = 0;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewPort;

		VkPipelineVertexInputStateCreateInfo vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
		vertexInputState.flags = 0;
		vertexInputState.vertexAttributeDescriptionCount = 0;
		vertexInputState.pVertexAttributeDescriptions = nullptr;
		vertexInputState.vertexBindingDescriptionCount = 0;
		vertexInputState.pVertexBindingDescriptions = nullptr;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr };
		inputAssemblyState.flags = 0;
		inputAssemblyState.primitiveRestartEnable = VK_FALSE;
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr };
		multisampleState.flags = 0;
		multisampleState.alphaToCoverageEnable = VK_FALSE;
		multisampleState.alphaToOneEnable = VK_FALSE;
		multisampleState.minSampleShading = 1.0f;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.sampleShadingEnable = VK_FALSE;
		multisampleState.pSampleMask = nullptr;

		VkPipelineRasterizationStateCreateInfo rasterState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr };
		rasterState.flags = 0;
		rasterState.cullMode = VK_CULL_MODE_NONE;
		rasterState.depthBiasEnable = VK_FALSE;
		rasterState.depthClampEnable = VK_TRUE;
		rasterState.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterState.lineWidth = 1.0f;
		rasterState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterState.rasterizerDiscardEnable = VK_TRUE;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
		depthStencilState.maxDepthBounds = 1.0f;
		depthStencilState.minDepthBounds = 0.0f;
		depthStencilState.depthBoundsTestEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.depthTestEnable = VK_FALSE; // TODO: Depth testing support.
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		depthStencilState.flags = 0;

		VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr };

		// We need viewport and scissor states to be dynamic to support window resizing without re-creating all pipelines, since doing so could take a considerable amount of time in complex scenes.
		VkDynamicState dynamicStates[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicStatesInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr };
		dynamicStatesInfo.flags = 0;
		dynamicStatesInfo.dynamicStateCount = _countof(dynamicStates);
		dynamicStatesInfo.pDynamicStates = dynamicStates;

		DynamicArray<VkPipelineShaderStageCreateInfo, PB_SHADER_STAGE_COUNT> shaderStages;
		for (u32 i = 0; i < PB_SHADER_STAGE_COUNT; ++i)
		{
			VkPipelineShaderStageCreateInfo stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
			stage.flags = 0;
			stage.module = reinterpret_cast<VkShaderModule>(desc.m_shaderModules[i]);
			stage.pName = "main";
			stage.pSpecializationInfo = nullptr;
			stage.stage = ConvertPBShaderStageToVK(static_cast<EShaderStage>(i));
			shaderStages.Push(stage);
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr };
		pipelineInfo.flags = 0;
		pipelineInfo.renderPass = reinterpret_cast<VkRenderPass>(desc.m_renderPass);
		pipelineInfo.subpass = static_cast<u32>(desc.m_subpass);
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = 0;
		pipelineInfo.layout = layout;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pDynamicState = &dynamicStatesInfo;
		pipelineInfo.pVertexInputState = &vertexInputState;
		pipelineInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pMultisampleState = &multisampleState;
		pipelineInfo.pRasterizationState = &rasterState;
		pipelineInfo.pDepthStencilState = &depthStencilState;
		pipelineInfo.pColorBlendState = &colorBlendState;

		pipelineInfo.pStages = shaderStages.Data();
		pipelineInfo.stageCount = shaderStages.Count();

		VkPipeline newPipeline = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateGraphicsPipelines(m_device->GetHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newPipeline);
		return { layout, newPipeline };
	}
};
