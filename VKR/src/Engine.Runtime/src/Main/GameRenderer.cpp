#include "GameRenderer.h"
#include "EditorMain.h"

#include "RenderGraphPasses/RayTracingPrePass.h"
#include "RenderGraphPasses/ShadowMapPass.h"
#include "RenderGraphPasses/GBufferPass.h"
#include "RenderGraphPasses/ShadowAccumPass.h"
#include "RenderGraphPasses/ShadowBlurPass.h"
#include "RenderGraphPasses/PathTracingMainPass.h"
#include "RenderGraphPasses/ReflectionBlurPass.h"
#include "RenderGraphPasses/DeferredLightingPass.h"
#include "RenderGraphPasses/AmbientOcclusionPass.h"
#include "RenderGraphPasses/AOBlurPass.h"
#include "RenderGraphPasses/BloomExtractionPass.h"
#include "RenderGraphPasses/BloomBlurPass.h"
#include "RenderGraphPasses/DebugLinePass.h"
#include "RenderGraphPasses/TextRenderPass.h"
#include "RenderGraphPasses/ImGUIRenderPass.h"
#include "RenderGraphPasses/MergeRenderPlanesPass.h"

#include "Entity/EntityHierarchy.h"

#include "EditorMain.h"

#include <sstream>

namespace Eng
{
	using namespace Math;

	void GameRenderer::Init(PB::IRenderer* renderer, CLib::Allocator* allocator, EntityHierarchy* hierarchy, TObjectPtr<Camera> camera, EditorMain* editor)
	{
		m_renderer = renderer;
		m_swapchain = renderer->GetSwapchain();
		m_allocator = allocator;
		m_hierarchy = hierarchy;
		m_camera = camera;
		m_editor = editor;

		m_shadowCascadeSectionRanges[0] = m_camera->ZNear();
	}

