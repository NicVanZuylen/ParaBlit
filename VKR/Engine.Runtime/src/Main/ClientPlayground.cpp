#include "ClientPlayground.h"

#include "WindowHandle.h"

#include "CLib/Allocator.h"

#include "RenderGraph/RenderGraph.h"

#include "RenderGraphPasses/ShadowMapPass.h"
#include "RenderGraphPasses/GBufferPass.h"
#include "RenderGraphPasses/ShadowAccumPass.h"
#include "RenderGraphPasses/ShadowBlurPass.h"
#include "RenderGraphPasses/DeferredLightingPass.h"
#include "RenderGraphPasses/AmbientOcclusionPass.h"
#include "RenderGraphPasses/AOBlurPass.h"
#include "RenderGraphPasses/BloomExtractionPass.h"
#include "RenderGraphPasses/BloomBlurPass.h"
#include "RenderGraphPasses/DebugLinePass.h"
#include "RenderGraphPasses/TextRenderPass.h"

#include "Utility/Input.h"

#include "Resource/FontTexture.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "Resource/Material.h"

#include "World/DrawBatch.h"
#include "World/ObjectDispatcher.h"
#include "World/RenderBoundingVolumeHierarchy.h"

#include "glm/gtc/type_ptr.hpp"

#include "Entity/Component/Transform.h"
#include "Entity/Component/RenderDefinition.h"

#include <sstream>
#include <iostream>
#include <random>

namespace Eng
{
	ClientPlayground::ClientPlayground(PB::IRenderer* renderer, CLib::Allocator* allocator)
	{
		m_renderer = renderer;
		m_swapchain = m_renderer->GetSwapchain();
		m_allocator = allocator;

		glm::vec3 sunDir(1.0f, 1.0f, 0.0f);

		Camera::CreateDesc cameraDesc;
		cameraDesc.m_position = glm::vec3(0.0f, 1.2f, 4.0f);
		cameraDesc.m_eulerAngles = glm::radians(glm::vec3(-45.0f, 0.0f, 0.0f));
		cameraDesc.m_sensitivity = 0.5f;
		cameraDesc.m_moveSpeed = 20.0f;
		cameraDesc.m_width = float(m_swapchain->GetWidth());
		cameraDesc.m_height = float(m_swapchain->GetHeight());
		cameraDesc.m_fovY = glm::radians(45.0f);
		cameraDesc.m_zFar = 500.0f;
		m_camera = Camera(cameraDesc);
		m_shadowCascadeSectionRanges[0] = m_camera.ZNear();

		InitResources();

		RenderBoundingVolumeHierarchy::CreateDesc rbvhDesc{};
		rbvhDesc.m_desiredMaxDepth = 50;

		rbvhDesc.m_toleranceDistanceX = 0.05f;
		rbvhDesc.m_toleranceDistanceY = 0.05f;

		rbvhDesc.m_toleranceStepX = 0.05f;
		rbvhDesc.m_toleranceStepZ = 0.05f;

		rbvhDesc.m_toleranceDistanceY = 0.2f;
		rbvhDesc.m_toleranceStepY = 0.2f;

		rbvhDesc.m_camera = &m_camera;

		m_renderHierarchy = m_allocator->Alloc<RenderBoundingVolumeHierarchy>(m_renderer, m_allocator, rbvhDesc);

		m_renderGraph = CreateRenderGraph();
		SetupDrawBatch();
	}

	ClientPlayground::~ClientPlayground()
	{
		m_allocator->Free(m_renderHierarchy);

		m_allocator->Free(m_spinnerPaintEntity);
		m_allocator->Free(m_spinnerDetailsEntity);
		m_allocator->Free(m_spinnerGlassEntity);

		DestroyResources();

		m_allocator->Free(m_renderGraph);

		// Free rendergraph nodes.
		m_allocator->Free(m_textPass);
		m_allocator->Free(m_debugLinePass);
		m_allocator->Free(m_bloomBlurPass);
		m_allocator->Free(m_bloomExtractionPass);
		m_allocator->Free(m_aoBlurPass);
		m_allocator->Free(m_ambientOcclusionPass);
		m_allocator->Free(m_deferredLightingPass);
		m_allocator->Free(m_shadowBlurPass);
		m_allocator->Free(m_shadowAccumPass);
		m_allocator->Free(m_gBufferPass);

		for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
			m_allocator->Free(m_shadowmapPass[i]);

		m_allocator->Free(m_fontTexture);
	}

