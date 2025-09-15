#include "PipelineCache.h"
#include "ParaBlitDebug.h"
#include "Renderer.h"
#include "CLib/Vector.h"
#include "PBUtil.h"
#include "MurmurHash/MurmurHash3.h"

namespace PB
{
	PFN_vkCreateRayTracingPipelinesKHR PipelineCache::vkCreateRayTracingPipelinesKHRFunc;
	PFN_vkGetRayTracingShaderGroupHandlesKHR PipelineCache::vkGetRayTracingShaderGroupHandlesKHRFunc;

	bool GraphicsPipelineDesc::operator==(const GraphicsPipelineDesc& other) const
	{
		bool stageModulesEqual = true;
		for (u32 i = 0; i < static_cast<u32>(EGraphicsShaderStage::GRAPHICS_STAGE_COUNT); ++i)
			stageModulesEqual &= (m_shaderModules[i] == other.m_shaderModules[i]);
		return m_renderPass == other.m_renderPass
			&& memcmp(&m_renderArea, &other.m_renderArea, sizeof(PB::Rect)) == 0
			&& memcmp(m_vertexBuffers, other.m_vertexBuffers, sizeof(m_vertexBuffers)) == 0
			&& memcmp(&m_vertexDesc, &other.m_vertexDesc, sizeof(VertexAttributeDesc)) == 0
			&& memcmp(m_colorBlendStates, other.m_colorBlendStates, sizeof(m_colorBlendStates)) == 0
			&& m_depthCompareOP == other.m_depthCompareOP
			&& m_stencilTestEnable == other.m_stencilTestEnable
			&& m_cullMode == other.m_cullMode
			&& m_subpass == other.m_subpass
			&& m_attachmentCount == other.m_attachmentCount
			&& m_topology == other.m_topology
			&& m_depthWriteEnable == other.m_depthWriteEnable
			&& stageModulesEqual;
	}

	bool ComputePipelineDesc::operator == (const ComputePipelineDesc& other) const
	{
		return m_computeModule == other.m_computeModule;
	}

	bool RayTracingPipelineDesc::operator == (const RayTracingPipelineDesc& other) const
	{
		bool stageModulesEqual = rayGenShaderModule == other.rayGenShaderModule;
		if (stageModulesEqual)
		{
			for (u32 i = 0; i < RayTracingPipelineDesc::MaxShadersPerStage; ++i)
			{
				stageModulesEqual &= (missShaderModules[i] == other.missShaderModules[i]);
				stageModulesEqual &= (closestHitShaderModules[i] == other.closestHitShaderModules[i]);
			}
		}

		return stageModulesEqual && other.maxPipelineRecursionCount == maxPipelineRecursionCount;
	}

	u64 PipelineDescHasher::operator()(const GraphicsPipelineDesc& desc) const
	{
		return MurmurHash3_x64_64(&desc, sizeof(GraphicsPipelineDesc), 0);
	}

	u64 PipelineDescHasher::operator()(const ComputePipelineDesc& desc) const
	{
		return MurmurHash3_x64_64(&desc, sizeof(ComputePipelineDesc), 0);
	}

	u64 PipelineDescHasher::operator()(const RayTracingPipelineDesc& desc) const
	{
		return MurmurHash3_x64_64(&desc, sizeof(RayTracingPipelineDesc), 0);
	}

	void PipelineCache::GetExtensionFunctions(VkInstance instance)
	{
		vkCreateRayTracingPipelinesKHRFunc = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(instance, "vkCreateRayTracingPipelinesKHR");
		vkGetRayTracingShaderGroupHandlesKHRFunc = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(instance, "vkGetRayTracingShaderGroupHandlesKHR");
	}

	void PipelineCache::Init(Renderer* renderer)
	{
		m_renderer = renderer;
		m_device = renderer->GetDevice();
		m_masterSetLayout = renderer->GetMasterSetLayout();
		m_uboSetLayout = renderer->GetUBOSetLayout();
		m_asSetLayout = renderer->GetAccelerationStructureSetLayout();

		CreateCommonPipelineLayouts();
	}

