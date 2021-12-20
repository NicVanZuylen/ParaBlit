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

#include "Input.h"

#include "FontTexture.h"
#include "Mesh.h"
#include "Shader.h"
#include "ObjectDispatcher.h"
#include "DrawBatch.h"

#include "glm/gtc/type_ptr.hpp"

ClientPlayground::ClientPlayground(PB::IRenderer* renderer, CLib::Allocator* allocator)
{
	m_renderer = renderer;
	m_swapchain = m_renderer->GetSwapchain();
	m_allocator = allocator;

	glm::vec3 sunDir(1.0f, 1.0f, 0.0f);
	//m_camera = Camera(glm::vec3(-2.0f, 0.5f, -6.0f), glm::vec3(glm::radians(-45.0f), glm::radians(-45.0f) , 0.0f), 0.5f, 5.0f);
	m_camera = Camera(sunDir * 5.0f, glm::radians(glm::vec3(-45.0f, 45.0f, 0.0f)), 0.5f, 5.0f);
	InitResources();

	m_geoShadowDispatchList = m_allocator->Alloc<ObjectDispatchList>();
	m_geoShadowDispatchList->Init(m_renderer, m_allocator, { 0, 0, 0, 0 });

	m_geoDispatchList = m_allocator->Alloc<ObjectDispatchList>();
	m_geoDispatchList->Init(m_renderer, m_allocator, { 0, 0, m_swapchain->GetWidth(), m_swapchain->GetHeight() });

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

	m_fontTexture = m_allocator->Alloc<PBClient::FontTexture>(m_renderer, "TestAssets/Fonts/arial.ttf");
}

ClientPlayground::~ClientPlayground()
{
	m_allocator->Free(m_drawBatch);

	m_allocator->Free(m_geoDispatchList);
	m_allocator->Free(m_geoShadowDispatchList);

	DestroyResources();

	m_allocator->Free(m_renderGraph);

	// Free rendergraph nodes.
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

void ClientPlayground::Update(GLFWwindow* window, Input* input, float deltaTime, float elapsedTime)
{
	m_camera.Update(deltaTime, input, window);

	// Update Camera -------------------------------------------------------------------------------------------------
	{
		constexpr float fov = 75.0f;
		constexpr float fovRadians = glm::radians(fov);

		MVPBuffer* bufferMatrices = (MVPBuffer*)m_mvpBuffer->BeginPopulate();

		// Model
		glm::mat4& model = bufferMatrices->m_model;
		model = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.1f * elapsedTime, 0.0f));
		model = glm::scale(model, glm::vec3(0.01f));
		model = glm::rotate<float>(model, elapsedTime * 0.2f, glm::vec3(0.0f, 1.0f, 0.0f));

		// View
		bufferMatrices->m_view = m_camera.GetViewMatrix(); // View
		bufferMatrices->m_invView = glm::inverse(bufferMatrices->m_view);

		// Projection
		glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
		axisCorrection[1][1] = -1.0f;
		axisCorrection[2][2] = 1.0f;
		axisCorrection[3][3] = 1.0f;

		bufferMatrices->m_proj = axisCorrection * glm::perspectiveFov<float>(fovRadians, (float)m_swapchain->GetWidth(), (float)m_swapchain->GetHeight(), 0.1f, 1000.0f);
		bufferMatrices->m_invProj = glm::inverse(bufferMatrices->m_proj);

		// Position
		bufferMatrices->m_camPos = glm::vec4(m_camera.GetPosition(), 1.0f);

		// Depth Reconstruction Constants
		bufferMatrices->m_aspectRatio = float(m_swapchain->GetWidth()) / m_swapchain->GetHeight();
		bufferMatrices->m_tanHalfFOV = glm::tan(fovRadians / 2);

		glm::mat4 modelCpy = model;

		m_mvpBuffer->EndPopulate();

		m_drawBatch->UpdateInstanceModelMatrix(m_firstInstanceHandles[0], glm::value_ptr(modelCpy));
		m_drawBatch->UpdateInstanceModelMatrix(m_firstInstanceHandles[1], glm::value_ptr(modelCpy));
		m_drawBatch->UpdateInstanceModelMatrix(m_firstInstanceHandles[2], glm::value_ptr(modelCpy));

		m_drawBatch->FinalizeUpdates();
	}
	// ---------------------------------------------------------------------------------------------------------------

	// Change render node output -------------------------------------------------------------------------------------
	{
		PB::u32 swapChainIdx = m_renderer->GetCurrentSwapchainImageIndex();
		auto* swapChainTex = m_swapchain->GetImage(swapChainIdx);
		m_bloomBlurPass->SetOutputTexture(swapChainTex);
		//m_ambientOcclusionPass->SetOutputTexture(swapChainTex);
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
	m_bloomBlurPass->SetOutputTexture(swapChainTex);
}