	void ClientPlayground::Update(GLFWwindow* window, Input* input, float deltaTime, float elapsedTime, float stallTime, bool updateMetrics)
	{
		m_camera.UpdateFreeCam(deltaTime, input, window);

		// Update Text ---------------------------------------------------------------------------------------------------
		{
			if (m_cpuTimeText && updateMetrics)
			{
				PB::Float2 anchorPos(20.0f, 20.0f);

				float swapchainWidthf = static_cast<float>(m_swapchain->GetWidth());
				float swapchainHeightf = static_cast<float>(m_swapchain->GetHeight());
				float fontHeightf = static_cast<float>(m_fontTexture->GetFontHeight());
				float anchorHeight = swapchainHeightf - fontHeightf - anchorPos.y;

				std::ostringstream str;
				str.precision(3);
				str << "CPU Time: " << ((deltaTime * 1000.0f) - stallTime) << "ms";

				m_textPass->TextReplace(m_cpuTimeText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight));

				str = std::ostringstream();
				str << "FPS: " << (1.0f / deltaTime);

				m_textPass->TextReplace(m_fpsText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight - fontHeightf));
			}
		}

		m_debugLinePass->DrawLine(PB::Float4(0.0f, 0.0f, 0.0f, 1.0f), PB::Float4(1.0f, 0.0f, 0.0f, 1.0f), PB::Float4(1.0f, 0.0f, 0.0f, 1.0f));
		m_debugLinePass->DrawLine(PB::Float4(0.0f, 0.0f, 0.0f, 1.0f), PB::Float4(0.0f, 1.0f, 0.0f, 1.0f), PB::Float4(0.0f, 1.0f, 0.0f, 1.0f));
		m_debugLinePass->DrawLine(PB::Float4(0.0f, 0.0f, 0.0f, 1.0f), PB::Float4(0.0f, 0.0f, 1.0f, 1.0f), PB::Float4(0.0f, 0.0f, 1.0f, 1.0f));

		if (!input->GetKey(GLFW_KEY_PAGE_UP, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_PAGE_UP, INPUTSTATE_PREVIOUS) && m_renderHierarchyDrawDebugDepth < 50)
		{
			++m_renderHierarchyDrawDebugDepth;
			std::cout << "BVH: Drawing at depth: " << m_renderHierarchyDrawDebugDepth << "\n";
		}

		if (!input->GetKey(GLFW_KEY_PAGE_DOWN, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_PAGE_DOWN, INPUTSTATE_PREVIOUS) && m_renderHierarchyDrawDebugDepth > 0)
		{
			--m_renderHierarchyDrawDebugDepth;
			std::cout << "BVH: Drawing at depth: " << m_renderHierarchyDrawDebugDepth << "\n";
		}

		if (!input->GetKey(GLFW_KEY_HOME, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_HOME, INPUTSTATE_PREVIOUS))
		{
			m_drawEntireRenderHierarchy = !m_drawEntireRenderHierarchy;
			std::cout << "BVH: Drawing whole tree: " << (m_drawEntireRenderHierarchy ? "true" : "false") << "\n";
		}

		if (m_drawEntireRenderHierarchy)
			m_renderHierarchy->DebugDraw(m_debugLinePass, m_renderHierarchyDrawDebugDepth, true);

		for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
		{
			m_shadowmapPass[i]->Update();
		}

		// Update Camera -------------------------------------------------------------------------------------------------
		{
			constexpr float fov = 45.0f;
			constexpr float fovRadians = glm::radians(fov);

			ViewConstantsBuffer* bufferMatrices = (ViewConstantsBuffer*)m_mvpBuffer->BeginPopulate();

			// View
			bufferMatrices->m_view = m_camera.GetViewMatrix(); // View
			bufferMatrices->m_invView = glm::inverse(bufferMatrices->m_view);

			// Projection
			bufferMatrices->m_proj = m_camera.GetProjectionMatrix();
			bufferMatrices->m_invProj = glm::inverse(bufferMatrices->m_proj);

			bufferMatrices->m_viewProj = bufferMatrices->m_proj * bufferMatrices->m_view;

			// Position
			bufferMatrices->m_camPos = glm::vec4(m_camera.Position(), 1.0f);

			// Depth Reconstruction Constants
			bufferMatrices->m_aspectRatio = float(m_swapchain->GetWidth()) / m_swapchain->GetHeight();
			bufferMatrices->m_tanHalfFOV = glm::tan(fovRadians / 2);

			// Frustrum
			const Camera::CameraFrustrum& frustrum = m_camera.GetFrustrum();

			bufferMatrices->m_mainFrustrumPlanes[0] = frustrum.m_near;
			bufferMatrices->m_mainFrustrumPlanes[1] = frustrum.m_left;
			bufferMatrices->m_mainFrustrumPlanes[2] = frustrum.m_right;
			bufferMatrices->m_mainFrustrumPlanes[3] = frustrum.m_top;
			bufferMatrices->m_mainFrustrumPlanes[4] = frustrum.m_bottom;
			bufferMatrices->m_mainFrustrumPlanes[5] = frustrum.m_far;

			m_mvpBuffer->EndPopulate();
		}
		// ---------------------------------------------------------------------------------------------------------------

		// Change render node output -------------------------------------------------------------------------------------
		{
			PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
			auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);
			//m_gBufferPass->SetOutputTexture(swapChainTex);
			//m_bloomBlurPass->SetOutputTexture(swapChainTex);
			//m_ambientOcclusionPass->SetOutputTexture(swapChainTex);
			m_textPass->SetOutputTexture(swapChainTex);
		}
		// ---------------------------------------------------------------------------------------------------------------

		m_renderGraph->Execute();
	}

	void ClientPlayground::UpdateResolution(uint32_t width, uint32_t height)
	{
		m_allocator->Free(m_renderGraph);
		m_renderGraph = CreateRenderGraph();

		PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
		auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);
		m_textPass->SetOutputTexture(swapChainTex);

		m_camera.SetWidth(float(width));
		m_camera.SetHeight(float(height));
	}

	void ClientPlayground::InitResources()
	{
		PB::BufferObjectDesc mvpBufferDesc;
		mvpBufferDesc.m_bufferSize = sizeof(ViewConstantsBuffer);
		mvpBufferDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
		mvpBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
		m_mvpBuffer = m_renderer->AllocateBuffer(mvpBufferDesc);

		// Basic textures
		PB::u8 texData[4] = { 255, 255, 255, 255 };

		PB::TextureDataDesc dataDesc{};
		dataDesc.m_data = texData;
		dataDesc.m_size = sizeof(texData);

		PB::TextureDesc solidTextureDesc{};
		solidTextureDesc.m_format = PB::ETextureFormat::R8G8B8A8_SRGB;
		solidTextureDesc.m_data = &dataDesc;
		solidTextureDesc.m_width = 1;
		solidTextureDesc.m_height = 1;
		solidTextureDesc.m_initialState = PB::ETextureState::SAMPLED;
		solidTextureDesc.m_usageStates = PB::ETextureState::SAMPLED;
		solidTextureDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
		m_solidWhiteTexture = m_renderer->AllocateTexture(solidTextureDesc);

		texData[0] = 0;
		texData[1] = 0;
		texData[2] = 0;
		m_solidBlackTexture = m_renderer->AllocateTexture(solidTextureDesc);

		solidTextureDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		texData[0] = 128;
		texData[1] = 128;
		texData[2] = 255;
		m_flatNormalTexture = m_renderer->AllocateTexture(solidTextureDesc);

		// Shaders
		m_shadowVertShader = m_allocator->Alloc<Eng::Shader>(m_renderer, "Shaders/GLSL/vs_obj_shad_batch", m_allocator, true);

		// Vertex pool
		m_vertexPool = m_allocator->Alloc<VertexPool>(m_renderer, uint32_t(sizeof(Eng::Vertex) * 1000000), uint32_t(sizeof(Eng::Vertex)));

		// Meshes & Textures
		m_paintMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_paint", m_vertexPool);
		m_detailsMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_details", m_vertexPool);
		m_glassMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_glass", m_vertexPool);
		m_planeMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Primitives/plane", m_vertexPool);

		m_paintTextures[0] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/paint2048/m_spinner_paint_diffuse", true, false, true);
		m_detailsTextures[0] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/details2048/m_spinner_details_diffuse", true, false, true);
		m_glassTextures[0] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/glass2048/m_spinner_glass_diffuse", true, false, true);

		m_paintTextures[1] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/paint2048/m_spinner_paint_normal", false, false, true);
		m_detailsTextures[1] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/details2048/m_spinner_details_normal", false, false, true);
		m_glassTextures[1] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/glass2048/m_spinner_glass_normal", false, false, true);

		m_paintTextures[2] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/paint2048/m_spinner_paint_specular_v2", true, false, true);
		m_detailsTextures[2] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/details2048/m_spinner_details_specular", true, false, true);
		m_glassTextures[2] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/glass2048/m_spinner_glass_specular", true, false, true);

		m_paintTextures[3] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/paint2048/m_spinner_paint_roughness", false, false, true);
		m_detailsTextures[3] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/details2048/m_spinner_details_roughness", false, false, true);
		m_glassTextures[3] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/glass2048/m_spinner_glass_roughness", false, false, true);

		m_detailsTextures[4] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/details2048/m_spinner_details_emissive", true, false, true);
		m_glassTextures[4] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Spinner/glass2048/m_spinner_glass_emissive", true, false, true);

		m_metalTextures[0] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Metal/diffuse", true, false, true);
		m_metalTextures[1] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Metal/normal", false, false, true);

		m_debugTextures[0] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Debug/debug_albedo", true, false, true);
		m_debugTextures[1] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Debug/debug_roughness", false, false, true);
		m_debugTextures[2] = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, "Textures/Debug/debug_specular", true, false, true);

		for (int i = 0; i < _countof(m_paintTextures); ++i)
			m_paintViews[i] = m_paintTextures[i]->GetTexture()->GetDefaultSRV();
		m_paintViews[4] = m_solidBlackTexture->GetDefaultSRV();

		for (int i = 0; i < _countof(m_detailsTextures); ++i)
			m_detailsViews[i] = m_detailsTextures[i]->GetTexture()->GetDefaultSRV();

		for (int i = 0; i < _countof(m_glassTextures); ++i)
			m_glassViews[i] = m_glassTextures[i]->GetTexture()->GetDefaultSRV();

		for (int i = 0; i < _countof(m_debugTextures); ++i)
			m_debugViews[i] = m_debugTextures[i]->GetTexture()->GetDefaultSRV();

		const char* skyboxFilenames[6]
		{
			"Assets/Textures/Skybox/right.jpg",
			"Assets/Textures/Skybox/left.jpg",
			"Assets/Textures/Skybox/top.jpg",
			"Assets/Textures/Skybox/bottom.jpg",
			"Assets/Textures/Skybox/front.jpg",
			"Assets/Textures/Skybox/back.jpg"
		};

		const char* envMapName = "Textures/Sky/Arches_E_PineTree_3k";

		m_hdrSkyTexture = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, envMapName, AssetPipeline::EConvolutedMapType::SKY);
		m_skyIrradianceMap = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, envMapName, AssetPipeline::EConvolutedMapType::IRRADIANCE);
		m_skyPrefilterMap = m_allocator->Alloc<Eng::Texture>(m_renderer, m_allocator, envMapName, AssetPipeline::EConvolutedMapType::PREFILTER);

		m_fontTexture = m_allocator->Alloc<Eng::FontTexture>(m_renderer, "Assets/Fonts/arial.ttf", 32);

		PB::SamplerDesc colorSamplerDesc;
		colorSamplerDesc.m_anisotropyLevels = 1.0f;
		colorSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
		m_colorSampler = m_renderer->GetSampler(colorSamplerDesc);

		PB::ITexture* paintTextures[5]
		{
			m_paintTextures[0]->GetTexture(),
			m_paintTextures[1]->GetTexture(),
			m_paintTextures[2]->GetTexture(),
			m_paintTextures[3]->GetTexture(),
			m_solidBlackTexture
		};
		m_spinnerMaterials[0] = m_allocator->Alloc<Eng::Material>(0, paintTextures, _countof(paintTextures), m_colorSampler);
		m_spinnerPaintEntity = m_allocator->Alloc<Eng::Entity>();
		m_spinnerPaintEntity->AddComponent<Eng::Transform>();
		m_spinnerPaintEntity->AddComponent<Eng::RenderDefinition>(m_paintMesh, m_spinnerMaterials[0]);

		PB::ITexture* detailsTextures[5]
		{
			m_detailsTextures[0]->GetTexture(),
			m_detailsTextures[1]->GetTexture(),
			m_detailsTextures[2]->GetTexture(),
			m_detailsTextures[3]->GetTexture(),
			m_detailsTextures[4]->GetTexture()
		};
		m_spinnerMaterials[1] = m_allocator->Alloc<Eng::Material>(0, detailsTextures, _countof(detailsTextures), m_colorSampler);
		m_spinnerDetailsEntity = m_allocator->Alloc<Eng::Entity>();
		m_spinnerDetailsEntity->AddComponent<Eng::Transform>();
		m_spinnerDetailsEntity->AddComponent<Eng::RenderDefinition>(m_detailsMesh, m_spinnerMaterials[1]);

		PB::ITexture* glassTextures[5]
		{
			m_glassTextures[0]->GetTexture(),
			m_glassTextures[1]->GetTexture(),
			m_glassTextures[2]->GetTexture(),
			m_glassTextures[3]->GetTexture(),
			m_glassTextures[4]->GetTexture()
		};
		m_spinnerMaterials[2] = m_allocator->Alloc<Eng::Material>(0, glassTextures, _countof(glassTextures), m_colorSampler);
		m_spinnerGlassEntity = m_allocator->Alloc<Eng::Entity>();
		m_spinnerGlassEntity->AddComponent<Eng::Transform>();
		m_spinnerGlassEntity->AddComponent<Eng::RenderDefinition>(m_glassMesh, m_spinnerMaterials[2]);

		PB::ITexture* planeTextures[5]
		{
			m_solidWhiteTexture,
			m_flatNormalTexture,
			m_solidBlackTexture,
			m_solidWhiteTexture,
			m_solidBlackTexture
		};
		m_planeMaterial = m_allocator->Alloc<Eng::Material>(0, planeTextures, _countof(planeTextures), m_colorSampler);

		planeTextures[0] = m_debugTextures[0]->GetTexture();
		planeTextures[2] = m_debugTextures[2]->GetTexture();
		planeTextures[3] = m_debugTextures[1]->GetTexture();
		m_debugMaterial = m_allocator->Alloc<Eng::Material>(0, planeTextures, _countof(planeTextures), m_colorSampler);
	}

	void ClientPlayground::DestroyResources()
	{
		for (Eng::Material* mat : m_spinnerMaterials)
			m_allocator->Free(mat);

		m_allocator->Free(m_planeMaterial);
		m_allocator->Free(m_debugMaterial);

		m_renderer->FreeTexture(m_solidWhiteTexture);
		m_renderer->FreeTexture(m_solidBlackTexture);
		m_renderer->FreeTexture(m_flatNormalTexture);

		for (auto& tex : m_paintTextures)
		{
			m_allocator->Free(tex);
			tex = nullptr;
		}
		for (auto& tex : m_detailsTextures)
		{
			m_allocator->Free(tex);
			tex = nullptr;
		}
		for (auto& tex : m_glassTextures)
		{
			m_allocator->Free(tex);
			tex = nullptr;
		}
		for (auto& tex : m_metalTextures)
		{
			m_allocator->Free(tex);
			tex = nullptr;
		}
		for (auto& tex : m_debugTextures)
		{
			m_allocator->Free(tex);
			tex = nullptr;
		}

		m_allocator->Free(m_hdrSkyTexture);
		m_allocator->Free(m_skyIrradianceMap);
		m_allocator->Free(m_skyPrefilterMap);

		m_allocator->Free(m_paintMesh);
		m_allocator->Free(m_detailsMesh);
		m_allocator->Free(m_glassMesh);
		m_allocator->Free(m_planeMesh);

		m_allocator->Free(m_shadowVertShader);

		m_allocator->Free(m_vertexPool);

		m_renderer->FreeBuffer(m_mvpBuffer);
		m_mvpBuffer = nullptr;
	}

	inline RenderGraph* ClientPlayground::CreateRenderGraph()
	{
		uint32_t frustrumPlanesOffset = offsetof(ViewConstantsBuffer, ViewConstantsBuffer::m_mainFrustrumPlanes);
		uint32_t frustrumPlanesSize = sizeof(ViewConstantsBuffer::m_mainFrustrumPlanes);

		PB::BufferViewDesc viewPlanesDesc;
		viewPlanesDesc.m_buffer = m_mvpBuffer;
		viewPlanesDesc.m_offset = frustrumPlanesOffset;
		viewPlanesDesc.m_size = frustrumPlanesSize;

		glm::vec3 sunDir = glm::normalize(glm::vec3(1.0f, 2.0f, 1.0f));

		RenderGraph* output = nullptr;
		{
			RenderGraphBuilder rgBuilder(m_renderer, m_allocator);

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
						m_shadowmapPass[i]->SetCamera(&m_camera, m_renderHierarchy, sunDir);
					}
					m_shadowmapPass[i]->AddToRenderGraph(&rgBuilder, ShadowmapResolution);
				}
			}

			// GBuffer pass
			{
				if (!m_gBufferPass)
					m_gBufferPass = m_allocator->Alloc<GBufferPass>(m_renderer, m_allocator, m_mvpBuffer->GetViewAsUniformBuffer(), m_mvpBuffer->GetViewAsUniformBuffer(viewPlanesDesc), &m_camera, m_renderHierarchy);
				m_gBufferPass->AddToRenderGraph(&rgBuilder);
			}

			// Shadow Accumulation Pass
			{
				if (!m_shadowAccumPass)
					m_shadowAccumPass = m_allocator->Alloc<ShadowAccumPass>(m_renderer, m_allocator);
				m_shadowAccumPass->AddToRenderGraph(&rgBuilder, ShadowmapResolution);
			}

			// Shadow Blur Pass
			{
				if (!m_shadowBlurPass)
					m_shadowBlurPass = m_allocator->Alloc<ShadowBlurPass>(m_renderer, m_allocator);
				m_shadowBlurPass->AddToRenderGraph(&rgBuilder);
			}

			// Ambient Occlusion pass
			{
				if (!m_ambientOcclusionPass)
				{
					AmbientOcclusionPass::CreateDesc desc{};
					desc.m_mvpUBOView = m_mvpBuffer->GetViewAsUniformBuffer();
					desc.m_halfRes = false;

					m_ambientOcclusionPass = m_allocator->Alloc<AmbientOcclusionPass>(m_renderer, m_allocator, desc);
				}
				m_ambientOcclusionPass->AddToRenderGraph(&rgBuilder);
			}

			// Ambient Occlusion Blur pass
			{
				if (!m_aoBlurPass)
				{
					AOBlurPass::CreateDesc desc{};
					desc.m_halfRes = false;

					m_aoBlurPass = m_allocator->Alloc<AOBlurPass>(m_renderer, m_allocator, desc);
				}
				m_aoBlurPass->AddToRenderGraph(&rgBuilder);
			}

			// Deferred lighting pass
			{
				if (!m_deferredLightingPass)
					m_deferredLightingPass = m_allocator->Alloc<DeferredLightingPass>(m_renderer, m_allocator);
				m_deferredLightingPass->AddToRenderGraph(&rgBuilder);
			}

			// Bloom Extraction Pass
			{
				if (!m_bloomExtractionPass)
					m_bloomExtractionPass = m_allocator->Alloc<BloomExtractionPass>(m_renderer, m_allocator, true);
				m_bloomExtractionPass->AddToRenderGraph(&rgBuilder);
			}

			// Bloom Blur Pass
			{
				if (!m_bloomBlurPass)
					m_bloomBlurPass = m_allocator->Alloc<BloomBlurPass>(m_renderer, m_allocator, true);
				m_bloomBlurPass->AddToRenderGraph(&rgBuilder);
			}

			// Debug Line Pass
			{
				if (!m_debugLinePass)
					m_debugLinePass = m_allocator->Alloc<DebugLinePass>(m_renderer, m_allocator, m_mvpBuffer->GetViewAsUniformBuffer());
				m_debugLinePass->AddToRenderGraph(&rgBuilder);
			}

			// Text Pass
			{
				if (!m_textPass)
					m_textPass = m_allocator->Alloc<TextRenderPass>(m_renderer, m_allocator);
				m_textPass->AddToRenderGraph(&rgBuilder);
			}

			output = rgBuilder.Build(false);
		}

		float fontHeight = static_cast<float>(m_fontTexture->GetFontHeight());

		if (!m_cpuTimeText)
		{
			m_cpuTimeText = m_textPass->AddText("CPU Time: 000000ms", m_fontTexture, PB::Float2(0.0f, 0.0f));
			m_fpsText = m_textPass->AddText("FPS: 000000", m_fontTexture, PB::Float2(0.0f, float(m_fontTexture->GetFontHeight())));
		}

		// Set up lighting
		{
			m_deferredLightingPass->SetMVPBuffer(m_mvpBuffer);

			m_shadowAccumPass->SetMVPBuffer(m_mvpBuffer);

			CLib::Vector<PB::UniformBufferView, ShadowCascadeCount> cascadeViews(ShadowCascadeCount);
			for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
				cascadeViews.PushBack(m_shadowmapPass[i]->GetShadowConstantsView());

			m_shadowAccumPass->SetCascadeViews(cascadeViews.Data(), ShadowCascadeCount);

			glm::vec3 sunColor = glm::vec3(2.4f);

			m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { sunColor.r, sunColor.g, sunColor.b, 1.0f });
			m_deferredLightingPass->SetSkyboxTexture(m_hdrSkyTexture, m_skyIrradianceMap, m_skyPrefilterMap);
		}

		return output;
	}

	void ClientPlayground::SetupDrawBatch()
	{
		PB::UniformBufferView mvpView = m_mvpBuffer->GetViewAsUniformBuffer();

		glm::mat4 modelMat = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, 0.0f));
		glm::mat4 spinnerModelMat = glm::scale(modelMat, glm::vec3(0.01f)); // Convert cm to m.

		PB::ResourceView plainViews[]
		{
			m_solidWhiteTexture->GetDefaultSRV(),
			m_flatNormalTexture->GetDefaultSRV(),
			m_solidBlackTexture->GetDefaultSRV(),
			m_solidWhiteTexture->GetDefaultSRV(),
			m_solidBlackTexture->GetDefaultSRV()
		};

		//PB::ResourceView plainViews[]
		//{
		//	m_metalTextures[0]->GetTexture()->GetDefaultSRV(),
		//	m_metalTextures[1]->GetTexture()->GetDefaultSRV(),
		//	m_solidBlackTexture->GetDefaultSRV(),
		//	m_solidBlackTexture->GetDefaultSRV()
		//};

		CLib::Vector<RenderBoundingVolumeHierarchy::ObjectData> nodes;

		//Bounds spinnerBounds(glm::vec3(-1.2f, 0.0f, -2.5f), glm::vec3(2.4f, 1.5f, 5.0f));
		Bounds spinnerBounds = m_paintMesh->GetBounds();
		spinnerBounds.Encapsulate(m_detailsMesh->GetBounds());
		spinnerBounds.Encapsulate(m_glassMesh->GetBounds());
		//spinnerBounds.m_origin *= 0.01f;
		//spinnerBounds.m_extents *= 0.01f;

		Eng::Transform* spinnerTransform = m_spinnerPaintEntity->GetComponent<Eng::Transform>();

		const uint32_t spinnerCount = 2;
		for (uint32_t i = 0; i < spinnerCount; ++i)
		{
			//glm::vec3 pos = glm::vec3(4.0f * (i / 10), 0.0f, -7.0f * (i % 10));
			glm::vec3 pos = glm::vec3(0.0f, 0.0f, -3.0f + (i * 6.0f));

			if (i == 1)
			{
				pos.y = 5.0f;
				pos.z += 2.5f;
			}

			CLib::Reflector transformReflector;
			spinnerTransform->GetReflection(transformReflector);

			*transformReflector.GetFieldWithName<glm::vec3>("m_translation") = pos;
			*transformReflector.GetFieldWithName<glm::quat>("m_quaternion") = glm::rotate(glm::identity<glm::quat>(), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			*transformReflector.GetFieldWithName<glm::vec3>("m_scale") = glm::vec3(0.01f);

			spinnerModelMat = spinnerTransform->GetMatrix();

			Bounds translatedBounds = spinnerBounds;
			translatedBounds.Transform(spinnerModelMat);

			RenderBoundingVolumeHierarchy::ObjectData& paintObj = nodes.PushBack();
			paintObj.m_mesh = m_paintMesh;
			paintObj.m_material = m_spinnerMaterials[0];
			paintObj.m_transform = spinnerModelMat;

			RenderBoundingVolumeHierarchy::ObjectData& detailsObj = nodes.PushBack();
			detailsObj.m_mesh = m_detailsMesh;
			detailsObj.m_material = m_spinnerMaterials[1];
			detailsObj.m_transform = spinnerModelMat;

			RenderBoundingVolumeHierarchy::ObjectData& glassObj = nodes.PushBack();
			glassObj.m_mesh = m_glassMesh;
			glassObj.m_material = m_spinnerMaterials[2];
			glassObj.m_transform = spinnerModelMat;
		}

		modelMat = glm::identity<glm::mat4>();

		glm::vec3 planeOffset = glm::vec3(0.0f, -0.2f, 0.0f);
		Bounds planeBounds = m_planeMesh->GetBounds();

		modelMat = glm::translate(glm::mat4(), planeOffset);
		planeBounds.Transform(modelMat);

		RenderBoundingVolumeHierarchy::ObjectData& planeObj = nodes.PushBack();
		planeObj.m_mesh = m_planeMesh;
		planeObj.m_material = m_planeMaterial;
		planeObj.m_transform = modelMat;

		plainViews[0] = m_debugViews[0];
		plainViews[2] = m_debugViews[2];
		plainViews[3] = m_debugViews[1];

		glm::vec3 debugPlaneOffset = glm::vec3(-2.4f, 1.0f, 0.0f);
		Bounds debugPlaneBounds = m_planeMesh->GetBounds();

		modelMat = glm::identity<glm::mat4>();
		modelMat = glm::translate(modelMat, debugPlaneOffset);
		modelMat = glm::rotate(modelMat, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		modelMat = glm::scale(modelMat, glm::vec3(0.2f, 0.2f, 0.2f));
		debugPlaneBounds.Transform(modelMat);

		RenderBoundingVolumeHierarchy::ObjectData& debugPlaneObj = nodes.PushBack();
		debugPlaneObj.m_mesh = m_planeMesh;
		debugPlaneObj.m_material = m_debugMaterial;
		debugPlaneObj.m_transform = modelMat;

		m_renderHierarchy->BuildBottomUp(nodes);

		PB::CommandContextDesc contextDesc{};
		contextDesc.m_renderer = m_renderer;
		contextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
		contextDesc.m_flags = PB::ECommandContextFlags::PRIORITY;

		PB::SCommandContext initCmdContext(m_renderer);
		initCmdContext->Init(contextDesc);
		initCmdContext->Begin();

		m_renderHierarchy->BakeHierarchies(initCmdContext.GetContext());

		initCmdContext->End();
		initCmdContext->Return();
	}

	PB::Pipeline ClientPlayground::GetShadowDrawBatchPipeline()
	{
		PB::GraphicsPipelineDesc pipelineDesc = m_shadowmapPass[0]->GetBasePipelineDesc(ShadowmapResolution);
		pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_shadowVertShader->GetModule();

		return m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
	}

	PB::BindingLayout ClientPlayground::GetGBufferDrawBatchBindings(PB::UniformBufferView& mvpView)
	{
		PB::BindingLayout layout{};
		layout.m_uniformBufferCount = 1;
		layout.m_uniformBuffers = &mvpView;

		return layout;
	}
};