	void PipelineCache::Destroy()
	{
		for (auto& pipeline : m_graphicsPipelineCache)
		{
			if(pipeline.second.m_layout != m_commonGraphicsPipelineLayout
				&& pipeline.second.m_layout != m_commonGraphicsMeshShaderPipelineLayout)
				vkDestroyPipelineLayout(m_device->GetHandle(), pipeline.second.m_layout, nullptr);
			vkDestroyPipeline(m_device->GetHandle(), pipeline.second.m_pipeline, nullptr);
		}

		for (auto& pipeline : m_computePipelineCache)
		{
			if (pipeline.second.m_layout != m_commonComputePipelineLayout)
				vkDestroyPipelineLayout(m_device->GetHandle(), pipeline.second.m_layout, nullptr);
			vkDestroyPipeline(m_device->GetHandle(), pipeline.second.m_pipeline, nullptr);
		}

		for (auto& pipeline : m_raytracingPipelineCache)
		{
			if (pipeline.second.m_layout != m_commonRaytracingPipelineLayout)
				vkDestroyPipelineLayout(m_device->GetHandle(), pipeline.second.m_layout, nullptr);

			m_renderer->FreeBuffer(pipeline.second.m_raytracingData.m_sbtBuffer);
			vkDestroyPipeline(m_device->GetHandle(), pipeline.second.m_pipeline, nullptr);
		}

		vkDestroyPipelineLayout(m_device->GetHandle(), m_commonGraphicsPipelineLayout, nullptr);
		m_commonGraphicsPipelineLayout = VK_NULL_HANDLE;
		vkDestroyPipelineLayout(m_device->GetHandle(), m_commonGraphicsMeshShaderPipelineLayout, nullptr);
		m_commonGraphicsMeshShaderPipelineLayout = VK_NULL_HANDLE;
		vkDestroyPipelineLayout(m_device->GetHandle(), m_commonComputePipelineLayout, nullptr);
		m_commonComputePipelineLayout = VK_NULL_HANDLE;
		vkDestroyPipelineLayout(m_device->GetHandle(), m_commonRaytracingPipelineLayout, nullptr);
		m_commonRaytracingPipelineLayout = VK_NULL_HANDLE;

		m_masterSetLayout = VK_NULL_HANDLE;
		m_uboSetLayout = VK_NULL_HANDLE;
		m_asSetLayout = VK_NULL_HANDLE;
	}

	Pipeline PipelineCache::GetPipeline(const GraphicsPipelineDesc& desc)
	{
		auto it = m_graphicsPipelineCache.find(desc);
		if (it == m_graphicsPipelineCache.end())
		{
			auto newPipeline = CreateGraphicsPipeline(desc);
			return reinterpret_cast<Pipeline>(&(m_graphicsPipelineCache[desc] = newPipeline));
		}
		else
			return reinterpret_cast<Pipeline>(&it->second);
	}

	Pipeline PipelineCache::GetPipeline(const ComputePipelineDesc& desc)
	{
		auto it = m_computePipelineCache.find(desc);
		if (it == m_computePipelineCache.end())
		{
			auto newPipeline = CreateComputePipeline(desc);
			return reinterpret_cast<Pipeline>(&(m_computePipelineCache[desc] = newPipeline));
		}
		else
			return reinterpret_cast<Pipeline>(&it->second);
	}

	Pipeline PipelineCache::GetPipeline(const RayTracingPipelineDesc& desc)
	{
		auto it = m_raytracingPipelineCache.find(desc);
		if (it == m_raytracingPipelineCache.end())
		{
			auto newPipeline = CreateRayTracingPipeline(desc);
			return reinterpret_cast<Pipeline>(&(m_raytracingPipelineCache[desc] = newPipeline));
		}
		else
			return reinterpret_cast<Pipeline>(&it->second);
	}

