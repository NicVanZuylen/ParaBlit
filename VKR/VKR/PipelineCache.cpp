#include "PipelineCache.h"
#include "ParaBlitDebug.h"
#include "Renderer.h"
#include "CLib/Vector.h"
#include "PBUtil.h"
#include "MurmurHash3.h"
#include "PBUtil.h"

namespace PB
{
	bool PipelineDesc::operator==(const PipelineDesc& other) const
	{
		bool stageModulesEqual = true;
		for (u32 i = 0; i < static_cast<u32>(EShaderStage::PB_SHADER_STAGE_COUNT); ++i)
			stageModulesEqual &= (m_shaderModules[i] == other.m_shaderModules[i]);
		return m_renderPass == other.m_renderPass && m_subpass == other.m_subpass && stageModulesEqual;
	}

	u64 PipelineDescHasher::operator()(const PipelineDesc& desc) const
	{
		return MurmurHash3_x64_64(&desc, sizeof(PipelineDesc), 0);
	}

	void PipelineCache::Init(Renderer* renderer)
	{
		m_device = renderer->GetDevice();
		m_masterSetLayout = renderer->GetMasterSetLayout();
		m_uboSetLayout = renderer->GetUBOSetLayout();
	}

	void PipelineCache::Destroy()
	{
		for (auto& pipeline : m_pipelineCache)
		{
			vkDestroyPipelineLayout(m_device->GetHandle(), pipeline.second.m_layout, nullptr);
			vkDestroyPipeline(m_device->GetHandle(), pipeline.second.m_pipeline, nullptr);
		}
		m_masterSetLayout = VK_NULL_HANDLE;
		m_uboSetLayout = VK_NULL_HANDLE;
	}

	Pipeline PipelineCache::GetPipeline(const PipelineDesc& desc)
	{
		auto it = m_pipelineCache.find(desc);
		if (it == m_pipelineCache.end())
		{
			auto newPipeline = CreatePipeline(desc);
			return reinterpret_cast<Pipeline>(&(m_pipelineCache[desc] = newPipeline));
		}
		else
			return reinterpret_cast<Pipeline>(&it->second);
	}

	PipelineData PipelineCache::CreatePipeline(const PipelineDesc& desc)
	{
		VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr };
		layoutInfo.flags = 0;

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = 128; // Minimum supported by vulkan spec.
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstant;

		VkDescriptorSetLayout setLayouts[] = { m_masterSetLayout, m_uboSetLayout };
		layoutInfo.setLayoutCount = 2;
		layoutInfo.pSetLayouts = setLayouts;

		VkPipelineLayout layout = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreatePipelineLayout(m_device->GetHandle(), &layoutInfo, nullptr, &layout));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(layout);

		VkRect2D scissor = { static_cast<int64_t>(desc.m_renderArea.x), static_cast<int64_t>(desc.m_renderArea.y), desc.m_renderArea.w, desc.m_renderArea.h };
		VkViewport viewPort;
		viewPort.minDepth = 0.0f;
		viewPort.maxDepth = 1.0f;
		viewPort.x = static_cast<float>(desc.m_renderArea.x);
		viewPort.y = static_cast<float>(desc.m_renderArea.y);
		viewPort.width = static_cast<float>(desc.m_renderArea.w);
		viewPort.height = static_cast<float>(desc.m_renderArea.h);

		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr };
		viewportState.flags = 0;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewPort;

		VkVertexInputBindingDescription vertexBindingDesc;
		vertexBindingDesc.binding = 0;
		vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		vertexBindingDesc.stride = desc.m_vertexDesc.vertexSize;

		u32 vertexAttributeLocation = 0;
		u32 totalVertexAttrOffset = 0;
		CLib::Vector<VkVertexInputAttributeDescription, VertexDesc::MaxVertexAttributes> attrDescriptions;
		for (auto& attrType : desc.m_vertexDesc.vertexAttributes)
		{
			if (attrType == EVertexAttributeType::NONE)
				break;

			VkVertexInputAttributeDescription& vertexAttrDesc = attrDescriptions.PushBack();
			vertexAttrDesc.binding = 0;
			vertexAttrDesc.format = PBVertexTypeToVkFormat(attrType);
			vertexAttrDesc.location = vertexAttributeLocation++;
			vertexAttrDesc.offset = totalVertexAttrOffset;
			totalVertexAttrOffset += static_cast<u32>(attrType);
		}

		VkPipelineVertexInputStateCreateInfo vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
		vertexInputState.flags = 0;
		vertexInputState.vertexAttributeDescriptionCount = attrDescriptions.Count();
		vertexInputState.pVertexAttributeDescriptions = attrDescriptions.Data();
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &vertexBindingDesc;

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
		rasterState.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterState.depthBiasEnable = VK_FALSE;
		rasterState.depthClampEnable = VK_FALSE;
		rasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterState.lineWidth = 1.0f;
		rasterState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterState.rasterizerDiscardEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
		depthStencilState.maxDepthBounds = 1.0f;
		depthStencilState.minDepthBounds = 0.0f;
		depthStencilState.depthBoundsTestEnable = VK_TRUE;
		depthStencilState.depthCompareOp = PBCompareOPtoVKCompareOP(desc.m_depthCompareOP);
		depthStencilState.depthTestEnable = desc.m_depthCompareOP != ECompareOP::ALWAYS;
		depthStencilState.depthWriteEnable = depthStencilState.depthTestEnable; // TODO: This should be a separate toggle from depth test.
		depthStencilState.stencilTestEnable = desc.m_stencilTestEnable;
		depthStencilState.flags = 0;

		VkPipelineColorBlendAttachmentState attachmentState = {};
		attachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		attachmentState.blendEnable = VK_FALSE;
		attachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		attachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		attachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr };
		colorBlendState.flags = 0;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &attachmentState;
		colorBlendState.logicOp = VK_LOGIC_OP_COPY;
		colorBlendState.logicOpEnable = VK_FALSE;
		colorBlendState.blendConstants[0] = 0.0f;
		colorBlendState.blendConstants[1] = 0.0f;
		colorBlendState.blendConstants[2] = 0.0f;
		colorBlendState.blendConstants[3] = 0.0f;

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

		CLib::Vector<VkPipelineShaderStageCreateInfo, static_cast<u32>(EShaderStage::PB_SHADER_STAGE_COUNT)> shaderStages;
		for (u32 i = 0; i < static_cast<u32>(EShaderStage::PB_SHADER_STAGE_COUNT); ++i)
		{
			VkPipelineShaderStageCreateInfo stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
			stage.flags = 0;
			stage.module = reinterpret_cast<VkShaderModule>(desc.m_shaderModules[i]);
			stage.pName = "main";
			stage.pSpecializationInfo = nullptr;
			stage.stage = ConvertPBShaderStageToVK(static_cast<EShaderStage>(i));
			shaderStages.PushBack(stage);
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr };
		pipelineInfo.flags = 0;
		pipelineInfo.renderPass = reinterpret_cast<VkRenderPass>(desc.m_renderPass);
		pipelineInfo.subpass = static_cast<u32>(desc.m_subpass);
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = 0;
		pipelineInfo.layout = layout;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pDynamicState = nullptr;
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
