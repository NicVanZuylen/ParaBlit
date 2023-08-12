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

#include "WorldRender/DrawBatch.h"
#include "WorldRender/ObjectDispatcher.h"

#include "glm/gtc/type_ptr.hpp"

#include "Entity/Component/Transform.h"
#include "Entity/Component/RenderDefinition.h"

#include <sstream>
#include <iostream>
#include <random>
#include <filesystem>

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

		cameraDesc.m_width = cameraDesc.m_height / 4;
		cameraDesc.m_position = glm::vec3(-16.0f, 1.0f, 16.0f);
		cameraDesc.m_eulerAngles = glm::radians(glm::vec3(0.0f, -55.0f, 0.0f));
		m_frustrumTestCamera = Camera(cameraDesc);

		std::string dbDir = std::filesystem::current_path().string();
		std::string databasePath = dbDir + "/Assets/build";
		m_assetStreamer.Init(m_renderer, databasePath.c_str());

		InitResources();

		m_hierarchy.Init(m_allocator, m_renderer, &m_assetStreamer);

		m_renderGraph = CreateRenderGraph();
		SetupDrawBatch();
	}

	ClientPlayground::~ClientPlayground()
	{
		m_hierarchy.Destroy();
		DestroyResources();
		m_assetStreamer.Shutdown();

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

				str = std::ostringstream();
				str << "Selected Entity: ";
				if (m_selectedEntity != nullptr)
					str << m_selectedEntity->GetName();
				else
					str << "None";

				m_textPass->TextReplace(m_selectedEntityText, str.str().c_str(), PB::Float2(anchorPos.x, anchorHeight - (fontHeightf * 2.0f)));
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

		if (!input->GetKey(GLFW_KEY_INSERT, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_INSERT, INPUTSTATE_PREVIOUS))
		{
			m_renderHierarchyDrawDebugDepth = m_renderHierarchyDrawDebugDepth == ~uint32_t(0) ? 0 : ~uint32_t(0);
			std::cout << "BVH: Drawing whole tree: " << (m_drawEntireRenderHierarchy ? "true" : "false") << "\n";
		}

		if (m_drawEntireRenderHierarchy)
			m_hierarchy.GetRenderHierarchy().DebugDraw(&m_camera, m_debugLinePass, m_renderHierarchyDrawDebugDepth, true);

		if (!input->GetKey(GLFW_KEY_END, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_END, INPUTSTATE_PREVIOUS))
		{
			m_drawRenderHierarchyPipelineTree = !m_drawRenderHierarchyPipelineTree;
			std::cout << "BVH: Drawing whole pipeline tree: " << (m_drawRenderHierarchyPipelineTree ? "true" : "false") << "\n";
		}

		if (m_drawRenderHierarchyPipelineTree)
			m_hierarchy.GetRenderHierarchy().DebugDrawBatchTree(&m_camera, m_debugLinePass, m_renderHierarchyDrawDebugDepth);

		for (uint32_t i = 0; i < ShadowCascadeCount; ++i)
		{
			m_shadowmapPass[i]->Update();
		}

		if (!input->GetKey(GLFW_KEY_R, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_R, INPUTSTATE_PREVIOUS) && m_selectedEntity != nullptr)
		{
			std::cout << "BVH: Rebuilding subtree experiment... \n";

			m_hierarchy.UpdateBVHTest(m_selectedEntity);
		}

		if (!input->GetKey(GLFW_KEY_DELETE, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_DELETE, INPUTSTATE_PREVIOUS) && m_selectedEntity != nullptr)
		{
			printf_s("Destroying Entity: %s\n", m_selectedEntity->GetName());

			m_hierarchy.DestroyEntity(m_selectedEntity);
			m_selectedEntity = nullptr;
		}

		if (input->GetKey(GLFW_KEY_E, INPUTSTATE_CURRENT))
		{
			m_hierarchy.GetEntityBoundingVolumeHierarchy().DebugDraw(&m_camera, m_debugLinePass, m_renderHierarchyDrawDebugDepth, true);
		}

		if (input->GetMouseButton(MOUSEBUTTON_LEFT, INPUTSTATE_PREVIOUS) && !input->GetMouseButton(MOUSEBUTTON_LEFT, INPUTSTATE_CURRENT))
		{
			glm::vec4 cursorScreenPos = glm::vec4(0.0f, 0.0f, 1.0f, 0.2f);
			cursorScreenPos.x = input->GetCursorX(INPUTSTATE_CURRENT) / m_swapchain->GetWidth();
			cursorScreenPos.y = input->GetCursorY(INPUTSTATE_CURRENT) / m_swapchain->GetHeight();

			glm::vec3 cursorFarPlanePos = m_camera.GetCursorFarPlaneWorldPosition(glm::vec2(cursorScreenPos.x, cursorScreenPos.y));
			glm::vec3 cursorNearPlanePos = m_camera.GetCursorNearPlaneWorldPosition(glm::vec2(cursorScreenPos.x, cursorScreenPos.y));
			glm::vec3 rayDirection = cursorFarPlanePos - cursorNearPlanePos;

			auto* selectedEntityData = m_hierarchy.GetEntityBoundingVolumeHierarchy().RaycastGetObjectData(m_debugLinePass, glm::vec3(cursorNearPlanePos), rayDirection);
			if (selectedEntityData)
			{
				m_selectedEntity = reinterpret_cast<const EntityBoundingVolumeHierarchy::ObjectData*>(selectedEntityData)->m_entity;
				printf_s("Selected Entity: %s\n", m_selectedEntity->GetName());
			}
			else
			{
				printf_s("No Entity Selected.\n");
			}
		}

		if (input->GetKey(GLFW_KEY_M, EInputState::INPUTSTATE_CURRENT))
		{
			m_frustrumTestCamera.Rotate(glm::radians(glm::vec3(0.0f, deltaTime * 45.0f, 0.0f)));
			//m_frustrumTestCamera.UpdateFreeCam(deltaTime, input, window);
			m_frustrumTestCamera.Update();
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

			m_mvpBuffer->EndPopulate();

			// Frustrum
			FrustrumPlanesBuffer* frustrumPlanes = (FrustrumPlanesBuffer*)m_frustrumPlanesBuffer->BeginPopulate();
			const Camera::CameraFrustrum& frustrum = m_camera.GetFrustrum();

			frustrumPlanes->m_planes[0] = frustrum.m_near;
			frustrumPlanes->m_planes[1] = frustrum.m_left;
			frustrumPlanes->m_planes[2] = frustrum.m_right;
			frustrumPlanes->m_planes[3] = frustrum.m_top;
			frustrumPlanes->m_planes[4] = frustrum.m_bottom;
			frustrumPlanes->m_planes[5] = frustrum.m_far;
			frustrumPlanes->m_camPos = glm::vec4(m_camera.Position(), 1.0f);
			frustrumPlanes->m_isOrthographic = false;

			m_frustrumPlanesBuffer->EndPopulate();

			FrustrumPlanesBuffer* testFrustrumPlanes = (FrustrumPlanesBuffer*)m_frustrumTestBuffer->BeginPopulate();

			const Camera::CameraFrustrum& testFrustrum = m_frustrumTestCamera.GetFrustrum();
			Camera::DrawFrustrum(m_debugLinePass, testFrustrum, glm::vec3(1.0f, 0.0f, 1.0f));

			testFrustrumPlanes->m_planes[0] = testFrustrum.m_near;
			testFrustrumPlanes->m_planes[1] = testFrustrum.m_left;
			testFrustrumPlanes->m_planes[2] = testFrustrum.m_right;
			testFrustrumPlanes->m_planes[3] = testFrustrum.m_top;
			testFrustrumPlanes->m_planes[4] = testFrustrum.m_bottom;
			testFrustrumPlanes->m_planes[5] = testFrustrum.m_far;
			testFrustrumPlanes->m_camPos = glm::vec4(m_frustrumTestCamera.Position(), 1.0f);
			testFrustrumPlanes->m_isOrthographic = false;

			m_frustrumTestBuffer->EndPopulate();
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

		m_assetStreamer.NextFrame();
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

		mvpBufferDesc.m_bufferSize = sizeof(FrustrumPlanesBuffer);
		mvpBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
		m_frustrumPlanesBuffer = m_renderer->AllocateBuffer(mvpBufferDesc);
		m_frustrumTestBuffer = m_renderer->AllocateBuffer(mvpBufferDesc);

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

		PB::GraphicsPipelineDesc meshShaderPipelineTest{};

		// Vertex pool
		m_vertexPool = m_allocator->Alloc<VertexPool>(m_renderer, uint32_t(sizeof(Eng::Vertex) * 1000000), uint32_t(sizeof(Eng::Vertex)));

		// Meshes & Textures
		//m_paintMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_paint", m_vertexPool);
		//m_detailsMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_details", m_vertexPool);
		//m_glassMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_glass", m_vertexPool);
		//m_planeMesh = m_allocator->Alloc<Eng::Mesh>(m_renderer, "Meshes/Primitives/plane", m_vertexPool);

		StreamableHandle paintMeshHandle
		(
			AssetEncoder::AssetHandle("Meshes/Objects/Spinner/mesh_spinner_low_paint").GetID(&Mesh::s_meshDatabaseLoader),
			Eng::EStreamableResourceType::MESH,
			Eng::StreamableHandle::EBindingType::NONE
		);

		StreamableHandle detailsMeshHandle
		(
			AssetEncoder::AssetHandle("Meshes/Objects/Spinner/mesh_spinner_low_details").GetID(&Mesh::s_meshDatabaseLoader),
			Eng::EStreamableResourceType::MESH,
			Eng::StreamableHandle::EBindingType::NONE
		);

		StreamableHandle glassMeshHandle
		(
			AssetEncoder::AssetHandle("Meshes/Objects/Spinner/mesh_spinner_low_glass").GetID(&Mesh::s_meshDatabaseLoader),
			Eng::EStreamableResourceType::MESH,
			Eng::StreamableHandle::EBindingType::NONE
		);

		StreamableHandle planeMeshHandle
		(
			AssetEncoder::AssetHandle("Meshes/Primitives/plane").GetID(&Mesh::s_meshDatabaseLoader),
			Eng::EStreamableResourceType::MESH,
			Eng::StreamableHandle::EBindingType::NONE
		);

		m_paintMesh = reinterpret_cast<Mesh*>(m_assetStreamer.GetResourceHandle(paintMeshHandle));
		m_detailsMesh = reinterpret_cast<Mesh*>(m_assetStreamer.GetResourceHandle(detailsMeshHandle));
		m_glassMesh = reinterpret_cast<Mesh*>(m_assetStreamer.GetResourceHandle(glassMeshHandle));
		m_planeMesh = reinterpret_cast<Mesh*>(m_assetStreamer.GetResourceHandle(planeMeshHandle));

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

		AssetEncoder::AssetID paintTextureIds[]
		{
			AssetEncoder::AssetHandle("Textures/Spinner/paint2048/m_spinner_paint_diffuse").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/paint2048/m_spinner_paint_normal").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/paint2048/m_spinner_paint_specular_v2").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/paint2048/m_spinner_paint_roughness").GetID(&Texture::s_textureDatabaseLoader),
			StreamableSimple::SrvBlackHandle
		};

		AssetEncoder::AssetID detailsTextureIds[]
		{
			AssetEncoder::AssetHandle("Textures/Spinner/details2048/m_spinner_details_diffuse").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/details2048/m_spinner_details_normal").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/details2048/m_spinner_details_specular").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/details2048/m_spinner_details_roughness").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/details2048/m_spinner_details_emissive").GetID(&Texture::s_textureDatabaseLoader)
		};

		AssetEncoder::AssetID glassTextureIds[]
		{
			AssetEncoder::AssetHandle("Textures/Spinner/glass2048/m_spinner_glass_diffuse").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/glass2048/m_spinner_glass_normal").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/glass2048/m_spinner_glass_specular").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/glass2048/m_spinner_glass_roughness").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Spinner/glass2048/m_spinner_glass_emissive").GetID(&Texture::s_textureDatabaseLoader)
		};

		AssetEncoder::AssetID debugTextureIds[]
		{
			AssetEncoder::AssetHandle("Textures/Debug/debug_albedo").GetID(&Texture::s_textureDatabaseLoader),
			StreamableSimple::SrvFlatNormalMapHandle,
			AssetEncoder::AssetHandle("Textures/Debug/debug_specular").GetID(&Texture::s_textureDatabaseLoader),
			AssetEncoder::AssetHandle("Textures/Debug/debug_roughness").GetID(&Texture::s_textureDatabaseLoader),
			StreamableSimple::SrvBlackHandle
		};

		PB::SamplerDesc colorSamplerDesc;
		colorSamplerDesc.m_anisotropyLevels = 1.0f;
		colorSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
		m_colorSampler = m_renderer->GetSampler(colorSamplerDesc);

		m_spinnerMaterials[0] = m_allocator->Alloc<Eng::Material>(0, paintTextureIds, _countof(paintTextureIds), m_colorSampler);
		m_spinnerMaterials[1] = m_allocator->Alloc<Eng::Material>(0, detailsTextureIds, _countof(detailsTextureIds), m_colorSampler);
		m_spinnerMaterials[2] = m_allocator->Alloc<Eng::Material>(0, glassTextureIds, _countof(glassTextureIds), m_colorSampler);
		m_planeMaterial = m_allocator->Alloc<Eng::Material>(0, debugTextureIds, _countof(debugTextureIds), m_colorSampler);
		m_debugMaterial = m_allocator->Alloc<Eng::Material>(0, debugTextureIds, _countof(debugTextureIds), m_colorSampler);
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

		m_allocator->Free(m_hdrSkyTexture);
		m_allocator->Free(m_skyIrradianceMap);
		m_allocator->Free(m_skyPrefilterMap);

		m_allocator->Free(m_shadowVertShader);

		m_allocator->Free(m_vertexPool);

		m_renderer->FreeBuffer(m_frustrumTestBuffer);
		m_renderer->FreeBuffer(m_frustrumPlanesBuffer);
		m_renderer->FreeBuffer(m_mvpBuffer);
		m_mvpBuffer = nullptr;
	}

	inline RenderGraph* ClientPlayground::CreateRenderGraph()
	{
		PB::IBufferObject* viewBuffer = m_frustrumTestBuffer;

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
						m_shadowmapPass[i]->SetCamera(&m_camera, &m_hierarchy.GetRenderHierarchy(), sunDir);
					}
					m_shadowmapPass[i]->AddToRenderGraph(&rgBuilder, ShadowmapResolution);
				}
			}

			// GBuffer pass
			{
				if (!m_gBufferPass)
					m_gBufferPass = m_allocator->Alloc<GBufferPass>(m_renderer, m_allocator, m_mvpBuffer->GetViewAsUniformBuffer(), viewBuffer->GetViewAsUniformBuffer(), &m_camera, &m_hierarchy.GetRenderHierarchy());
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
			m_selectedEntityText = m_textPass->AddText("Selected Entity: None", m_fontTexture, PB::Float2(0.0f, 2.0f * float(m_fontTexture->GetFontHeight())));
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

		AssetEncoder::AssetID paintMeshID = AssetEncoder::AssetHandle("Meshes/Objects/Spinner/mesh_spinner_low_paint").GetID(&Mesh::s_meshDatabaseLoader);
		AssetEncoder::AssetID detailsMeshID = AssetEncoder::AssetHandle("Meshes/Objects/Spinner/mesh_spinner_low_details").GetID(&Mesh::s_meshDatabaseLoader);
		AssetEncoder::AssetID glassMeshID = AssetEncoder::AssetHandle("Meshes/Objects/Spinner/mesh_spinner_low_glass").GetID(&Mesh::s_meshDatabaseLoader);
		AssetEncoder::AssetID planeMeshID = AssetEncoder::AssetHandle("Meshes/Primitives/plane").GetID(&Mesh::s_meshDatabaseLoader);
		AssetEncoder::AssetID bunnyMeshID = AssetEncoder::AssetHandle("Meshes/Objects/Stanford/Bunny").GetID(&Mesh::s_meshDatabaseLoader);

		AssetEncoder::AssetID deathMeshID = AssetEncoder::AssetHandle("Meshes/Primitives/12mILL").GetID(&Mesh::s_meshDatabaseLoader);

		const uint32_t spinnerCount = 2;
		for (uint32_t i = 0; i < spinnerCount; ++i)
		{
			glm::vec3 pos = glm::vec3(8.0f * (i / 10), 0.0f, -7.0f * (i % 10));
			//glm::vec3 pos = glm::vec3(0.0f, 0.0f, -3.0f + (i * 6.0f));
		
			if (i == 1)
			{
				pos.y = 5.0f;
				pos.z += 2.5f;
			}
		
			glm::quat spinnerQuat = glm::rotate(glm::identity<glm::quat>(), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		
			Entity* paintEntity = m_hierarchy.AddEntity(pos, "spinner_paint");
			Transform* paintTransform = paintEntity->GetComponent<Transform>();
			paintTransform->SetPosition(pos);
			paintTransform->SetRotation(spinnerQuat);
			paintEntity->AddComponent<RenderDefinition>(paintMeshID, m_spinnerMaterials[0]);
			m_hierarchy.CommitEntity(paintEntity);
		
			Entity* detailsEntity = m_hierarchy.AddEntity(pos, "spinner_details");
			Transform* detailsTransform = detailsEntity->GetComponent<Transform>();
			detailsTransform->SetPosition(pos);
			detailsTransform->SetRotation(spinnerQuat);
			detailsEntity->AddComponent<RenderDefinition>(detailsMeshID, m_spinnerMaterials[1]);
			m_hierarchy.CommitEntity(detailsEntity);
		
			Entity* glassEntity = m_hierarchy.AddEntity(pos, "spinner_glass");
			Transform* glassTransform = glassEntity->GetComponent<Transform>();
			glassTransform->SetPosition(pos);
			glassTransform->SetRotation(spinnerQuat);
			glassEntity->AddComponent<RenderDefinition>(glassMeshID, m_spinnerMaterials[2]);
			m_hierarchy.CommitEntity(glassEntity);
		}
		
		modelMat = glm::identity<glm::mat4>();
		glm::vec3 planeOffset = glm::vec3(0.0f, -0.2f, 0.0f);
		modelMat = glm::translate(glm::mat4(), planeOffset);
		
		Entity* planeEntity = m_hierarchy.AddEntity(planeOffset, "plane");
		Transform* planeTransform = planeEntity->GetComponent<Transform>();
		planeTransform->SetPosition(planeOffset);
		planeEntity->AddComponent<RenderDefinition>(planeMeshID, m_planeMaterial);
		m_hierarchy.CommitEntity(planeEntity);

		glm::vec3 thescythe = glm::vec3(4.0f, 2.0f, -4.0f);
		
		Entity* deathEntity = m_hierarchy.AddEntity(thescythe, "death");
		Transform* whyamIdoingthis = deathEntity->GetComponent<Transform>();
		whyamIdoingthis->SetPosition(thescythe);
		deathEntity->AddComponent<RenderDefinition>(deathMeshID, m_planeMaterial);
		m_hierarchy.CommitEntity(deathEntity); // Sweet jeebus save me...

		
		glm::vec3 debugPlaneOffset = glm::vec3(-2.4f, 1.0f, 0.0f);
		
		Entity* debugPlaneEntity = m_hierarchy.AddEntity(debugPlaneOffset, "debug_plane");
		Transform* debugPlaneTransform = debugPlaneEntity->GetComponent<Transform>();
		debugPlaneTransform->SetPosition(debugPlaneOffset);
		debugPlaneTransform->RotateEulerZ(-45.0f);
		debugPlaneTransform->SetScale(glm::vec3(0.2f));
		debugPlaneEntity->AddComponent<RenderDefinition>(planeMeshID, m_debugMaterial);
		m_hierarchy.CommitEntity(debugPlaneEntity);
		
		glm::vec3 bunnyOffset = glm::vec3(3.0f, 0.5f, 0.0f);
		Entity* bunnyEntity = m_hierarchy.AddEntity(bunnyOffset, "bunny");
		Transform* bunnyTransform = bunnyEntity->GetComponent<Transform>();
		bunnyTransform->SetPosition(bunnyOffset);
		bunnyTransform->SetScale(glm::vec3(0.3f));
		bunnyEntity->AddComponent<RenderDefinition>(bunnyMeshID, m_debugMaterial);
		m_hierarchy.CommitEntity(bunnyEntity);

		m_hierarchy.BakeTrees();
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