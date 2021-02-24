#include "GBufferPass.h"

#include "DrawBatch.h"

using namespace PBClient;

GBufferPass::GBufferPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;

	m_spinnerVertPool = m_allocator->Alloc<VertexPool>(m_renderer, sizeof(Vertex) * 1000000, sizeof(Vertex));
	m_paintMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_paint.obj", m_spinnerVertPool);
	m_detailsMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_details.obj", m_spinnerVertPool);
	m_glassMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Objects/Spinner/mesh_spinner_low_glass.obj", m_spinnerVertPool);
	m_planeMesh = m_allocator->Alloc<Mesh>(m_renderer, "TestAssets/Primitives/plane.obj", m_spinnerVertPool);
	
	m_paintTextures[0] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_diffuse.tga");
	m_detailsTextures[0] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_diffuse.tga");
	m_glassTextures[0] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_diffuse.tga");

	m_paintTextures[1] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_normal.tga");
	m_detailsTextures[1] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_normal.tga");
	m_glassTextures[1] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_normal.tga");

	m_paintTextures[2] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_specular.tga");
	m_detailsTextures[2] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_specular.tga");
	m_glassTextures[2] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_specular.tga");

	m_paintTextures[3] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/paint2048/m_spinner_paint_roughness.tga");
	m_detailsTextures[3] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/details2048/m_spinner_details_roughness.tga");
	m_glassTextures[3] = m_allocator->Alloc<Texture>(m_renderer, "TestAssets/Objects/Spinner/glass2048/m_spinner_glass_roughness.tga");

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
	m_solidWhiteTex = m_renderer->AllocateTexture(solidTextureDesc);
	
	texData[0] = 0;
	texData[1] = 0;
	texData[2] = 0;
	m_solidBlackTex = m_renderer->AllocateTexture(solidTextureDesc);

	texData[0] = 128;
	texData[1] = 128;
	texData[2] = 255;
	m_flatNormalTex = m_renderer->AllocateTexture(solidTextureDesc);

	for (PB::u32 i = 0; i < _countof(m_paintTextures); ++i)
		m_paintViews[i] = m_paintTextures[i]->GetTexture()->GetDefaultSRV();
	for (PB::u32 i = 0; i < _countof(m_detailsTextures); ++i)
		m_detailsViews[i] = m_detailsTextures[i]->GetTexture()->GetDefaultSRV();
	for (PB::u32 i = 0; i < _countof(m_glassTextures); ++i)
		m_glassViews[i] = m_glassTextures[i]->GetTexture()->GetDefaultSRV();

	//m_vertShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_obj_def.spv", m_allocator);
	//m_fragShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_obj_def.spv", m_allocator);
	m_vertShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/vs_obj_def_batch.spv", m_allocator);
	m_fragShader = m_allocator->Alloc<Shader>(m_renderer, "TestAssets/Shaders/SPIR-V/fs_obj_def_batch.spv", m_allocator);

	PB::SamplerDesc colorSamplerDesc;
	colorSamplerDesc.m_anisotropyLevels = 1.0f;
	colorSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	colorSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::REPEAT;
	m_colorSampler = m_renderer->GetSampler(colorSamplerDesc);

	m_geoDispatchList.Init(renderer, m_allocator);
}

GBufferPass::~GBufferPass()
{
	m_allocator->Free(m_spinnerBatch);

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
	m_renderer->FreeTexture(m_solidWhiteTex);
	m_renderer->FreeTexture(m_solidBlackTex);
	m_renderer->FreeTexture(m_flatNormalTex);

	m_allocator->Free(m_paintMesh);
	m_allocator->Free(m_detailsMesh);
	m_allocator->Free(m_glassMesh);
	m_allocator->Free(m_planeMesh);

	m_allocator->Free(m_spinnerVertPool);

	m_allocator->Free(m_vertShader);
	m_allocator->Free(m_fragShader);
}

