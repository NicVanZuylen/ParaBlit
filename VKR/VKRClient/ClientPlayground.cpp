#include "ClientPlayground.h"

#include "GLFW/glfw3.h"

#include "CLib/Allocator.h"

#include "RenderGraph.h"

#include "ShadowMapPass.h"
#include "GBufferPass.h"
#include "ShadowAccumPass.h"
#include "ShadowBlurPass.h"
#include "DeferredLightingPass.h"
#include "AmbientOcclusionPass.h"
#include "AOBlurPass.h"
#include "BloomExtractionPass.h"
#include "BloomBlurPass.h"
#include "DebugLinePass.h"
#include "TextRenderPass.h"

#include "Input.h"

#include "FontTexture.h"
#include "Mesh.h"
#include "Shader.h"
#include "ObjectDispatcher.h"
#include "DrawBatch.h"

#include "RenderBoundingVolumeHierarchy.h"

#include "glm/gtc/type_ptr.hpp"

#include <sstream>
#include <iostream>
#include <random>

ClientPlayground::ClientPlayground(PB::IRenderer* renderer, CLib::Allocator* allocator)
{
	m_renderer = renderer;
	m_swapchain = m_renderer->GetSwapchain();
	m_allocator = allocator;

	glm::vec3 sunDir(1.0f, 1.0f, 0.0f);
	//m_camera = Camera(glm::vec3(-2.0f, 0.5f, -6.0f), glm::vec3(glm::radians(-45.0f), glm::radians(-45.0f) , 0.0f), 0.5f, 5.0f);

	Camera::CreateDesc cameraDesc;
	cameraDesc.m_position = sunDir * 5.0f;
	cameraDesc.m_eulerAngles = glm::radians(glm::vec3(-45.0f, 45.0f, 0.0f));
	cameraDesc.m_sensitivity = 0.5f;
	cameraDesc.m_moveSpeed = 20.0f;
	cameraDesc.m_width = m_swapchain->GetWidth();
	cameraDesc.m_height = m_swapchain->GetHeight();
	cameraDesc.m_fovY = glm::radians(45.0f);
	cameraDesc.m_zFar = 100.0f;
	m_camera = Camera(cameraDesc);

	//cameraDesc.m_position = glm::vec3(-5.0f, 5.0f, -5.0f);
	//cameraDesc.m_position = glm::vec3(-35.0f, 7.5f, -35.0f);
	cameraDesc.m_position = glm::vec3(-21.0f, 7.5f, -21.0f);
	cameraDesc.m_eulerAngles = glm::vec3(0.0f);
	m_frustrumTestCamera = Camera(cameraDesc);

	InitResources();

	m_geoShadowDispatchList = m_allocator->Alloc<ObjectDispatchList>();
	m_geoShadowDispatchList->Init(m_renderer, m_allocator, { 0, 0, 0, 0 });

	m_geoDispatchList = m_allocator->Alloc<ObjectDispatchList>();
	m_geoDispatchList->Init(m_renderer, m_allocator, { 0, 0, m_swapchain->GetWidth(), m_swapchain->GetHeight() });

	uint32_t indexCount = ((m_paintMesh->IndexCount() + m_detailsMesh->IndexCount() + m_glassMesh->IndexCount()) * (255 / 3)) + m_planeMesh->IndexCount();
	m_drawBatch = m_allocator->Alloc<DrawBatch>(m_renderer, m_allocator, m_vertexPool, indexCount);
	m_renderGraph = CreateRenderGraph();
	SetupDrawBatch();

	PB::CommandContextDesc contextDesc{};
	contextDesc.m_renderer = m_renderer;
	contextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;

	PB::SCommandContext initCmdContext(m_renderer);
	initCmdContext->Init(contextDesc);
	initCmdContext->Begin();

	m_drawBatch->UpdateIndices(initCmdContext.GetContext());

	initCmdContext->End();
	initCmdContext->Return();

	RenderBoundingVolumeHierarchy::CreateDesc rbvhDesc{};
	rbvhDesc.m_desiredMaxDepth = 50;

	rbvhDesc.m_toleranceDistanceX = 0.05f;
	rbvhDesc.m_toleranceDistanceY = 0.05f;

	rbvhDesc.m_toleranceStepX = 0.05f;
	rbvhDesc.m_toleranceStepZ = 0.05f;

	rbvhDesc.m_toleranceDistanceY = 0.2f;
	rbvhDesc.m_toleranceStepY = 0.2f;

	//rbvhDesc.m_camera = &m_camera;
	rbvhDesc.m_camera = &m_frustrumTestCamera;

	m_renderHierarchy = m_allocator->Alloc<RenderBoundingVolumeHierarchy>(rbvhDesc);

	// --------------------------------------------------------------------------------------
	// Cluster Test
	/*{
		CLib::Vector<std::pair<glm::vec3, glm::vec3>> nodes;

		nodes.PushBack
		({
			glm::vec3(5.0f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(5.0f, 5.0f, 6.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(5.0f, 7.0f, 5.0f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(5.0f, 7.0f, 6.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(7.0f, 5.0f, 5.0f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(7.0f, 5.0f, 6.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 5.0f, 5.0f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 5.0f, 6.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.0f, 5.0f, 1.0f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(5.0f, 5.0f, 1.0f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 6.5f, 5.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 5.0f, 9.4f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 6.5f, 9.4f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 5.0f, 10.6f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(10.5f, 6.5f, 10.6f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(5.5f, 5.0f, 10.6f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(5.5f, 6.5f, 10.6f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(7.5f, 5.0f, 8.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(7.5f, 6.5f, 8.5f),
			glm::vec3(1.0f)
		});

		nodes.PushBack
		({
			glm::vec3(4.0f, 5.0f, 4.0f),
			glm::vec3(10.0f)
		});

		nodes.PushBack
		({
			glm::vec3(0.0f),
			glm::vec3(50.0f)
		});

		m_renderHierarchy->BuildBottomUp(nodes);
	}*/
	// --------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------
	// Merge test

	/*m_renderHierarchy->InsertNode(glm::vec3(-3.0f, 0.0f, -3.0f), glm::vec3(3.0f));
	m_renderHierarchy->InsertNode(glm::vec3(-5.0f, 1.0f, -4.0f), glm::vec3(3.0f));

	m_renderHierarchy->InsertNode(glm::vec3(3.0f, 1.0f, 6.0f), glm::vec3(3.0f));
	m_renderHierarchy->InsertNode(glm::vec3(4.5f, 2.5f, 7.5f), glm::vec3(3.0f));
	m_renderHierarchy->InsertNode(glm::vec3(5.0f, 3.0f, 8.0f), glm::vec3(1.0f));
	m_renderHierarchy->InsertNode(glm::vec3(7.0f, 5.0f, 9.0f), glm::vec3(1.0f));
	m_renderHierarchy->InsertNode(glm::vec3(3.0f, 1.0f, 6.0f), glm::vec3(1.0f));

	m_renderHierarchy->InsertNode(glm::vec3(5.5f, 3.5f, 8.5f), glm::vec3(1.0f, 15.0f, 1.0f));
	m_renderHierarchy->InsertNode(glm::vec3(6.5f, 3.5f, 8.5f), glm::vec3(1.0f));

	m_renderHierarchy->InsertNode(glm::vec3(7.5f, 5.0f, 8.5f), glm::vec3(0.1f));
	m_renderHierarchy->InsertNode(glm::vec3(7.5f, 6.0f, 9.0f), glm::vec3(0.1f));
	m_renderHierarchy->InsertNode(glm::vec3(8.5f, 5.0f, 6.0f), glm::vec3(0.1f));*/

	// --------------------------------------------------------------------------------------
	// Force Merge Test

	//m_renderHierarchy->InsertNode(glm::vec3(-2.0f, 0.0f, -5.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-5.0f, 0.0f, -5.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-2.0f, 0.0f, -2.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-10.0f, 0.0f, -4.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-10.0f, 0.0f, -2.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-10.0f, 0.0f, -0.0f), glm::vec3(1.0f));
	//
	//m_renderHierarchy->InsertNode(glm::vec3(-10.0f, 0.0f, -10.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-8.0f, 0.0f, -10.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-6.0f, 0.0f, -10.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-4.0f, 0.0f, -10.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-2.0f, 0.0f, -10.0f), glm::vec3(1.0f));
	//m_renderHierarchy->InsertNode(glm::vec3(-0.0f, 0.0f, -10.0f), glm::vec3(1.0f));


	// --------------------------------------------------------------------------------------
	// Random node layout

	const uint32_t nodeCount = 20;

	const glm::vec3 minExtents(1.0f);
	const glm::vec3 maxExtents = glm::vec3(2.0f) - minExtents;

	const glm::vec3 minOrigin(-20.0f, 0.0f, -20.0f);
	const glm::vec3 maxOrigin(20.0f, 0.0f, 20.0f);

	std::default_random_engine rand{};
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	CLib::Vector<std::pair<glm::vec3, glm::vec3>> nodes;
	//nodes.PushBack({ glm::vec3(-30.0f), glm::vec3(60.0f) });
	//for (uint32_t level = 0; level < 5; ++level)
	//{
	//	for (uint32_t i = 0; i < nodeCount; ++i)
	//	{
	//		glm::vec3 nodeExtents
	//		{
	//			minExtents.x + (dist(rand) * maxExtents.x),
	//			minExtents.y + (dist(rand) * maxExtents.y),
	//			minExtents.z + (dist(rand) * maxExtents.z)
	//		};

	//		glm::vec3 nodeOrigin
	//		{
	//			minOrigin.x + ((maxOrigin.x - minOrigin.x) * dist(rand)),
	//			//minOrigin.y + ((maxOrigin.y - minOrigin.y) * dist(rand)),
	//			level * 3.0f,
	//			minOrigin.z + ((maxOrigin.z - minOrigin.z) * dist(rand))
	//		};

	//		nodes.PushBack({ nodeOrigin, nodeExtents });
	//	}
	//}

	//nodes.PushBack({ glm::vec3(-1.2f, 0.0f, -2.5f), glm::vec3(2.4f, 1.5f, 5.0f) });
	nodes.PushBack({ glm::vec3(-0.6f, 0.0f, -1.25f), glm::vec3(1.2f, 0.75f, 2.5f) });

	//nodes.PushBack({ glm::vec3(-2.5f), glm::vec3(2.5f) });
	//nodes.PushBack({ glm::vec3(-7.5f, -2.5f, -7.5f), glm::vec3(2.5f) });

	m_renderHierarchy->BuildBottomUp(nodes);

	// --------------------------------------------------------------------------------------
}