	void GameRenderer::InitResources()
	{
		PB::BufferObjectDesc mvpBufferDesc;
		mvpBufferDesc.m_bufferSize = sizeof(ViewConstantsBuffer);
		mvpBufferDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
		mvpBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
		m_mvpBuffer = m_renderer->AllocateBuffer(mvpBufferDesc);

		mvpBufferDesc.m_bufferSize = sizeof(FrustrumPlanesBuffer);
		mvpBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
		m_frustrumPlanesBuffer = m_renderer->AllocateBuffer(mvpBufferDesc);

		const char* envMapName = "Textures/Sky/Arches_E_PineTree_3k";

		m_hdrSkyTexture = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, envMapName, AssetPipeline::EConvolutedMapType::SKY);
		m_skyIrradianceMap = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, envMapName, AssetPipeline::EConvolutedMapType::IRRADIANCE);
		m_skyPrefilterMap = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, envMapName, AssetPipeline::EConvolutedMapType::PREFILTER);

		{
			m_noiseTextureArray = m_allocator->Alloc<Texture>(m_renderer);

			CLib::Vector<AssetEncoder::AssetID> blueNoiseAssetIDs;
			blueNoiseAssetIDs.Reserve(64);

			for (uint32_t layer = 0; layer < blueNoiseAssetIDs.Capacity(); ++layer)
			{
				std::string name = std::to_string(layer);
				auto nameNumbered = "Textures/blue_noise/256_256/HDR_RGBA_" + std::string(4 - std::min<size_t>(4, name.length()), '0') + name; // HDR_RGBA_####

				auto id = AssetEncoder::AssetHandle(nameNumbered.c_str()).GetID(&Texture::s_textureDatabaseLoader);
				if (id != 0)
				{
					blueNoiseAssetIDs.PushBack(id);
				}
			}
			m_noiseTextureArray->Load2DArray(blueNoiseAssetIDs.Data(), blueNoiseAssetIDs.Count());
		}
	}

	void GameRenderer::DestroyResources()
	{
		// ---------------------------------------------------------------------------------------------------------------
		// RenderGraph nodes

		m_allocator->Free(m_textPass);
		m_allocator->Free(m_debugLinePass);
		m_allocator->Free(m_bloomBlurPass);
		m_allocator->Free(m_bloomExtractionPass);
		m_allocator->Free(m_aoBlurPass);
		m_allocator->Free(m_ambientOcclusionPass);
		m_allocator->Free(m_deferredLightingPass);
		if (m_reflectionBlurPass)
			m_allocator->Free(m_reflectionBlurPass);
		if (m_shadowBlurPass)
			m_allocator->Free(m_shadowBlurPass);
		if (m_shadowAccumPass)
			m_allocator->Free(m_shadowAccumPass);
		if (m_gBufferPass)
			m_allocator->Free(m_gBufferPass);
		for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
		{
			if (m_shadowmapPass[i])
				m_allocator->Free(m_shadowmapPass[i]);
		}
		if (m_rtPrePass)
			m_allocator->Free(m_rtPrePass);
		if (m_pathTracingMainPass)
			m_allocator->Free(m_pathTracingMainPass);
		// ---------------------------------------------------------------------------------------------------------------

		m_allocator->Free(m_hdrSkyTexture);
		m_allocator->Free(m_skyIrradianceMap);
		m_allocator->Free(m_skyPrefilterMap);
		m_allocator->Free(m_noiseTextureArray);

		m_renderer->FreeBuffer(m_frustrumPlanesBuffer);
		m_renderer->FreeBuffer(m_mvpBuffer);

		if (m_worldRenderOutput)
		{
			if (m_worldRenderOutputData.ref._TexID)
			{
				m_renderer->GetImGUIModule()->RemoveTexture(m_worldRenderOutputData);
			}
			m_renderer->FreeTexture(m_worldRenderOutput);
		}
	}

	void GameRenderer::CreateWorldRenderGraph(RenderGraph* graph)
	{
		bool enableRayTracing = m_renderer->GetDeviceLimitations()->m_supportRaytracing;
		bool enableRayTracedShadows = enableRayTracing;

		PB::IBufferObject* viewBuffer = m_frustrumPlanesBuffer;

		Vector3f sunDir = Vector3f(1.0f, 2.0f, 1.0f).Normalized();

		{
			RenderGraphBuilder rgBuilder(m_renderer, m_allocator);

			auto addGBufferPass = [&]()
			{
				if (!m_gBufferPass)
					m_gBufferPass = m_allocator->Alloc<GBufferPass>(m_renderer, m_allocator, m_mvpBuffer->GetViewAsUniformBuffer(), viewBuffer->GetViewAsUniformBuffer(), m_camera, m_hierarchy);
				m_gBufferPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			};

			// Ray Tracing Pre-Pass
			if (enableRayTracing)
			{
				RayTracingPrePass::CreateDesc rtPrePassDesc{};
				rtPrePassDesc.hierarchyToDraw = m_hierarchy;
				rtPrePassDesc.viewConstView = m_mvpBuffer->GetViewAsUniformBuffer();
				rtPrePassDesc.viewPlanesView = viewBuffer->GetViewAsUniformBuffer();

				if (!m_rtPrePass)
				{
					m_rtPrePass = m_allocator->Alloc<RayTracingPrePass>(m_renderer, m_allocator, rtPrePassDesc);
				}
				m_rtPrePass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);

				// GBuffer pass (Needs to happen earlier in the frame for ray tracing.
				addGBufferPass();
			}

			if (enableRayTracedShadows) // RayTracedShadow Pass
			{
				PathTracingMainPass::CreateDesc rtShadowPassDesc{};
				rtShadowPassDesc.tlas = m_rtPrePass->GetTLAS();
				rtShadowPassDesc.tlasInstanceIndexBuffer = m_rtPrePass->GetInstanceIndexBuffer();
				rtShadowPassDesc.worldConstantsBuffer = m_rtPrePass->GetSetWorldConstantsBuffer();
				rtShadowPassDesc.viewConstView = m_mvpBuffer->GetViewAsUniformBuffer();
				rtShadowPassDesc.noiseTexturesArray = m_noiseTextureArray;
				rtShadowPassDesc.skyboxTexture = m_hdrSkyTexture;
				rtShadowPassDesc.shadowRaysPerPixel = 1;
				rtShadowPassDesc.useCameraRays = false;

				if (!m_pathTracingMainPass)
				{
					m_pathTracingMainPass = m_allocator->Alloc<PathTracingMainPass>(m_renderer, m_allocator, rtShadowPassDesc);
				}
				m_pathTracingMainPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}
			else
			{
				// Shadowmap Pass
				{
					for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
					{
						if (!m_shadowmapPass[i])
						{
							ShadowMapPass::CreateDesc shadowMapPassDesc;
							shadowMapPassDesc.m_frustrumSectionNear = m_shadowCascadeSectionRanges[i * 2];
							shadowMapPassDesc.m_frustrumSectionFar = m_shadowCascadeSectionRanges[(i * 2) + 1];
							shadowMapPassDesc.m_softShadowPenumbraDistance = 0.5f;
							shadowMapPassDesc.m_shadowBiasMultiplier = 0.1f * (i + 1);
							shadowMapPassDesc.m_shadowmapResolution = ShadowmapResolution;
							shadowMapPassDesc.m_cascadeIndex = i;

							m_shadowmapPass[i] = m_allocator->Alloc<ShadowMapPass>(m_renderer, m_allocator, shadowMapPassDesc);
							m_shadowmapPass[i]->SetCamera(m_camera, m_hierarchy, &m_hierarchy->GetRenderHierarchy(), sunDir);
						}
						m_shadowmapPass[i]->AddToRenderGraph(&rgBuilder, ShadowmapResolution);
					}
				}

				// Gbuffer pass
				addGBufferPass();

				// Shadow Accumulation Pass
				{
					if (!m_shadowAccumPass)
						m_shadowAccumPass = m_allocator->Alloc<ShadowAccumPass>(m_renderer, m_allocator);
					m_shadowAccumPass->AddToRenderGraph(&rgBuilder, ShadowmapResolution, m_worldRenderResolution);
				}
			}

			// Reflection Blur Pass
			if (enableRayTracedShadows)
			{
				if (!m_reflectionBlurPass)
					m_reflectionBlurPass = m_allocator->Alloc<ReflectionBlurPass>(m_renderer, m_allocator);
				m_reflectionBlurPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Shadow Blur Pass
			{
				if (!m_shadowBlurPass)
					m_shadowBlurPass = m_allocator->Alloc<ShadowBlurPass>(m_renderer, m_allocator, enableRayTracedShadows);
				m_shadowBlurPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Ambient Occlusion pass
			{
				if (!m_ambientOcclusionPass)
				{
					AmbientOcclusionPass::CreateDesc desc{};
					desc.m_mvpUBOView = m_mvpBuffer->GetViewAsUniformBuffer();
					desc.m_halfRes = true;

					m_ambientOcclusionPass = m_allocator->Alloc<AmbientOcclusionPass>(m_renderer, m_allocator, desc);
				}
				m_ambientOcclusionPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Ambient Occlusion Blur pass
			{
				if (!m_aoBlurPass)
				{
					AOBlurPass::CreateDesc desc{};

					m_aoBlurPass = m_allocator->Alloc<AOBlurPass>(m_renderer, m_allocator, desc);
				}
				m_aoBlurPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Deferred lighting pass
			{
				if (!m_deferredLightingPass)
					m_deferredLightingPass = m_allocator->Alloc<DeferredLightingPass>(m_renderer, m_allocator, enableRayTracedShadows);
				m_deferredLightingPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Bloom Extraction Pass
			{
				if (!m_bloomExtractionPass)
					m_bloomExtractionPass = m_allocator->Alloc<BloomExtractionPass>(m_renderer, m_allocator, true);
				m_bloomExtractionPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Bloom Blur Pass
			{
				if (!m_bloomBlurPass)
					m_bloomBlurPass = m_allocator->Alloc<BloomBlurPass>(m_renderer, m_allocator, true);
				m_bloomBlurPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Debug Line Pass
			{
				if (!m_debugLinePass)
					m_debugLinePass = m_allocator->Alloc<DebugLinePass>(m_renderer, m_allocator, m_mvpBuffer->GetViewAsUniformBuffer());
				m_debugLinePass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			// Text Pass
			{
				if (!m_textPass)
					m_textPass = m_allocator->Alloc<TextRenderPass>(m_renderer, m_allocator);
				m_textPass->AddToRenderGraph(&rgBuilder, m_worldRenderResolution);
			}

			if (m_worldRenderOutput != nullptr)
			{
				m_renderer->FreeTexture(m_worldRenderOutput);

				if (m_worldRenderOutputData.ref._TexID)
				{
					m_renderer->GetImGUIModule()->RemoveTexture(m_worldRenderOutputData);
				}
			}

			constexpr const char* OutputRGName = "MergedOutput";
			const auto& outputData = rgBuilder.GetTextureData(OutputRGName);

			PB::TextureDesc worldRenderOutputDesc{};
			worldRenderOutputDesc.m_name = "WorldRender_Output";
			worldRenderOutputDesc.m_format = m_swapchain->GetImageFormat();
			worldRenderOutputDesc.m_width = m_worldRenderResolution.x;
			worldRenderOutputDesc.m_height = m_worldRenderResolution.y;
			worldRenderOutputDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_NONE;

			if constexpr (ENG_EDITOR)
			{
				worldRenderOutputDesc.m_usageStates = outputData->m_usage | PB::ETextureState::SAMPLED;
			}
			else
			{
				worldRenderOutputDesc.m_usageStates = outputData->m_usage | PB::ETextureState::COPY_SRC;
			}
			m_worldRenderOutput = m_renderer->AllocateTexture(worldRenderOutputDesc);

			if constexpr (ENG_EDITOR)
			{
				PB::TextureViewDesc outputViewDesc{};
				outputViewDesc.m_texture = m_worldRenderOutput;
				outputViewDesc.m_format = worldRenderOutputDesc.m_format;
				outputViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
				outputViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_2D;

				PB::SamplerDesc outputSamplerDesc{};
				outputSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
				outputSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;

				m_renderer->GetImGUIModule()->AddTexture(m_worldRenderOutputData, m_worldRenderOutput, outputViewDesc, outputSamplerDesc);
			}
			rgBuilder.RegisterUserTexture("MergedOutput", m_worldRenderOutput);

			rgBuilder.Build(graph, false);
		}

		// Set up lighting
		{
			m_deferredLightingPass->SetMVPBuffer(m_mvpBuffer);

			if (!enableRayTracedShadows)
			{
				m_shadowAccumPass->SetMVPBuffer(m_mvpBuffer);

				CLib::Vector<PB::UniformBufferView, ShadowCascadeCount> cascadeViews(ShadowCascadeCount);
				for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
					cascadeViews.PushBack(m_shadowmapPass[i]->GetShadowConstantsView());

				m_shadowAccumPass->SetCascadeViews(cascadeViews.Data(), ShadowCascadeCount);
			}

			Vector3f sunColor = Vector3f(2.4f);

			m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { sunColor.r, sunColor.g, sunColor.b, 1.0f });
			m_deferredLightingPass->SetSkyboxTexture(m_hdrSkyTexture, m_skyIrradianceMap, m_skyPrefilterMap);

			if (enableRayTracing)
			{
				auto& rtWorldConstants = m_rtPrePass->GetSetWorldConstants();
				rtWorldConstants.sunDirection = sunDir;
				rtWorldConstants.sunIntensity = 1.0f;
				rtWorldConstants.sunColor = sunColor;

				m_rtPrePass->SetSkyboxTexture(m_hdrSkyTexture);
			}
		}
	}

	void GameRenderer::EndFrame(float deltaTime)
	{
		if (m_rtPrePass)
		{
			m_rtPrePass->Update(deltaTime);
		}
		else
		{
			for (auto& shadowPass : m_shadowmapPass)
			{
				shadowPass->Update();
			}
		}

		// Update Camera -------------------------------------------------------------------------------------------------
		{
			constexpr float fov = 45.0f;
			float fovRadians = ToRadians(fov);

			ViewConstantsBuffer* bufferMatrices = (ViewConstantsBuffer*)m_mvpBuffer->BeginPopulate();

			// View
			bufferMatrices->m_view = m_camera->GetViewMatrix(); // View
			bufferMatrices->m_invView = Inverse(bufferMatrices->m_view);

			// Projection
			bufferMatrices->m_proj = m_camera->GetProjectionMatrix();
			bufferMatrices->m_invProj = Inverse(bufferMatrices->m_proj);

			bufferMatrices->m_viewProjLastFrame = m_viewProjLastFrame;
			bufferMatrices->m_viewProj = m_viewProjLastFrame = bufferMatrices->m_proj * bufferMatrices->m_view;

			// Position
			bufferMatrices->m_camPosLastFrame = bufferMatrices->m_camPos;
			bufferMatrices->m_camPos = m_camera->Position();

			// Depth Reconstruction Constants
			bufferMatrices->m_aspectRatio = float(m_worldRenderResolution.x) / m_worldRenderResolution.y;
			bufferMatrices->m_tanHalfFOV = Tan(fovRadians / 2);

			m_mvpBuffer->EndPopulate();

			// Frustrum
			FrustrumPlanesBuffer* frustrumPlanes = (FrustrumPlanesBuffer*)m_frustrumPlanesBuffer->BeginPopulate();
			const Camera::CameraFrustrum& frustrum = m_camera->GetFrustrum();

			frustrumPlanes->m_planes[0] = frustrum.m_near;
			frustrumPlanes->m_planes[1] = frustrum.m_left;
			frustrumPlanes->m_planes[2] = frustrum.m_right;
			frustrumPlanes->m_planes[3] = frustrum.m_top;
			frustrumPlanes->m_planes[4] = frustrum.m_bottom;
			frustrumPlanes->m_planes[5] = frustrum.m_far;
			frustrumPlanes->m_camPos = m_camera->Position();
			frustrumPlanes->m_isOrthographic = 0;

			m_frustrumPlanesBuffer->EndPopulate();
		}
		// ---------------------------------------------------------------------------------------------------------------

		// ---------------------------------------------------------------------------------------------------------------
		// Execute main render graph.
		{
			PB::CommandContextDesc contextDesc{};
			contextDesc.m_renderer = m_renderer;
			contextDesc.m_usage = PB::ECommandContextUsage::GRAPHICS;

			PB::SCommandContext cmdContext(m_renderer);
			cmdContext->Init(contextDesc);
			cmdContext->Begin();

			m_mainRenderGraph.Execute(cmdContext.GetContext());
			if (m_editor != nullptr) // If the editor is enabled, the world rendering output is shown in the viewport window.
			{
				m_editor->RenderGUI(cmdContext.GetContext());
			}
			else // Otherwise copy the world rendering output straight onto the swapchain image.
			{
				PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
				auto* swapChainTex = m_renderer->GetSwapchain()->GetImage(swapChainIdx);

				cmdContext->CmdTransitionTexture(swapChainTex, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);
				cmdContext->CmdCopyTextureToTexture(m_worldRenderOutput, swapChainTex);
				cmdContext->CmdTransitionTexture(swapChainTex, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
			}

			cmdContext->End();
			cmdContext->Return();
		}
		// ---------------------------------------------------------------------------------------------------------------
	}

	void GameRenderer::UpdateResolution(uint32_t width, uint32_t height)
	{
		m_worldRenderResolution = Vector2u(width, height);

		m_mainRenderGraph.~RenderGraph();
		m_mainRenderGraph = RenderGraph();

		CreateWorldRenderGraph(&m_mainRenderGraph);
	}

	void GameRenderer::SetCamera(TObjectPtr<Camera> camera)
	{
		m_camera = camera;

		m_shadowCascadeSectionRanges[0] = m_camera->ZNear();
	}
}