void ClientPlayground::InitResources()
{
	PB::BufferObjectDesc mvpBufferDesc;
	mvpBufferDesc.m_bufferSize = sizeof(MVPBuffer);
	mvpBufferDesc.m_options = PB::EBufferOptions::ZERO_INITIALIZE;
	mvpBufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::UNIFORM;
	m_mvpBuffer = m_renderer->AllocateBuffer(mvpBufferDesc);

	// Basic textures
	PB::u8 texData[4] = { 255, 255, 255, 255 };
	PB::TextureDesc solidTextureDesc{};
	solidTextureDesc.m_data.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
	solidTextureDesc.m_data.m_data = texData;
	solidTextureDesc.m_data.m_size = sizeof(texData);
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
	m_paintMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_paint", true, m_vertexPool);
	m_detailsMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_details", true, m_vertexPool);
	m_glassMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Objects/Spinner/mesh_spinner_low_glass", true, m_vertexPool);
	m_planeMesh = m_allocator->Alloc<PBClient::Mesh>(m_renderer, "Meshes/Primitives/plane", true, m_vertexPool);

	m_paintTextures[0] = m_allocator->Alloc<PBClient::Texture> (m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_diffuse.tga");
	m_detailsTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_diffuse.tga");
	m_glassTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_diffuse.tga");

	m_paintTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_normal.tga");
	m_detailsTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_normal.tga");
	m_glassTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_normal.tga");

	m_paintTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_specular.tga");
	m_detailsTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_specular.tga");
	m_glassTextures[2] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_specular.tga");

	m_paintTextures[3] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_roughness.tga");
	m_detailsTextures[3] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_roughness.tga");
	m_glassTextures[3] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_roughness.tga");

	m_detailsTextures[4] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_emissive.tga");
	m_glassTextures[4] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_emissive.tga");

	m_metalTextures[0] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Metal/diffuse.tga");
	m_metalTextures[1] = m_allocator->Alloc<PBClient::Texture>(m_renderer, "TestAssets/Objects/Metal/normal.tga");

	for (int i = 0; i < _countof(m_paintTextures); ++i)
		m_paintViews[i] = m_paintTextures[i]->GetTexture()->GetDefaultSRV();
	m_paintViews[4] = m_solidBlackTexture->GetDefaultSRV();

	for (int i = 0; i < _countof(m_detailsTextures); ++i)
		m_detailsViews[i] = m_detailsTextures[i]->GetTexture()->GetDefaultSRV();
	//m_detailsViews[4] = m_solidBlackTexture->GetDefaultSRV();

	for (int i = 0; i < _countof(m_glassTextures); ++i)
		m_glassViews[i] = m_glassTextures[i]->GetTexture()->GetDefaultSRV();
	//m_glassViews[4] = m_solidBlackTexture->GetDefaultSRV();

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
				m_gBufferPass = m_allocator->Alloc<GBufferPass>(m_renderer, m_allocator);
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

		output = rgBuilder.Build();
	}

	m_shadowmapPass->SetDispatchList(m_geoShadowDispatchList, true);
	m_gBufferPass->SetDispatchList(m_geoDispatchList, true);

	// Set up lighting
	{
		glm::vec3 sunDir(1.0f, 1.0f, 0.0f);

		Camera shadowCam(glm::vec3(0.0f, 0.0f, -4.0f) + (sunDir * 50.0f), glm::radians(glm::vec3(-45.0f, 45.0f, 0.0f)));

		glm::mat4 shadowView = shadowCam.GetViewMatrix();

		constexpr const float ShadowDistance = 8.0f;

		m_shadowmapPass->SetViewMatrix(glm::value_ptr(shadowView));
		m_shadowmapPass->SetShadowParameters(ShadowDistance, 0.5f, 0.2f, ShadowmapResolution);

		m_deferredLightingPass->SetMVPBuffer(m_mvpBuffer);

		m_shadowAccumPass->SetMVPBuffer(m_mvpBuffer);
		m_shadowAccumPass->SetSVBBuffer(m_shadowmapPass->GetSVBView());

		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 0.3f, 0.3f, 0.3f, 0.3f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
		//m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 5.0f, 5.0f, 5.0f, 1.0f });
		m_deferredLightingPass->SetDirectionalLight(0, { sunDir.x, sunDir.y, sunDir.z, 1.0f }, { 0.3f, 0.3f, 0.4f, 1.0f });
		m_deferredLightingPass->SetPointLight(0, { -1.0f, 3.0f, -4.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }, 5.0f);
		m_deferredLightingPass->SetPointLight(1, { 1.0f, 3.0f, -4.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, 5.0f);
	}

	return output;
}

void ClientPlayground::SetupDrawBatch()
{
	PB::UniformBufferView mvpView = m_mvpBuffer->GetViewAsUniformBuffer();

	uint32_t indexCount = ((m_paintMesh->IndexCount() + m_detailsMesh->IndexCount() + m_glassMesh->IndexCount()) * (255 / 3)) + m_planeMesh->IndexCount();

	m_drawBatch = m_allocator->Alloc<DrawBatch>(m_renderer, m_allocator, m_vertexPool, indexCount);
	m_drawBatch->AddToDispatchList(m_geoShadowDispatchList, GetShadowDrawBatchPipeline(), m_shadowmapPass->GetDrawBatchBindings());
	m_drawBatch->AddToDispatchList(m_geoDispatchList, GetGBufferDrawBatchPipeline(), GetGBufferDrawBatchBindings(mvpView));

	glm::mat4 modelMat = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -4.0f));
	glm::mat4 spinnerModelMat = glm::scale(modelMat, glm::vec3(0.01f)); // Convert cm to m.

	PB::ResourceView plainViews[]
	{
		m_solidWhiteTexture->GetDefaultSRV(),
		//m_metalTextures[1]->GetTexture()->GetDefaultSRV(),
		m_flatNormalTexture->GetDefaultSRV(),
		m_solidBlackTexture->GetDefaultSRV(),
		m_solidBlackTexture->GetDefaultSRV(),
		m_solidBlackTexture->GetDefaultSRV()
	};

	/*PB::ResourceView plainViews[]
	{
		m_metalTextures[0]->GetTexture()->GetDefaultSRV(),
		m_metalTextures[1]->GetTexture()->GetDefaultSRV(),
		m_solidBlackTexture->GetDefaultSRV(),
		m_solidBlackTexture->GetDefaultSRV()
	};*/

	for (uint32_t i = 0; i < 2; ++i)
	{
		modelMat = glm::translate(glm::mat4(), glm::vec3(4.0f * (i / 10), -0.2f, -7.0f * (i % 10)));
		modelMat = glm::rotate(modelMat, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		spinnerModelMat = glm::scale(modelMat, glm::vec3(0.01f)); // Convert cm to m.

		if (i == 0)
		{
			m_firstInstanceHandles[0] = m_drawBatch->AddInstance(m_paintMesh, glm::value_ptr(spinnerModelMat), m_paintViews, _countof(m_paintViews), m_colorSampler);
			m_firstInstanceHandles[1] = m_drawBatch->AddInstance(m_detailsMesh, glm::value_ptr(spinnerModelMat), m_detailsViews, _countof(m_detailsViews), m_colorSampler);
			m_firstInstanceHandles[2] = m_drawBatch->AddInstance(m_glassMesh, glm::value_ptr(spinnerModelMat), m_glassViews, _countof(m_glassViews), m_colorSampler);
			continue;
		}

		m_drawBatch->AddInstance(m_paintMesh, glm::value_ptr(spinnerModelMat), m_paintViews, _countof(m_paintViews), m_colorSampler);
		m_drawBatch->AddInstance(m_detailsMesh, glm::value_ptr(spinnerModelMat), m_detailsViews, _countof(m_detailsViews), m_colorSampler);
		m_drawBatch->AddInstance(m_glassMesh, glm::value_ptr(spinnerModelMat), m_glassViews, _countof(m_glassViews), m_colorSampler);
	}

	modelMat = glm::translate(glm::mat4(), glm::vec3(0.0f, -0.2f, -4.0f));
	m_drawBatch->AddInstance(m_planeMesh, glm::value_ptr(modelMat), plainViews, _countof(plainViews), m_colorSampler);
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