void GBufferPass::OnPreRenderPass(const RenderGraphInfo& info)
{
	PB::UniformBufferView mvpView = m_mvpBuffer->GetViewAsUniformBuffer();
	PB::ResourceView resources[]
	{
		m_paintViews[0],
		m_paintViews[1],
		m_paintViews[2],
		m_paintViews[3],
		m_colorSampler
	};

	PB::BindingLayout bindings{};
	bindings.m_uniformBufferCount = 1;
	bindings.m_uniformBuffers = &mvpView;
	bindings.m_resourceCount = _countof(resources);
	bindings.m_resourceViews = resources;

	if (!m_pipeline)
	{
		/*PB::GraphicsPipelineDesc pipelineDesc{};
		pipelineDesc.m_renderPass = info.m_renderPass;
		pipelineDesc.m_subpass = 0;
		pipelineDesc.m_renderArea = { 0, 0, info.m_renderer->GetSwapchain()->GetWidth(), info.m_renderer->GetSwapchain()->GetHeight() };
		pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_vertShader->GetModule();
		pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_fragShader->GetModule();
		pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
		pipelineDesc.m_attachmentCount = 3;
		pipelineDesc.m_vertexBuffers[0] = { sizeof(Vertex), PB::EVertexBufferType::VERTEX };
		pipelineDesc.m_vertexBuffers[1] = { sizeof(Vertex), PB::EVertexBufferType::INSTANCE };
		
		auto& vertexDesc = pipelineDesc.m_vertexDesc;
		vertexDesc.vertexAttributes[0] = { 0, PB::EVertexAttributeType::FLOAT4 };
		vertexDesc.vertexAttributes[1] = { 0, PB::EVertexAttributeType::FLOAT4 };
		vertexDesc.vertexAttributes[2] = { 0, PB::EVertexAttributeType::FLOAT4 };
		vertexDesc.vertexAttributes[3] = { 0, PB::EVertexAttributeType::FLOAT2 };
		vertexDesc.vertexAttributes[4] = { 1, PB::EVertexAttributeType::MAT4 };*/
		PB::GraphicsPipelineDesc pipelineDesc{};
		pipelineDesc.m_renderPass = info.m_renderPass;
		pipelineDesc.m_subpass = 0;
		pipelineDesc.m_renderArea = { 0, 0, info.m_renderer->GetSwapchain()->GetWidth(), info.m_renderer->GetSwapchain()->GetHeight() };
		pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = m_vertShader->GetModule();
		pipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = m_fragShader->GetModule();
		pipelineDesc.m_depthCompareOP = PB::ECompareOP::LEQUAL;
		pipelineDesc.m_attachmentCount = 3;

		m_pipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);

		m_spinnerBatch = m_allocator->Alloc<DrawBatch>(m_renderer, m_allocator, m_pipeline, m_mvpBuffer->GetViewAsUniformBuffer(), &m_geoDispatchList, m_spinnerVertPool);

		PB::DrawIndexedIndirectParams drawParams;
		drawParams.firstIndex = 0;
		drawParams.firstInstance = 0;
		drawParams.indexCount = m_paintMesh->IndexCount();
		drawParams.instanceCount = 1;
		drawParams.offset = 0;
		drawParams.vertexOffset = 0;

		float modelMat[16] =
		{
			0.01f, 0, 0, 0,
			0, 0.01f, 0, 0,
			0, 0, 0.01f, 0,
			0, 0, -4, 1,
		};

		//m_geoDispatchList.AddObject(m_pipeline, m_spinnerVertPool->GetPoolBuffer(), m_paintMesh->GetIndexBuffer(), bindings, drawParams, m_instanceBuffer);
		//m_geoDispatchList.AddObject(m_pipeline, m_paintMesh->GetVertexBuffer(), m_paintMesh->GetIndexBuffer(), bindings, drawParams, m_instanceBuffer);
		m_spinnerBatch->AddInstance(m_paintMesh, modelMat, m_paintViews, _countof(m_paintViews), m_colorSampler);

		resources[0] = m_detailsViews[0];
		resources[1] = m_detailsViews[1];
		resources[2] = m_detailsViews[2];
		resources[3] = m_detailsViews[3];
		drawParams.indexCount = m_detailsMesh->IndexCount();
		//m_geoDispatchList.AddObject(m_pipeline, m_detailsMesh->GetVertexBuffer(), m_detailsMesh->GetIndexBuffer(), bindings, drawParams, m_instanceBuffer);
		m_spinnerBatch->AddInstance(m_detailsMesh, modelMat, m_detailsViews, _countof(m_detailsViews), m_colorSampler);

		resources[0] = m_glassViews[0];
		resources[1] = m_glassViews[1];
		resources[2] = m_glassViews[2];
		resources[3] = m_glassViews[3];
		drawParams.indexCount = m_glassMesh->IndexCount();
		//m_geoDispatchList.AddObject(m_pipeline, m_glassMesh->GetVertexBuffer(), m_glassMesh->GetIndexBuffer(), bindings, drawParams, m_instanceBuffer);
		m_spinnerBatch->AddInstance(m_glassMesh, modelMat, m_glassViews, _countof(m_glassViews), m_colorSampler);

		float planeModelMat[16] =
		{
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, -0.2f, -4, 1,
		};

		PB::ResourceView whiteTexView = m_solidWhiteTex->GetDefaultSRV();
		PB::ResourceView blackTexView = m_solidBlackTex->GetDefaultSRV();
		PB::ResourceView flatNormalView = m_flatNormalTex->GetDefaultSRV();
		PB::ResourceView planeViews[4]
		{
			whiteTexView,
			flatNormalView,
			blackTexView,
			blackTexView,
		};

		m_spinnerBatch->AddInstance(m_planeMesh, planeModelMat, planeViews, _countof(planeViews), m_colorSampler);

		m_spinnerBatch->UpdateIndices(info.m_commandContext);
	}

	m_geoDispatchList.Update(info.m_commandContext);
}

void GBufferPass::OnPassBegin(const RenderGraphInfo& info)
{
	m_geoDispatchList.Dispatch(info.m_commandContext, info.m_renderPass, info.m_frameBuffer);
}

void GBufferPass::OnPostRenderPass(const RenderGraphInfo& info)
{
	if (!m_outputTexture)
		return;

	// Transition color and output to correct layouts...
	info.m_commandContext->CmdTransitionTexture(info.m_renderTargets[0], PB::ETextureState::COPY_SRC);
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST);
	
	// Copy color to output.
	info.m_commandContext->CmdCopyTextureToTexture(info.m_renderTargets[0], m_outputTexture);
	
	// Transition output texture to present.
	info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT);
}

void GBufferPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}

void GBufferPass::SetMVPBuffer(PB::IBufferObject* buffer)
{
	m_mvpBuffer = buffer;
}

void GBufferPass::SetInstanceBuffer(PB::IBufferObject* buffer)
{
	m_instanceBuffer = buffer;
}