ClientPlayground::~ClientPlayground()
{
	m_allocator->Free(m_renderHierarchy);

	m_allocator->Free(m_drawBatch);

	m_allocator->Free(m_geoDispatchList);
	m_allocator->Free(m_geoShadowDispatchList);

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
	m_allocator->Free(m_shadowmapPass);

	m_allocator->Free(m_fontTexture);
}

void ClientPlayground::Update(GLFWwindow* window, Input* input, float deltaTime, float elapsedTime, float stallTime, bool updateMetrics)
{
	m_camera.UpdateFreeCam(deltaTime, input, window);
	m_frustrumTestCamera.Rotate(glm::vec3(0.0f, glm::radians(15.0f * deltaTime), 0.0f));
	//m_frustrumTestCamera.SetRotation(glm::vec3(0.0f, glm::radians(120.0f), 0.0f));
	m_frustrumTestCamera.Update();

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

	//if (!input->GetKey(GLFW_KEY_I, INPUTSTATE_CURRENT) && input->GetKey(GLFW_KEY_I, INPUTSTATE_PREVIOUS))
	//{
	//	const glm::vec3 minExtents(0.1f);
	//	const glm::vec3 maxExtents = glm::vec3(1.0f) - minExtents;

	//	const glm::vec3 minOrigin(-5.0f);
	//	const glm::vec3 maxOrigin(5.0f);

	//	std::default_random_engine rand(uint64_t(elapsedTime * 1000.0f));
	//	std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	//	glm::vec3 nodeExtents
	//	{
	//		minExtents.x + (dist(rand) * maxExtents.x),
	//		minExtents.y + (dist(rand) * maxExtents.y),
	//		minExtents.z + (dist(rand) * maxExtents.z)
	//	};

	//	glm::vec3 nodeOrigin
	//	{
	//		minOrigin.x + ((maxOrigin.x - minOrigin.x) * dist(rand)),
	//		minOrigin.y + ((maxOrigin.y - minOrigin.y) * dist(rand)),
	//		minOrigin.z + ((maxOrigin.z - minOrigin.z) * dist(rand))
	//	};

	//	m_renderHierarchy->InsertNode(nodeOrigin, nodeExtents);
	//}

	m_renderHierarchy->DebugDraw(m_debugLinePass, m_drawEntireRenderHierarchy ? ~uint32_t(0) : m_renderHierarchyDrawDebugDepth, true);

	// Update Camera -------------------------------------------------------------------------------------------------
	{
		constexpr float fov = 45.0f;
		constexpr float fovRadians = glm::radians(fov);

		ViewConstantsBuffer* bufferMatrices = (ViewConstantsBuffer*)m_mvpBuffer->BeginPopulate();

		// Model
		glm::mat4& model = bufferMatrices->m_model;
		model = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.1f * elapsedTime, 0.0f));
		model = glm::scale(model, glm::vec3(0.01f));
		model = glm::rotate<float>(model, elapsedTime * 0.2f, glm::vec3(0.0f, 1.0f, 0.0f));

		// View
		bufferMatrices->m_view = m_camera.GetViewMatrix(); // View
		bufferMatrices->m_invView = glm::inverse(bufferMatrices->m_view);

		// Projection
		bufferMatrices->m_proj = m_camera.GetProjectionMatrix();
		bufferMatrices->m_invProj = glm::inverse(bufferMatrices->m_proj);

		// Position
		bufferMatrices->m_camPos = glm::vec4(m_camera.Position(), 1.0f);

		// Depth Reconstruction Constants
		bufferMatrices->m_aspectRatio = float(m_swapchain->GetWidth()) / m_swapchain->GetHeight();
		bufferMatrices->m_tanHalfFOV = glm::tan(fovRadians / 2);

		// Frustrum
		const Camera::CameraFrustrum& frustrum = m_frustrumTestCamera.GetFrustrum();

		bufferMatrices->m_mainFrustrumPlanes[0] = frustrum.m_near;
		bufferMatrices->m_mainFrustrumPlanes[1] = frustrum.m_left;
		bufferMatrices->m_mainFrustrumPlanes[2] = frustrum.m_right;
		bufferMatrices->m_mainFrustrumPlanes[3] = frustrum.m_top;
		bufferMatrices->m_mainFrustrumPlanes[4] = frustrum.m_bottom;
		bufferMatrices->m_mainFrustrumPlanes[5] = frustrum.m_far;

		glm::mat4 modelCpy = model;

		m_mvpBuffer->EndPopulate();

		//m_drawBatch->UpdateInstanceModelMatrix(m_firstInstanceHandles[0], glm::value_ptr(modelCpy));
		//m_drawBatch->UpdateInstanceModelMatrix(m_firstInstanceHandles[1], glm::value_ptr(modelCpy));
		//m_drawBatch->UpdateInstanceModelMatrix(m_firstInstanceHandles[2], glm::value_ptr(modelCpy));

		//m_drawBatch->FinalizeUpdates();
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

	m_geoShadowDispatchList->FlushCommandLists(); // Force command lists to be re-recorded.
	m_geoDispatchList->UpdateRenderArea({ 0, 0, width, height }); // Will also flush command lists.

	PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
	auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);
	m_textPass->SetOutputTexture(swapChainTex);

	m_camera.SetWidth(width);
	m_camera.SetHeight(height);

	m_frustrumTestCamera.SetWidth(width);
	m_frustrumTestCamera.SetHeight(height);
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

	// Vertex pool
	m_vertexPool = m_allocator->Alloc<VertexPool>(m_renderer, uint32_t(sizeof(PBClient::Vertex) * 1000000), uint32_t(sizeof(PBClient::Vertex)));

	// Shaders
	m_shadowVertShader = m_allocator->Alloc<PBClient::Shader>(m_renderer, "Shaders/GLSL/vs_obj_shad_batch", m_allocator, true);
	m_vertShader = m_allocator->Alloc<PBClient::Shader>(m_renderer, "Shaders/GLSL/vs_obj_def_batch", m_allocator, true);
	m_fragShader = m_allocator->Alloc<PBClient::Shader>(m_renderer, "Shaders/GLSL/fs_obj_def_batch", m_allocator, true);

	// Meshes & Textures
	m_paintMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_paint", m_vertexPool);
	m_detailsMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_details", m_vertexPool);
	m_glassMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_glass", m_vertexPool);
	m_planeMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Primitives/plane", m_vertexPool);

	m_paintTextures[0] = m_allocator->Alloc<PBClient::Texture> (m_renderer, m_allocator, "../Assets/Textures/Spinner/paint2048/m_spinner_paint_diffuse.tga");
	m_detailsTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/details2048/m_spinner_details_diffuse.tga");
	m_glassTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/glass2048/m_spinner_glass_diffuse.tga");

	m_paintTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/paint2048/m_spinner_paint_normal.tga", false);
	m_detailsTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/details2048/m_spinner_details_normal.tga", false);
	m_glassTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/glass2048/m_spinner_glass_normal.tga", false);

	m_paintTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/paint2048/m_spinner_paint_specular_v2.tga");
	m_detailsTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/details2048/m_spinner_details_specular.tga");
	m_glassTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/glass2048/m_spinner_glass_specular.tga");

	m_paintTextures[3] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/paint2048/m_spinner_paint_roughness.tga", false);
	m_detailsTextures[3] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/details2048/m_spinner_details_roughness.tga", false);
	m_glassTextures[3] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/glass2048/m_spinner_glass_roughness.tga", false);

	m_detailsTextures[4] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/details2048/m_spinner_details_emissive.tga");
	m_glassTextures[4] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Spinner/glass2048/m_spinner_glass_emissive.tga");

	m_metalTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Metal/diffuse.tga");
	m_metalTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Metal/normal.tga", false);

	m_debugTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Debug/debug_albedo.tga");
	m_debugTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Debug/debug_roughness.tga", false);
	m_debugTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Debug/debug_specular.tga");

	for (int i = 0; i < _countof(m_paintTextures); ++i)
		m_paintViews[i] = m_paintTextures[i]->GetTexture()->GetDefaultSRV();
	m_paintViews[4] = m_solidBlackTexture->GetDefaultSRV();

	for (int i = 0; i < _countof(m_detailsTextures); ++i)
		m_detailsViews[i] = m_detailsTextures[i]->GetTexture()->GetDefaultSRV();

	for (int i = 0; i < _countof(m_glassTextures); ++i)
		m_glassViews[i] = m_glassTextures[i]->GetTexture()->GetDefaultSRV();
	//m_glassViews[3] = m_solidWhiteTexture->GetDefaultSRV();
	//m_glassViews[2] = m_solidWhiteTexture->GetDefaultSRV();

	for (int i = 0; i < _countof(m_debugTextures); ++i)
		m_debugViews[i] = m_debugTextures[i]->GetTexture()->GetDefaultSRV();

	//m_paintViews[0] = m_solidWhiteTexture->GetDefaultSRV();
	//m_detailsViews[0] = m_solidWhiteTexture->GetDefaultSRV();
	//m_glassViews[0] = m_solidWhiteTexture->GetDefaultSRV();

	const char* skyboxFilenames[6]
	{
		"../Assets/Textures/Skybox/right.jpg",
		"../Assets/Textures/Skybox/left.jpg",
		"../Assets/Textures/Skybox/top.jpg",
		"../Assets/Textures/Skybox/bottom.jpg",
		"../Assets/Textures/Skybox/front.jpg",
		"../Assets/Textures/Skybox/back.jpg"
	};

	m_hdrSkyTexture = m_allocator->Alloc<PBClient::Texture>(m_renderer, m_allocator, "../Assets/Textures/Sky/Arches_E_PineTree_3k.hdr", true, true);

	m_fontTexture = m_allocator->Alloc<PBClient::FontTexture>(m_renderer, "../Assets/Fonts/arial.ttf", 32);

	PB::SamplerDesc colorSamplerDesc;
	colorSamplerDesc.m_anisotropyLevels = 1.0f;
	colorSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
	m_colorSampler = m_renderer->GetSampler(colorSamplerDesc);
}