	inline void PipelineCache::CreateCommonPipelineLayouts()
	{
		VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr };
		layoutInfo.flags = 0;

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = 128; // Minimum supported by vulkan spec.
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstant;

		VkDescriptorSetLayout setLayouts[] = { m_masterSetLayout, m_uboSetLayout, m_asSetLayout };
		layoutInfo.setLayoutCount = 2;
		layoutInfo.pSetLayouts = setLayouts;

		// Graphics
		{
			layoutInfo.setLayoutCount = 2;

			PB_ERROR_CHECK(vkCreatePipelineLayout(m_device->GetHandle(), &layoutInfo, nullptr, &m_commonGraphicsPipelineLayout));
			PB_BREAK_ON_ERROR;
			PB_ASSERT(m_commonGraphicsPipelineLayout);
		}

		// Graphics w/ Mesh Shaders
		{
			layoutInfo.setLayoutCount = 2;

			pushConstant.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
			PB_ERROR_CHECK(vkCreatePipelineLayout(m_device->GetHandle(), &layoutInfo, nullptr, &m_commonGraphicsMeshShaderPipelineLayout));
			PB_BREAK_ON_ERROR;
			PB_ASSERT(m_commonGraphicsMeshShaderPipelineLayout);
		}

		// Compute
		{
			layoutInfo.setLayoutCount = 2;

			pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			PB_ERROR_CHECK(vkCreatePipelineLayout(m_device->GetHandle(), &layoutInfo, nullptr, &m_commonComputePipelineLayout));
			PB_BREAK_ON_ERROR;
			PB_ASSERT(m_commonComputePipelineLayout);
		}

