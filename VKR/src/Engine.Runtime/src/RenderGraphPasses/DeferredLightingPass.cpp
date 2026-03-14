#include "DeferredLightingPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Texture.h"

namespace Eng
{
	DeferredLightingPass::DeferredLightingPass(PB::IRenderer* renderer, CLib::Allocator* allocator, bool useRaytracedShadows)
		: RenderGraphBehaviour(renderer, allocator)
		, m_useRaytracedShadows(useRaytracedShadows)
	{
		PB::BufferObjectDesc lightingBufferDesc;
		lightingBufferDesc.m_bufferSize = sizeof(LightingBuffer);
		lightingBufferDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
		lightingBufferDesc.m_usage = PB::EBufferUsage::UNIFORM;
		m_lightingBuffer = renderer->AllocateBuffer(lightingBufferDesc);

		PB::BufferViewDesc lightViewDesc;
		lightViewDesc.m_offset = 0;
		lightViewDesc.m_size = sizeof(LightingBuffer::m_pointLights);
		m_pointLightingView = m_lightingBuffer->GetViewAsUniformBuffer(lightViewDesc);

		lightViewDesc.m_offset = sizeof(LightingBuffer::m_pointLights);
		lightViewDesc.m_size = sizeof(LightingBuffer::m_directionalLights);
		m_dirLightingView = m_lightingBuffer->GetViewAsUniformBuffer(lightViewDesc);

		PB::SamplerDesc gBufferSamplerDesc;
		gBufferSamplerDesc.m_filter = PB::ESamplerFilter::NEAREST;
		gBufferSamplerDesc.m_mipFilter = PB::ESamplerFilter::NEAREST;
		gBufferSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
		m_gBufferSampler = renderer->GetSampler(gBufferSamplerDesc);

		AssetEncoder::AssetHandle pointLightMeshHandle("Meshes/Primitives/sphere");
		m_pointLightVolumeMesh.Init(pointLightMeshHandle.GetID(&Mesh::s_meshDatabaseLoader), &Mesh::s_meshDatabaseLoader);
		m_pointLightVolumeMesh.Load(m_renderer, &Mesh::s_meshDatabaseLoader);

		enum class EShaderStagePermutation
		{
			SHADER_STAGE,
			PERMUTATION_END
		};

		AssetEncoder::ShaderPermutationTable permTable{};
		permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, AssetEncoder::EShaderStagePermutation::VERTEX);

		m_screenQuadShader = m_allocator->Alloc<Shader>(m_renderer, "Shaders/GLSL/def_directional_light_shadow", permTable.GetKey(), m_allocator, true);
		m_pointLightVTXShader = m_allocator->Alloc<Shader>(m_renderer, "Shaders/GLSL/vs_obj_point_light", 0, m_allocator, true);

		permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, AssetEncoder::EShaderStagePermutation::FRAGMENT);
		permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_1, uint8_t(m_useRaytracedShadows));
		m_defDirLightShader = m_allocator->Alloc<Shader>(m_renderer, "Shaders/GLSL/def_directional_light_shadow", permTable.GetKey(), m_allocator, true);
		permTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_1, 0);
		m_pointLightShader = m_allocator->Alloc<Shader>(m_renderer, "Shaders/GLSL/fs_def_point_light", 0, m_allocator, true);

		PB::BufferObjectDesc indirectParamsDesc;
		indirectParamsDesc.m_bufferSize = sizeof(PB::DrawIndexedIndirectParams);
		indirectParamsDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
		indirectParamsDesc.m_usage = PB::EBufferUsage::INDIRECT_PARAMS;
		m_pointLightIndirectParamsBuffer = m_renderer->AllocateBuffer(indirectParamsDesc);
	}

	DeferredLightingPass::~DeferredLightingPass()
	{
		if (m_reusableCmdList)
			m_renderer->FreeCommandList(m_reusableCmdList);

		if (m_specBDRFLut)
		{
			m_renderer->FreeTexture(m_specBDRFLut);
			m_specBDRFLut = nullptr;
		}

		m_renderer->FreeBuffer(m_lightingBuffer);

		m_allocator->Free(m_screenQuadShader);
		m_allocator->Free(m_defDirLightShader);
		m_allocator->Free(m_pointLightVTXShader);
		m_allocator->Free(m_pointLightShader);

		m_renderer->FreeBuffer(m_pointLightIndirectParamsBuffer);
	}

	void DeferredLightingPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (m_specBDRFLut == nullptr)
		{
			GenSpecBDRFLut(info.m_commandContext);
		}

		info.m_commandContext->CmdBeginLabel("DeferredLightingPass", { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void DeferredLightingPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (m_mappedLightingBuffer)
		{
			m_lightingBuffer->EndPopulate();
			m_mappedLightingBuffer = nullptr;

			PB::u8* mappedPtr = m_pointLightIndirectParamsBuffer->BeginPopulate();
			PB::DrawIndexedIndirectParams indirectParams{};
			indirectParams.m_offset = 0;
			indirectParams.m_firstIndex = 0;
			indirectParams.m_indexCount = m_pointLightVolumeMesh.IndexCount();
			indirectParams.m_instanceCount = m_pointLightCount;
			indirectParams.m_vertexOffset = 0;
			m_pointLightIndirectParamsBuffer->PopulateWithDrawIndexedIndirectParams(mappedPtr, indirectParams);
			m_pointLightIndirectParamsBuffer->EndPopulate();
		}

		auto RecordPass = [&]()
		{
			PB::CommandContextDesc scopedContextDesc{};
			scopedContextDesc.m_renderer = m_renderer;
			scopedContextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;
			scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

			PB::SCommandContext scopedContext(m_renderer);
			scopedContext->Init(scopedContextDesc);
			scopedContext->Begin(info.m_renderPass, info.m_frameBuffer);

			auto renderWidth = m_targetResolution.x;
			auto renderHeight = m_targetResolution.y;

			PB::TextureViewDesc skyboxViewDesc{};
			skyboxViewDesc.m_texture = m_skyboxTexture->GetTexture();
			skyboxViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
			skyboxViewDesc.m_format = m_skyboxTexture->IsCompressed() ? PB::ETextureFormat::BC6H_RGB_U16F : PB::ETextureFormat::R16G16B16A16_FLOAT;
			skyboxViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_CUBE;
			skyboxViewDesc.m_subresources.m_arrayCount = 1;
			skyboxViewDesc.m_subresources.m_mipCount = 1;

			PB::ResourceView skyboxView = m_skyboxTexture->GetTexture()->GetView(skyboxViewDesc);

			PB::SamplerDesc iblSamplerDesc;
			iblSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
			iblSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
			iblSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
			iblSamplerDesc.maxLod = 1.0f;
			PB::ResourceView iblSampler = m_renderer->GetSampler(iblSamplerDesc);

			skyboxViewDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
			skyboxViewDesc.m_texture = m_irradianceMap->GetTexture();
			PB::ResourceView irradianceView = m_irradianceMap->GetTexture()->GetView(skyboxViewDesc);

			skyboxViewDesc.m_format = m_prefilterEnvMap->IsCompressed() ? PB::ETextureFormat::BC6H_RGB_U16F : PB::ETextureFormat::R16G16B16A16_FLOAT;
			skyboxViewDesc.m_texture = m_prefilterEnvMap->GetTexture();
			skyboxViewDesc.m_subresources.m_mipCount = m_prefilterEnvMap->GetMipCount();
			PB::ResourceView prefilterView = m_prefilterEnvMap->GetTexture()->GetView(skyboxViewDesc);

			// Sky
			if (m_skyboxTexture != nullptr)
			{
				PB::GraphicsPipelineDesc skyboxPipelineDesc{};
				skyboxPipelineDesc.m_attachmentCount = 1;
				skyboxPipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL; // This ensures the skybox will only rasterize where depth is max.
				skyboxPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
				skyboxPipelineDesc.m_stencilTestEnable = false;
				skyboxPipelineDesc.m_depthWriteEnable = false;
				skyboxPipelineDesc.m_cullMode = PB::EFaceCullMode::BACK;
				skyboxPipelineDesc.m_subpass = 0;
				skyboxPipelineDesc.m_renderPass = info.m_renderPass;
				skyboxPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_skybox", 0, m_allocator, true).GetModule();
				skyboxPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_skybox", 0, m_allocator, true).GetModule();
				skyboxPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;

				PB::Pipeline skyboxPipeline = m_renderer->GetPipelineCache()->GetPipeline(skyboxPipelineDesc);

				PB::UniformBufferView skyBufferViews[] = { m_mvpBuffer->GetViewAsUniformBuffer() };
				PB::ResourceView skyResourceViews[]
				{
					skyboxView,
					iblSampler
				};

				PB::BindingLayout skyBindingLayout{};
				skyBindingLayout.m_uniformBufferCount = PB_ARRAY_LENGTH(skyBufferViews);
				skyBindingLayout.m_uniformBuffers = skyBufferViews;
				skyBindingLayout.m_resourceCount = PB_ARRAY_LENGTH(skyResourceViews);
				skyBindingLayout.m_resourceViews = skyResourceViews;

				scopedContext->CmdBindPipeline(skyboxPipeline);
				scopedContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
				scopedContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });
				scopedContext->CmdBindResources(skyBindingLayout);
				scopedContext->CmdDraw(36, 1);
			}

			// Directional lighting

			PB::UniformBufferView dirBufferViews[] = { m_mvpBuffer->GetViewAsUniformBuffer(), m_dirLightingView };
			PB::ResourceView dirResourceViews[]
			{
				transientTextures[m_colorTextureIndex]->GetDefaultSRV(),
				transientTextures[m_normalTextureIndex]->GetDefaultSRV(),
				transientTextures[m_specAndRoughTextureIndex]->GetDefaultSRV(),
				transientTextures[m_depthTextureIndex]->GetDefaultSRV(),
				transientTextures[m_shadowMaskTextureIndex]->GetDefaultSRV(),
				transientTextures[m_aoTextureIndex]->GetDefaultSRV(),
				irradianceView,
				m_useRaytracedShadows ? transientTextures[m_reflectionTextureIndex]->GetDefaultSRV() : prefilterView,
				m_specBDRFLut->GetDefaultSRV(),
				m_gBufferSampler,
				iblSampler,
			};

			PB::BindingLayout dirBindingLayout{};
			dirBindingLayout.m_uniformBufferCount = PB_ARRAY_LENGTH(dirBufferViews);
			dirBindingLayout.m_uniformBuffers = dirBufferViews;
			dirBindingLayout.m_resourceCount = PB_ARRAY_LENGTH(dirResourceViews);
			dirBindingLayout.m_resourceViews = dirResourceViews;

			if (m_dirLightingPipeline == 0)
			{
				PB::GraphicsPipelineDesc lightingPipelineDesc{};
				lightingPipelineDesc.m_attachmentCount = 1;
				lightingPipelineDesc.m_depthCompareOP = PB::ECompareOP::GREATER; // The screen quad will be positioned at max depth, so directional lighting will only rasterize where world geo is rendered (as it lowers depth).
				lightingPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
				lightingPipelineDesc.m_stencilTestEnable = false;
				lightingPipelineDesc.m_depthWriteEnable = false;
				lightingPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
				lightingPipelineDesc.m_subpass = 0;
				lightingPipelineDesc.m_renderPass = info.m_renderPass;
				lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_screenQuadShader->GetModule();
				lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_defDirLightShader->GetModule();
				lightingPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;

				m_dirLightingPipeline = m_renderer->GetPipelineCache()->GetPipeline(lightingPipelineDesc);
			}

			{
				scopedContext->CmdBindPipeline(m_dirLightingPipeline);
				scopedContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
				scopedContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });
				scopedContext->CmdBindResources(dirBindingLayout);
				scopedContext->CmdDraw(6, 1);
			}

			if (m_pointLightingPipeline == 0)
			{
				PB::GraphicsPipelineDesc lightingPipelineDesc{};
				lightingPipelineDesc.m_attachmentCount = 1;
				lightingPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS; // Always should disable depth testing.
				lightingPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
				lightingPipelineDesc.m_stencilTestEnable = false;
				lightingPipelineDesc.m_depthWriteEnable = false;
				lightingPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
				lightingPipelineDesc.m_subpass = 0;
				lightingPipelineDesc.m_renderPass = info.m_renderPass;
				lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_pointLightVTXShader->GetModule();
				lightingPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_pointLightShader->GetModule();
				lightingPipelineDesc.m_vertexBuffers[0] = { sizeof(AssetPipeline::Vertex), PB::EVertexBufferType::VERTEX };
				lightingPipelineDesc.m_vertexDesc.vertexAttributes[0] = { 0, PB::EVertexAttributeType::FLOAT3 };
				lightingPipelineDesc.m_colorBlendStates[0] = PB::GraphicsPipelineDesc::DefaultBlendState();

				m_pointLightingPipeline = m_renderer->GetPipelineCache()->GetPipeline(lightingPipelineDesc);
			}

			// Point lighting
			if (m_pointLightCount > 0)
			{
				PB::UniformBufferView pntBufferViews[] = { m_mvpBuffer->GetViewAsUniformBuffer(), m_pointLightingView };
				PB::ResourceView pntResourceViews[]
				{
					transientTextures[0]->GetDefaultSRV(),
					transientTextures[1]->GetDefaultSRV(),
					transientTextures[2]->GetDefaultSRV(),
					transientTextures[3]->GetDefaultSRV(),
					m_gBufferSampler,
				};

				PB::BindingLayout pntBindingLayout{};
				pntBindingLayout.m_uniformBufferCount = PB_ARRAY_LENGTH(pntBufferViews);
				pntBindingLayout.m_uniformBuffers = pntBufferViews;
				pntBindingLayout.m_resourceCount = PB_ARRAY_LENGTH(pntResourceViews);
				pntBindingLayout.m_resourceViews = pntResourceViews;

				scopedContext->CmdBindPipeline(m_pointLightingPipeline);
				scopedContext->CmdBindResources(pntBindingLayout);
				scopedContext->CmdBindVertexBuffer(m_pointLightVolumeMesh.GetVertexBuffer(), m_pointLightVolumeMesh.GetIndexBuffer(), PB::EIndexType::PB_INDEX_TYPE_UINT32);
				scopedContext->CmdDrawIndexedIndirect(m_pointLightIndirectParamsBuffer, 0);
			}

			scopedContext->End();
			return scopedContext->Return();
		};
		if (!m_reusableCmdList)
			m_reusableCmdList = RecordPass();

		info.m_commandContext->CmdExecuteList(m_reusableCmdList);
	}

	void DeferredLightingPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdEndLastLabel();
	}

	void DeferredLightingPass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		if (m_reusableCmdList)
		{
			m_renderer->FreeCommandList(m_reusableCmdList);
			m_reusableCmdList = nullptr;
		}

		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = true;

		// Read G Buffers
		m_colorTextureIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& colorReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		colorReadDesc.m_format = PB::ETextureFormat::A2R10G10B10_UNORM;
		colorReadDesc.m_width = targetResolution.x;
		colorReadDesc.m_height = targetResolution.y;
		colorReadDesc.m_name = "G_Color";
		colorReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		colorReadDesc.m_usageFlags = PB::ETextureState::SAMPLED;

		m_normalTextureIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& normalDesc = nodeDesc.m_transientTextures.PushBackInit();
		normalDesc.m_format = PB::ETextureFormat::A2R10G10B10_UNORM;
		normalDesc.m_width = targetResolution.x;
		normalDesc.m_height = targetResolution.y;
		normalDesc.m_name = "G_Normal";
		normalDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		normalDesc.m_usageFlags = PB::ETextureState::SAMPLED;

		m_specAndRoughTextureIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& specAndRoughDesc = nodeDesc.m_transientTextures.PushBackInit();
		specAndRoughDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		specAndRoughDesc.m_width = targetResolution.x;
		specAndRoughDesc.m_height = targetResolution.y;
		specAndRoughDesc.m_name = "G_SpecAndRough";
		specAndRoughDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		specAndRoughDesc.m_usageFlags = PB::ETextureState::SAMPLED;

		m_depthTextureIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& depthReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		depthReadDesc.m_format = PB::ETextureFormat::D32_FLOAT;
		depthReadDesc.m_width = targetResolution.x;
		depthReadDesc.m_height = targetResolution.y;
		depthReadDesc.m_name = "G_Depth";
		depthReadDesc.m_initialUsage = PB::ETextureState::READ_ONLY_DEPTH_STENCIL;
		depthReadDesc.m_usageFlags = PB::ETextureState::READ_ONLY_DEPTH_STENCIL | PB::ETextureState::SAMPLED;

		m_shadowMaskTextureIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& shadowMaskReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		shadowMaskReadDesc.m_format = PB::ETextureFormat::R8_UNORM;
		shadowMaskReadDesc.m_name = "ShadowMask";
		shadowMaskReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		shadowMaskReadDesc.m_usageFlags = PB::ETextureState::SAMPLED;

		m_aoTextureIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& ambientOcclusionReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		ambientOcclusionReadDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		ambientOcclusionReadDesc.m_width = 0;	// Leaving dimensions as zero should inherit the dimensions from the AO pass.
		ambientOcclusionReadDesc.m_height = 0;
		ambientOcclusionReadDesc.m_name = "AO_Output";
		ambientOcclusionReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		ambientOcclusionReadDesc.m_usageFlags = PB::ETextureState::SAMPLED;

		if (m_useRaytracedShadows)
		{
			m_reflectionTextureIndex = nodeDesc.m_transientTextures.Count();
			TransientTextureDesc& rtReflectionReadDesc = nodeDesc.m_transientTextures.PushBackInit();
			rtReflectionReadDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
			rtReflectionReadDesc.m_width = 0;	// Leaving dimensions as zero should inherit the dimensions from the RT reflection pass.
			rtReflectionReadDesc.m_height = 0;
			rtReflectionReadDesc.m_name = "RT_Reflections";
			rtReflectionReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
			rtReflectionReadDesc.m_usageFlags = PB::ETextureState::SAMPLED;
		}

		// Output
		AttachmentDesc& colorDesc = nodeDesc.m_attachments.PushBackInit();
		colorDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		colorDesc.m_width = targetResolution.x;
		colorDesc.m_height = targetResolution.y;
		colorDesc.m_name = "LightingColorOutput";
		colorDesc.m_usage = PB::EAttachmentUsage::COLOR;

		AttachmentDesc& depthDesc = nodeDesc.m_attachments.PushBackInit();
		depthDesc.m_format = PB::ETextureFormat::D32_FLOAT;
		depthDesc.m_width = targetResolution.x;
		depthDesc.m_height = targetResolution.y;
		depthDesc.m_name = "G_Depth";
		depthDesc.m_usage = PB::EAttachmentUsage::READ_ONLY_DEPTHSTENCIL;

		nodeDesc.m_renderWidth = colorDesc.m_width;
		nodeDesc.m_renderHeight = colorDesc.m_height;
		m_targetResolution = targetResolution;

		builder->AddNode(nodeDesc);
	}

	void DeferredLightingPass::SetMVPBuffer(PB::IBufferObject* buf)
	{
		m_mvpBuffer = buf;
	}

	void DeferredLightingPass::SetDirectionalLight(uint32_t index, PB::Float4 direction, PB::Float4 color)
	{
		if (!m_mappedLightingBuffer)
			m_mappedLightingBuffer = reinterpret_cast<LightingBuffer*>(m_lightingBuffer->BeginPopulate());

		auto& lights = m_mappedLightingBuffer->m_directionalLights;
		lights.m_lights[index].m_direction = direction;
		lights.m_lights[index].m_color = color;

		if (m_directionalLightCount <= index)
			m_directionalLightCount = index + 1;

		lights.m_lightCount = static_cast<int32_t>(m_directionalLightCount);
		lights.m_emissionIntensityScale = 5.0f;
	}

	void DeferredLightingPass::SetPointLight(uint32_t index, PB::Float4 position, PB::Float3 color, float radius)
	{
		if (!m_mappedLightingBuffer)
			m_mappedLightingBuffer = reinterpret_cast<LightingBuffer*>(m_lightingBuffer->BeginPopulate());

		auto& light = m_mappedLightingBuffer->m_pointLights.m_lights[index];
		light.m_position = position;
		light.m_color = color;
		light.m_radius = radius;

		if (m_pointLightCount <= index)
			m_pointLightCount = index + 1;
	}

	void DeferredLightingPass::SetSkyboxTexture(Eng::Texture* skyboxTexture, Eng::Texture* irradianceMap, Eng::Texture* prefilterMap)
	{
		if (m_reusableCmdList) // Record new command list to use the new skybox.
		{
			m_renderer->FreeCommandList(m_reusableCmdList);
			m_reusableCmdList = nullptr;
		}

		m_skyboxTexture = skyboxTexture;
		m_irradianceMap = irradianceMap;
		m_prefilterEnvMap = prefilterMap;
	}

	void DeferredLightingPass::GenSpecBDRFLut(PB::ICommandContext* cmdContext)
	{
		PB::TextureDesc bdrfLutDesc{};
		bdrfLutDesc.m_dimension = PB::ETextureDimension::DIMENSION_2D;
		bdrfLutDesc.m_format = PB::ETextureFormat::R16G16_FLOAT;
		bdrfLutDesc.m_width = 512;
		bdrfLutDesc.m_height = 512;
		bdrfLutDesc.m_usageStates = PB::ETextureState::COLORTARGET | PB::ETextureState::SAMPLED;
		bdrfLutDesc.m_mipCount = 1;
		if (!m_specBDRFLut)
			m_specBDRFLut = m_renderer->AllocateTexture(bdrfLutDesc);

		PB::RenderPass genBDRFRp = nullptr;
		{
			PB::RenderPassDesc genBDRFRpDesc{};
			genBDRFRpDesc.m_attachmentCount = 1;
			genBDRFRpDesc.m_subpassCount = 1;

			auto& attach = genBDRFRpDesc.m_attachments[0];
			attach.m_expectedState = PB::ETextureState::COLORTARGET;
			attach.m_finalState = PB::ETextureState::SAMPLED;
			attach.m_format = bdrfLutDesc.m_format;
			attach.m_loadAction = PB::EAttachmentAction::NONE;
			attach.m_keepContents = true;

			auto& subpass = genBDRFRpDesc.m_subpasses[0];
			subpass.m_attachments[0].m_attachmentFormat = bdrfLutDesc.m_format;
			subpass.m_attachments[0].m_attachmentIdx = 0;
			subpass.m_attachments[0].m_usage = PB::EAttachmentUsage::COLOR;

			genBDRFRp = m_renderer->GetRenderPassCache()->GetRenderPass(genBDRFRpDesc);
		}

		PB::Framebuffer framebuffer = nullptr;
		{
			PB::TextureViewDesc rtView{};
			rtView.m_expectedState = PB::ETextureState::COLORTARGET;
			rtView.m_format = bdrfLutDesc.m_format;
			rtView.m_texture = m_specBDRFLut;
			rtView.m_type = PB::ETextureViewType::VIEW_TYPE_2D;

			PB::FramebufferDesc convolutionFBDesc{};
			convolutionFBDesc.m_attachmentViews[0] = m_specBDRFLut->GetRenderTargetView(rtView);
			convolutionFBDesc.m_width = bdrfLutDesc.m_width;
			convolutionFBDesc.m_height = bdrfLutDesc.m_height;
			convolutionFBDesc.m_renderPass = genBDRFRp;

			framebuffer = m_renderer->GetFramebufferCache()->GetFramebuffer(convolutionFBDesc);
		}

		PB::Pipeline genBDRFPipeline = 0;
		{
			PB::GraphicsPipelineDesc genBDRFPipelineDesc{};
			genBDRFPipelineDesc.m_attachmentCount = 1;
			genBDRFPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
			genBDRFPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
			genBDRFPipelineDesc.m_stencilTestEnable = false;
			genBDRFPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
			genBDRFPipelineDesc.m_subpass = 0;
			genBDRFPipelineDesc.m_renderPass = genBDRFRp;
			genBDRFPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_screenQuad", 0, m_allocator, true).GetModule();
			genBDRFPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_gen_spec_bdrf_lut", 0, m_allocator, true).GetModule();
			genBDRFPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;

			genBDRFPipeline = m_renderer->GetPipelineCache()->GetPipeline(genBDRFPipelineDesc);
		}

		cmdContext->CmdTransitionTexture(m_specBDRFLut, PB::ETextureState::NONE, PB::ETextureState::COLORTARGET);
		cmdContext->CmdBeginRenderPass(genBDRFRp, bdrfLutDesc.m_width, bdrfLutDesc.m_height, framebuffer, nullptr, 0);

		cmdContext->CmdBindPipeline(genBDRFPipeline);
		cmdContext->CmdSetViewport({ 0, 0, bdrfLutDesc.m_width, bdrfLutDesc.m_height }, 0.0f, 1.0f);
		cmdContext->CmdSetScissor({ 0, 0, bdrfLutDesc.m_width, bdrfLutDesc.m_height });

		cmdContext->CmdDraw(6, 1);

		cmdContext->CmdEndRenderPass();
	}
};