void ClientPlayground::DestroyResources()
{
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

	m_allocator->Free(m_paintMesh);
	m_allocator->Free(m_detailsMesh);
	m_allocator->Free(m_glassMesh);
	m_allocator->Free(m_planeMesh);

	m_allocator->Free(m_shadowVertShader);
	m_allocator->Free(m_vertShader);
	m_allocator->Free(m_fragShader);

	m_allocator->Free(m_vertexPool);

	m_renderer->FreeBuffer(m_mvpBuffer);
	m_mvpBuffer = nullptr;
}

inline RenderGraph* ClientPlayground::CreateRenderGraph()
{
	RenderGraph* output = nullptr;
	{
		RenderGraphBuilder rgBuilder(m_renderer, m_allocator);

		// Shadowmap Pass
		{
			if(!m_shadowmapPass)
				m_shadowmapPass = m_allocator->Alloc<ShadowMapPass>(m_renderer, m_allocator);
			m_shadowmapPass->AddToRenderGraph(&rgBuilder, ShadowmapResolution);
		}

		// GBuffer pass
		{
			if(!m_gBufferPass)
				m_gBufferPass = m_allocator->Alloc<GBufferPass>(m_renderer, m_allocator, m_mvpBuffer->GetViewAsUniformBuffer(), m_drawBatch);
			m_gBufferPass->AddToRenderGraph(&rgBuilder);
		}
		
		// Shadow Accumulation Pass
		{
			if(!m_shadowAccumPass)
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
			if(!m_deferredLightingPass)
				m_deferredLightingPass = m_allocator->Alloc<DeferredLightingPass>(m_renderer, m_allocator);
			m_deferredLightingPass->AddToRenderGraph(&rgBuilder);
		}

		// Bloom Extraction Pass
		{
			if(!m_bloomExtractionPass)
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

	if(!m_cpuTimeText)
	{
		m_cpuTimeText = m_textPass->AddText("CPU Time: 000000ms", m_fontTexture, PB::Float2(0.0f, 0.0f));
		m_fpsText = m_textPass->AddText("FPS: 000000", m_fontTexture, PB::Float2(0.0f, float(m_fontTexture->GetFontHeight())));
	}

	m_shadowmapPass->SetDispatchList(m_geoShadowDispatchList, true);
	m_gBufferPass->SetDispatchList(m_geoDispatchList, true);

	// Set up lighting
	{
		glm::vec3 sunDir(1.0f, 1.0f, 0.0f);

		//Camera shadowCam(glm::vec3(0.0f, 0.0f, -4.0f) + (sunDir * 50.0f), glm::radians(glm::vec3(-45.0f, 45.0f, 0.0f)));
		Camera::CreateDesc shadowCamDesc{};
		shadowCamDesc.m_position = glm::vec3(0.0f, 0.0f, 0.0f);
		shadowCamDesc.m_eulerAngles = glm::radians(glm::vec3(0.0f, 0.0f, 0.0f));
		Camera shadowCam(shadowCamDesc);

		glm::mat4 shadowView = shadowCam.GetViewMatrix();

		constexpr const float ShadowDistance = 8.0f;

		m_shadowmapPass->SetViewMatrix(glm::value_ptr(shadowView));
		m_shadowmapPass->SetShadowParameters(ShadowDistance, 0.5f, 0.2f, ShadowmapResolution);

		m_deferredLightingPass->SetMVPBuffer(m_mvpBuffer);

		m_shadowAccumPass->SetMVPBuffer(m_mvpBuffer);
		m_shadowAccumPass->SetSVBBuffer(m_shadowmapPass->GetSVBView());

		glm::vec3 sunColor = glm::vec3(2.4f);

		m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { sunColor.r, sunColor.g, sunColor.b, 1.0f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 5.0f, 5.0f, 5.0f, 1.0f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 0.3f, 0.3f, 0.4f, 1.0f });
		//m_deferredLightingPass->SetPointLight(0, { -1.0f, 1.0f, -4.0f, 1.0f }, { 0.0f, 3.0f, 3.0f }, 5.0f);
		//m_deferredLightingPass->SetPointLight(1, { 1.0f, 1.0f, -4.0f, 1.0f }, { 3.0f, 0.0f, 3.0f }, 5.0f);

		m_deferredLightingPass->SetSkyboxTexture(m_hdrSkyTexture, true, 1);
	}

	return output;
}

void ClientPlayground::SetupDrawBatch()
{
	PB::UniformBufferView mvpView = m_mvpBuffer->GetViewAsUniformBuffer();

	//m_drawBatch->AddToDispatchList(m_geoShadowDispatchList, GetShadowDrawBatchPipeline(), m_shadowmapPass->GetDrawBatchBindings());
	//m_drawBatch->AddToDispatchList(m_geoDispatchList, GetGBufferDrawBatchPipeline(), GetGBufferDrawBatchBindings(mvpView));

	glm::mat4 modelMat = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -4.0f));
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

	glm::vec3 boundOrigin0 = glm::vec3(-1.2f, 0.0f, -2.5f);
	glm::vec3 boundExtent0 = glm::vec3(2.4f, 1.5f, 5.0f);

	glm::vec3 boundOrigin1 = glm::vec3(-0.6f, 0.0f, -1.25f);
	glm::vec3 boundExtent1 = glm::vec3(1.2f, 0.75f, 2.5f);

	const uint32_t spinnerCount = 85;
	for (uint32_t i = 0; i < spinnerCount; ++i)
	{
		glm::vec3 pos = glm::vec3(4.0f * (i / 10), 0.0f, -7.0f * (i % 10));
		//glm::vec3 pos = glm::vec3(4.0f * i, 0.0f, 0.0f);
		modelMat = glm::translate(glm::mat4(), pos);
		modelMat = glm::rotate(modelMat, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		spinnerModelMat = glm::scale(modelMat, glm::vec3(0.01f)); // Convert cm to m.

		if (i == 0)
		{
			m_firstInstanceHandles[0] = m_drawBatch->AddInstance(m_paintMesh, glm::value_ptr(spinnerModelMat), boundOrigin0, boundExtent0, m_paintViews, _countof(m_paintViews), m_colorSampler);
			m_firstInstanceHandles[1] = m_drawBatch->AddInstance(m_detailsMesh, glm::value_ptr(spinnerModelMat), boundOrigin0, boundExtent0, m_detailsViews, _countof(m_detailsViews), m_colorSampler);
			m_firstInstanceHandles[2] = m_drawBatch->AddInstance(m_glassMesh, glm::value_ptr(spinnerModelMat), boundOrigin0, boundExtent0, m_glassViews, _countof(m_glassViews), m_colorSampler);
			continue;
		}

		m_drawBatch->AddInstance(m_paintMesh, glm::value_ptr(spinnerModelMat), pos + boundOrigin0, boundExtent0, m_paintViews, _countof(m_paintViews), m_colorSampler);
		m_drawBatch->AddInstance(m_detailsMesh, glm::value_ptr(spinnerModelMat), pos + boundOrigin0, boundExtent0, m_detailsViews, _countof(m_detailsViews), m_colorSampler);
		m_drawBatch->AddInstance(m_glassMesh, glm::value_ptr(spinnerModelMat), pos + boundOrigin0, boundExtent0, m_glassViews, _countof(m_glassViews), m_colorSampler);
	}

	/*modelMat = glm::translate(glm::mat4(), glm::vec3(0.0f, -0.2f, -4.0f));
	m_drawBatch->AddInstance(m_planeMesh, glm::value_ptr(modelMat), boundOrigin, boundExtent, plainViews, _countof(plainViews), m_colorSampler);

	plainViews[0] = m_debugViews[0];
	plainViews[2] = m_debugViews[2];
	plainViews[3] = m_debugViews[1];

	modelMat = glm::identity<glm::mat4>();
	modelMat = glm::translate(modelMat, glm::vec3(-2.4f, 1.0f, -4.0f));
	modelMat = glm::rotate(modelMat, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	modelMat = glm::scale(modelMat, glm::vec3(0.2f, 0.2f, 0.2f));
	m_drawBatch->AddInstance(m_planeMesh, glm::value_ptr(modelMat), boundOrigin, boundExtent, plainViews, _countof(plainViews), m_colorSampler);*/


	/*
	modelMat = glm::identity<glm::mat4>();
	modelMat = glm::scale(modelMat, glm::vec3(0.01f));
	m_drawBatch->AddInstance(m_paintMesh, glm::value_ptr(modelMat), boundOrigin, boundExtent, m_paintViews, _countof(m_paintViews), m_colorSampler);
	m_drawBatch->AddInstance(m_detailsMesh, glm::value_ptr(modelMat), boundOrigin, boundExtent, m_detailsViews, _countof(m_detailsViews), m_colorSampler);
	m_drawBatch->AddInstance(m_glassMesh, glm::value_ptr(modelMat), boundOrigin, boundExtent, m_glassViews, _countof(m_glassViews), m_colorSampler);*/

	m_drawBatch->UpdateCullParams();
}

PB::Pipeline ClientPlayground::GetGBufferDrawBatchPipeline()
{
	PB::GraphicsPipelineDesc pipelineDesc = m_gBufferPass->GetBasePipelineDesc();
	pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_vertShader->GetModule();
	pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_fragShader->GetModule();

	return m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
}

PB::Pipeline ClientPlayground::GetShadowDrawBatchPipeline()
{
	PB::GraphicsPipelineDesc pipelineDesc = m_shadowmapPass->GetBasePipelineDesc(ShadowmapResolution);
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