		// Raytracing
		if(m_device->GetDeviceLimitations()->m_supportRaytracing)
		{
			layoutInfo.setLayoutCount = 3;

			pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			PB_ERROR_CHECK(vkCreatePipelineLayout(m_device->GetHandle(), &layoutInfo, nullptr, &m_commonRaytracingPipelineLayout));
			PB_BREAK_ON_ERROR;
			PB_ASSERT(m_commonRaytracingPipelineLayout);
		}
	}

	PipelineData PipelineCache::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		PB::Rect renderArea = desc.m_renderArea;
		if (renderArea.w * renderArea.h == 0)
		{
			renderArea.w = 128;
			renderArea.h = 128;
		}

		VkRect2D scissor = { renderArea.x, renderArea.y, renderArea.w, renderArea.h };
		VkViewport viewPort;
		viewPort.minDepth = 0.0f;
		viewPort.maxDepth = 1.0f;
		viewPort.x = static_cast<float>(renderArea.x);
		viewPort.y = static_cast<float>(renderArea.y);
		viewPort.width = static_cast<float>(renderArea.w);
		viewPort.height = static_cast<float>(renderArea.h);

		VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr };
		viewportState.flags = 0;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewPort;

		CLib::Vector<VkVertexInputBindingDescription, GraphicsPipelineDesc::MaxVertexBuffers> bindingDescs;
		{
			u32 bindingIndex = 0;
			for (auto& bufferDesc : desc.m_vertexBuffers)
			{
				if (bufferDesc.m_type == EVertexBufferType::NONE)
					break;

				PB_ASSERT(bufferDesc.m_vertexSize > 0);

				VkVertexInputBindingDescription& vertexBindingDesc = bindingDescs.PushBack();
				vertexBindingDesc.binding = bindingIndex++;
				vertexBindingDesc.stride = bufferDesc.m_vertexSize;

				switch (bufferDesc.m_type)
				{
				case EVertexBufferType::VERTEX:
					vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
					break;
				case EVertexBufferType::INSTANCE:
					vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
					break;
				default:
					vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
					break;
				}
			}
		}

		CLib::Vector<VkVertexInputAttributeDescription, VertexAttributeDesc::MaxVertexAttributes> attrDescriptions;
		{
			u32 vertexAttributeLocation = 0;
			u32 totalVertexAttrOffset[VertexAttributeDesc::MaxVertexAttributes]{};
			for (auto& attr : desc.m_vertexDesc.vertexAttributes)
			{
				if (attr.m_attribute == EVertexAttributeType::NONE)
					break;

				u32 locationCount = attr.m_attribute == EVertexAttributeType::MAT4 ? 4 : 1;

				for (u32 i = 0; i < locationCount; ++i)
				{
					VkVertexInputAttributeDescription& vertexAttrDesc = attrDescriptions.PushBack();
					vertexAttrDesc.binding = attr.m_buffer;
					vertexAttrDesc.format = PBVertexTypeToVkFormat(attr.m_attribute);
					vertexAttrDesc.location = vertexAttributeLocation++;
					vertexAttrDesc.offset = totalVertexAttrOffset[attr.m_buffer];

					if (locationCount > 1) // Matrices use multiple float4 attributes.
						totalVertexAttrOffset[attr.m_buffer] += sizeof(float) * 4;
					else
						totalVertexAttrOffset[attr.m_buffer] += static_cast<u32>(attr.m_attribute);
				}
			}
		}

		VkPipelineVertexInputStateCreateInfo vertexInputState{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
		vertexInputState.flags = 0;
		vertexInputState.vertexAttributeDescriptionCount = attrDescriptions.Count();
		vertexInputState.pVertexAttributeDescriptions = attrDescriptions.Data();
		vertexInputState.vertexBindingDescriptionCount = bindingDescs.Count();
		vertexInputState.pVertexBindingDescriptions = bindingDescs.Data();

		VkPrimitiveTopology topology;
		switch (desc.m_topology)
		{
		case EPrimitiveTopologyType::TRIANGLE_LIST:
			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		case EPrimitiveTopologyType::TRIANGLE_STRIP:
			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			break;
		case EPrimitiveTopologyType::LINE_LIST:
			topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			break;
		case EPrimitiveTopologyType::LINE_STRIP:
			topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			break;
		case EPrimitiveTopologyType::POINT_LIST:
			topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			break;
		default:
			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		}

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr };
		inputAssemblyState.flags = 0;
		inputAssemblyState.primitiveRestartEnable = VK_FALSE;
		inputAssemblyState.topology = topology;

		VkPipelineMultisampleStateCreateInfo multisampleState{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr };
		multisampleState.flags = 0;
		multisampleState.alphaToCoverageEnable = VK_FALSE;
		multisampleState.alphaToOneEnable = VK_FALSE;
		multisampleState.minSampleShading = 1.0f;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.sampleShadingEnable = VK_FALSE;
		multisampleState.pSampleMask = nullptr;

		VkPipelineRasterizationStateCreateInfo rasterState{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr };
		rasterState.flags = 0;
		rasterState.depthBiasEnable = VK_FALSE;
		rasterState.depthClampEnable = VK_FALSE;
		rasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterState.lineWidth = desc.m_lineThickness;
		rasterState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterState.rasterizerDiscardEnable = VK_FALSE;

		switch (desc.m_cullMode)
		{
		case EFaceCullMode::NONE:
			rasterState.cullMode = VK_CULL_MODE_NONE;
			break;
		case EFaceCullMode::BACK:
			rasterState.cullMode = VK_CULL_MODE_BACK_BIT;
			break;
		case EFaceCullMode::FRONT:
			rasterState.cullMode = VK_CULL_MODE_FRONT_BIT;
			break;
		default:
			rasterState.cullMode = VK_CULL_MODE_BACK_BIT;
			break;
		}

		VkPipelineDepthStencilStateCreateInfo depthStencilState{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
		depthStencilState.maxDepthBounds = 1.0f;
		depthStencilState.minDepthBounds = 0.0f;
		depthStencilState.depthCompareOp = PBCompareOPtoVKCompareOP(desc.m_depthCompareOP);
		depthStencilState.depthTestEnable = desc.m_depthCompareOP != ECompareOP::ALWAYS;
		depthStencilState.depthBoundsTestEnable = depthStencilState.depthTestEnable;
		depthStencilState.depthWriteEnable = desc.m_depthWriteEnable;
		depthStencilState.stencilTestEnable = desc.m_stencilTestEnable;
		depthStencilState.flags = 0;

		PB_ASSERT(desc.m_attachmentCount > 0 || !desc.m_shaderModules[static_cast<u32>(EGraphicsShaderStage::FRAGMENT)]);
		CLib::Vector<VkPipelineColorBlendAttachmentState, 8> colorBlendAttachmentStates;
		for (u32 i = 0; i < desc.m_attachmentCount; ++i)
		{
			const AttachmentBlendState& blendState = desc.m_colorBlendStates[i];
			VkPipelineColorBlendAttachmentState& attachmentState = colorBlendAttachmentStates.PushBack();
			attachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			attachmentState.blendEnable = static_cast<VkBool32>(blendState.m_enableBlending);
			attachmentState.srcColorBlendFactor = static_cast<VkBlendFactor>(blendState.m_srcColor);
			attachmentState.dstColorBlendFactor = static_cast<VkBlendFactor>(blendState.m_dstColor);
			attachmentState.colorBlendOp = static_cast<VkBlendOp>(blendState.m_srcBlend);
			attachmentState.srcAlphaBlendFactor = static_cast<VkBlendFactor>(blendState.m_srcAlpha);
			attachmentState.dstAlphaBlendFactor = static_cast<VkBlendFactor>(blendState.m_dstAlpha);
			attachmentState.alphaBlendOp = static_cast<VkBlendOp>(blendState.m_dstBlend);
		}

		VkPipelineColorBlendStateCreateInfo colorBlendState{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr };
		colorBlendState.flags = 0;
		colorBlendState.attachmentCount = colorBlendAttachmentStates.Count();
		colorBlendState.pAttachments = colorBlendAttachmentStates.Data();
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

		VkPipelineDynamicStateCreateInfo dynamicStatesInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr };
		dynamicStatesInfo.flags = 0;
		dynamicStatesInfo.dynamicStateCount = _countof(dynamicStates);
		dynamicStatesInfo.pDynamicStates = dynamicStates;

		CLib::Vector<VkPipelineShaderStageCreateInfo, static_cast<u32>(EGraphicsShaderStage::GRAPHICS_STAGE_COUNT)> shaderStages;
		bool hasVertexShader = true;
		bool hasMeshShader = false;
		for (u32 i = 0; i < static_cast<u32>(EGraphicsShaderStage::GRAPHICS_STAGE_COUNT); ++i)
		{
			if (!desc.m_shaderModules[i])
			{
				if (i == u32(EGraphicsShaderStage::VERTEX))
					hasVertexShader = false;

				continue;
			}
			else if (i == u32(EGraphicsShaderStage::MESH))
			{
				hasMeshShader = true;
			}

			PB_ASSERT_MSG(i != u32(EGraphicsShaderStage::MESH) && i != u32(EGraphicsShaderStage::TASK)
				|| (hasVertexShader == false || desc.m_shaderModules[i] == 0)
				, "Graphics pipeline cannot have a both vertex and mesh/task shader stages. They are mutually exclusive.");

			VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
			stage.flags = 0;
			stage.module = reinterpret_cast<VkShaderModule>(desc.m_shaderModules[i]);
			stage.pName = "main";
			stage.pSpecializationInfo = nullptr;
			stage.stage = ConvertPBShaderStageToVK(static_cast<EGraphicsShaderStage>(i));
			shaderStages.PushBack(stage);
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr };
		pipelineInfo.flags = 0;
		pipelineInfo.renderPass = reinterpret_cast<VkRenderPass>(desc.m_renderPass);
		pipelineInfo.subpass = static_cast<u32>(desc.m_subpass);
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = 0;
		pipelineInfo.layout = hasMeshShader ? m_commonGraphicsMeshShaderPipelineLayout : m_commonGraphicsPipelineLayout;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pDynamicState = (desc.m_renderArea.w * desc.m_renderArea.h > 0) ? nullptr : &dynamicStatesInfo;
		pipelineInfo.pVertexInputState = hasMeshShader == false ? &vertexInputState : nullptr;
		pipelineInfo.pInputAssemblyState = hasMeshShader == false ? &inputAssemblyState : nullptr;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pMultisampleState = &multisampleState;
		pipelineInfo.pRasterizationState = &rasterState;
		pipelineInfo.pDepthStencilState = &depthStencilState;
		pipelineInfo.pColorBlendState = desc.m_attachmentCount > 0 ? &colorBlendState : nullptr;

		pipelineInfo.pStages = shaderStages.Data();
		pipelineInfo.stageCount = shaderStages.Count();

		VkPipelineRenderingCreateInfoKHR dynamicRenderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR, nullptr };
		if (desc.m_renderPass && desc.m_attachmentCount > 0 && m_device->GetDynamicRenderingFeatures()->dynamicRendering == VK_TRUE)
		{
			pipelineInfo.renderPass = nullptr;
			pipelineInfo.pNext = &dynamicRenderingInfo;

			RenderPassCache::DynamicRenderPass* pass = reinterpret_cast<RenderPassCache::DynamicRenderPass*>(desc.m_renderPass);

			dynamicRenderingInfo.viewMask = 0;
			dynamicRenderingInfo.colorAttachmentCount = pass->m_colorAttachmentFormats.Count();
			dynamicRenderingInfo.pColorAttachmentFormats = pass->m_colorAttachmentFormats.Data();
			dynamicRenderingInfo.depthAttachmentFormat = pass->m_inheritanceInfo.depthAttachmentFormat;
			dynamicRenderingInfo.stencilAttachmentFormat = pass->m_inheritanceInfo.stencilAttachmentFormat;
		}

		VkPipeline newPipeline = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateGraphicsPipelines(m_device->GetHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newPipeline);
		return { pipelineInfo.layout, newPipeline, {}, false, hasMeshShader };
	}

	PipelineData PipelineCache::CreateComputePipeline(const ComputePipelineDesc& desc)
	{
		VkComputePipelineCreateInfo computePipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr };
		computePipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		computePipelineInfo.basePipelineIndex = 0;
		computePipelineInfo.flags = 0;
		computePipelineInfo.layout = m_commonComputePipelineLayout;

		VkPipelineShaderStageCreateInfo& computeStage = computePipelineInfo.stage;
		computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		computeStage.pNext = nullptr;
		computeStage.flags = 0;
		computeStage.module = reinterpret_cast<VkShaderModule>(desc.m_computeModule);
		computeStage.pName = "main";
		computeStage.pSpecializationInfo = nullptr;
		computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

		VkPipeline newPipeline = VK_NULL_HANDLE;
		vkCreateComputePipelines(m_device->GetHandle(), VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &newPipeline);
		PB_BREAK_ON_ERROR;
		PB_ASSERT(newPipeline);
		return { computePipelineInfo.layout, newPipeline, {}, true, false, false };
	}

	PipelineData PipelineCache::CreateRayTracingPipeline(const RayTracingPipelineDesc& desc)
	{
		CLib::Vector<VkPipelineShaderStageCreateInfo, static_cast<u32>(ERayTracingShaderStage::RAYTRACING_STAGE_COUNT)> shaderStages;
		CLib::Vector<VkRayTracingShaderGroupCreateInfoKHR, (RayTracingPipelineDesc::MaxShadersPerStage * 2) + 1> shaderGroups;

		// Ray gen module
		{
			PB_ASSERT_MSG(desc.rayGenShaderModule != 0, "Raygen module is required but missing.");

			VkRayTracingShaderGroupCreateInfoKHR& genShaderGroupInfo = shaderGroups.PushBack();
			genShaderGroupInfo = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr };
			genShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			genShaderGroupInfo.generalShader = shaderStages.Count();
			genShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
			genShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
			genShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

			VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
			stage.flags = 0;
			stage.module = reinterpret_cast<VkShaderModule>(desc.rayGenShaderModule);
			stage.pName = "main";
			stage.pSpecializationInfo = nullptr;
			stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			shaderStages.PushBack(stage);
		}

		// Miss modules
		u32 missModuleCount = 0;
		for (u32 i = 0; i < RayTracingPipelineDesc::MaxShadersPerStage; ++i)
		{
			if (!desc.missShaderModules[i])
			{
				continue;
			}

			VkRayTracingShaderGroupCreateInfoKHR& missShaderGroupInfo = shaderGroups.PushBack();
			missShaderGroupInfo = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr };
			missShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			missShaderGroupInfo.generalShader = shaderStages.Count();
			missShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
			missShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
			missShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

			VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
			stage.flags = 0;
			stage.module = reinterpret_cast<VkShaderModule>(desc.missShaderModules[i]);
			stage.pName = "main";
			stage.pSpecializationInfo = nullptr;
			stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
			shaderStages.PushBack(stage);

			++missModuleCount;
		}

		// Closest Hit modules
		u32 closestHitModuleCount = 0;
		for (u32 i = 0; i < RayTracingPipelineDesc::MaxShadersPerStage; ++i)
		{
			if (!desc.closestHitShaderModules[i])
			{
				continue;
			}

			VkRayTracingShaderGroupCreateInfoKHR& hitShaderGroupInfo = shaderGroups.PushBack();
			hitShaderGroupInfo = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr };
			hitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			hitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
			hitShaderGroupInfo.closestHitShader = shaderStages.Count();
			hitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
			hitShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

			VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr };
			stage.flags = 0;
			stage.module = reinterpret_cast<VkShaderModule>(desc.closestHitShaderModules[i]);
			stage.pName = "main";
			stage.pSpecializationInfo = nullptr;
			stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			shaderStages.PushBack(stage);

			++closestHitModuleCount;
		}

		PB_ASSERT(shaderGroups.Count() == shaderStages.Count());

		VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR, nullptr };
		rtPipelineInfo.flags = VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR | VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR;
		rtPipelineInfo.stageCount = shaderStages.Count();
		rtPipelineInfo.pStages = shaderStages.Data();
		rtPipelineInfo.groupCount = shaderGroups.Count();
		rtPipelineInfo.pGroups = shaderGroups.Data();
		rtPipelineInfo.maxPipelineRayRecursionDepth = desc.maxPipelineRecursionCount;
		rtPipelineInfo.pLibraryInfo = nullptr;
		rtPipelineInfo.pLibraryInterface = nullptr;
		rtPipelineInfo.pDynamicState = nullptr;
		rtPipelineInfo.layout = m_commonRaytracingPipelineLayout;
		rtPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		rtPipelineInfo.basePipelineIndex = 0;

		VkPipeline rtPipeline = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateRayTracingPipelinesKHRFunc(m_device->GetHandle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rtPipelineInfo, nullptr, &rtPipeline));
		PB_BREAK_ON_ERROR;
		PB_ASSERT(rtPipeline != VK_NULL_HANDLE);

		// Shader binding table setup
		RayTracingSBTData sbtData{};
		{
			constexpr u32 rayGenHandleCount = 1;
			u32 missHandleCount = missModuleCount;
			u32 closestHitHandleCount = closestHitModuleCount;

			u32 handleCount = rayGenHandleCount + missHandleCount + closestHitHandleCount;
			u32 groupBaseAlign = m_device->GetRaytracingPipelineProperties().shaderGroupBaseAlignment;
			u32 handleSize = std::max<u32>(m_device->GetRaytracingPipelineProperties().shaderGroupHandleSize, m_device->GetRaytracingPipelineProperties().shaderGroupHandleAlignment);

			PB::BufferObjectDesc sbtBufferDesc{};
			sbtBufferDesc.m_bufferSize = (groupBaseAlign * 4) + (handleSize * handleCount);
			sbtBufferDesc.m_options = 0;
			sbtBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::SHADER_BINDING_TABLE;
			sbtData.m_sbtBuffer = reinterpret_cast<BufferObject*>(m_renderer->AllocateBuffer(sbtBufferDesc));

			auto alignUp = [](u64 val, u64 align) -> u64
			{
				u64 mod = val % align;
				u64 pad = align - mod;
				return val + (mod ? pad : 0);
			};

			VkDeviceAddress sbtBufferAddress = sbtData.m_sbtBuffer->GetDeviceAddress();
			VkDeviceAddress sbtAlignedBufferAddress = alignUp(sbtBufferAddress, groupBaseAlign);
			u64 groupAlignOffset = sbtAlignedBufferAddress - sbtBufferAddress;

			auto& rayGenRegion = sbtData.m_rgenRegion;
			auto& missRegion = sbtData.m_missRegion;
			auto& closestHitRegion = sbtData.m_hitRegion;
			auto& callableRegion = sbtData.m_callRegion; // Currently unused.

			// Calc regions
			{
				rayGenRegion.deviceAddress = sbtAlignedBufferAddress;
				rayGenRegion.stride = alignUp(handleSize, groupBaseAlign);
				rayGenRegion.size = rayGenRegion.stride;

				missRegion.deviceAddress = rayGenRegion.deviceAddress + rayGenRegion.stride;
				//missRegion.stride = alignUp(handleSize * missHandleCount, groupBaseAlign);
				//missRegion.size = missRegion.stride;
				missRegion.size = alignUp(handleSize * missHandleCount, groupBaseAlign);
				missRegion.stride = handleSize;

				closestHitRegion.deviceAddress = missRegion.deviceAddress + missRegion.size;
				closestHitRegion.size = alignUp(handleSize * closestHitHandleCount, groupBaseAlign);
				closestHitRegion.stride = handleSize;

				callableRegion.deviceAddress = 0;
				callableRegion.size = 0;
				callableRegion.stride = callableRegion.size;
			}

			// Retrieve shader handles
			std::vector<u8> handlesData;
			{
				handlesData.resize(handleSize * handleCount);

				PB_ERROR_CHECK(vkGetRayTracingShaderGroupHandlesKHRFunc(m_device->GetHandle(), rtPipeline, 0, handleCount, handlesData.size(), handlesData.data()));
				PB_BREAK_ON_ERROR;
			}

			// Populate buffer with handles placed in the correct regions
			{
				u8* handles = sbtData.m_sbtBuffer->BeginPopulate() + groupAlignOffset;
				std::memset(handles, 0, sbtData.m_sbtBuffer->GetSize());

				u32 writeHandleOffset = 0;
				std::memcpy(handles, handlesData.data() + writeHandleOffset, handleSize);

				writeHandleOffset += handleSize;
				handles += rayGenRegion.size;
				std::memcpy(handles, handlesData.data() + writeHandleOffset, missHandleCount * handleSize);

				writeHandleOffset += missHandleCount * handleSize;
				handles += missRegion.size;
				std::memcpy(handles, handlesData.data() + writeHandleOffset, closestHitHandleCount * handleSize);

				sbtData.m_sbtBuffer->EndPopulate();
			}
		}

		return { rtPipelineInfo.layout, rtPipeline, sbtData, false, false, true };
	}
};
