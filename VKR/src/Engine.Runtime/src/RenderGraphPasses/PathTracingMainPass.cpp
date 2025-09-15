#include "PathTracingMainPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/Shader.h"
#include "Resource/Mesh.h"
#include "Resource/Texture.h"
#include "Engine.Math/Scalar.h"

namespace Eng
{
	PathTracingMainPass::PathTracingMainPass(PB::IRenderer* renderer, CLib::Allocator* allocator, const CreateDesc& desc) : RenderGraphBehaviour(renderer, allocator)
	{
		m_desc = desc;
		m_renderer = renderer;
		m_allocator = allocator;

		PB::TextureViewDesc noiseViewDesc{};
		noiseViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
		noiseViewDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		noiseViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_2D_ARRAY;
		noiseViewDesc.m_subresources = PB::SubresourceRange::AllArrayLayers();
		m_noiseTextureView = m_desc.noiseTexturesArray->GetTexture()->GetView(noiseViewDesc);

		enum class EPathTracePermutation
		{
			STAGE,
			USE_CAMERA_RAYS,
			SHADOW_RAY_COUNT,
			PERMUTATION_END
		};

		PB::RayTracingPipelineDesc rtShadowPipelineDesc{};

		AssetEncoder::ShaderPermutationTable<EPathTracePermutation> ptPermTable{};
		ptPermTable.SetPermutation(EPathTracePermutation::STAGE, AssetEncoder::ERTShaderStagePermutation::RAYGEN);
		ptPermTable.SetPermutation(EPathTracePermutation::USE_CAMERA_RAYS, uint8_t(m_desc.useCameraRays));
		ptPermTable.SetPermutation(EPathTracePermutation::SHADOW_RAY_COUNT, std::max<uint32_t>(m_desc.shadowRaysPerPixel - 1, 0u));
		{
			rtShadowPipelineDesc.rayGenShaderModule = Eng::Shader(m_renderer, "Shaders/GLSL/rt_pathtrace", ptPermTable.GetKey(), m_allocator, true).GetModule();
		}

		AssetEncoder::ShaderPermutationTable genericStagePermTable{};
		genericStagePermTable.SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, AssetEncoder::ERTShaderStagePermutation::MISS);
		{
			rtShadowPipelineDesc.missShaderModules[0] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_shadows", genericStagePermTable.GetKey(), m_allocator, true).GetModule();
			rtShadowPipelineDesc.missShaderModules[1] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_reflections", genericStagePermTable.GetKey(), m_allocator, true).GetModule();
		}

		ptPermTable.SetPermutation(EPathTracePermutation::STAGE, AssetEncoder::ERTShaderStagePermutation::CLOSESTHIT);
		genericStagePermTable.Reset().SetPermutation(AssetEncoder::EDefaultPermutationID::PERMUTATION_0, AssetEncoder::ERTShaderStagePermutation::CLOSESTHIT);
		{
			rtShadowPipelineDesc.closestHitShaderModules[0] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_pathtrace", ptPermTable.GetKey(), m_allocator, true).GetModule();
			rtShadowPipelineDesc.closestHitShaderModules[1] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_shadows", genericStagePermTable.GetKey(), m_allocator, true).GetModule();
			rtShadowPipelineDesc.closestHitShaderModules[2] = Eng::Shader(m_renderer, "Shaders/GLSL/rt_reflections", genericStagePermTable.GetKey(), m_allocator, true).GetModule();
		}

		m_shadowRaytracingPipeline = m_renderer->GetPipelineCache()->GetPipeline(rtShadowPipelineDesc);

		PB::TextureViewDesc skyboxViewDesc{};
		skyboxViewDesc.m_texture = m_desc.skyboxTexture->GetTexture();
		skyboxViewDesc.m_expectedState = PB::ETextureState::SAMPLED;
		skyboxViewDesc.m_format = m_desc.skyboxTexture->IsCompressed() ? PB::ETextureFormat::BC6H_RGB_U16F : PB::ETextureFormat::R16G16B16A16_FLOAT;
		skyboxViewDesc.m_type = PB::ETextureViewType::VIEW_TYPE_CUBE;
		skyboxViewDesc.m_subresources.m_arrayCount = 1;
		skyboxViewDesc.m_subresources.m_mipCount = 1;

		m_skyboxView = m_desc.skyboxTexture->GetTexture()->GetView(skyboxViewDesc);

		PB::SamplerDesc skySamplerDesc;
		skySamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
		skySamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		skySamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
		skySamplerDesc.maxLod = 1.0f;
		m_skySampler = m_renderer->GetSampler(skySamplerDesc);
	}

	PathTracingMainPass::~PathTracingMainPass()
	{
		for (auto& tex : m_shadowAccumTextures)
		{
			if (tex)
				m_renderer->FreeTexture(tex);
			tex = nullptr;
		}
		for (auto& tex : m_reflectionAccumTextures)
		{
			if (tex)
				m_renderer->FreeTexture(tex);
			tex = nullptr;
		}
	}

	void PathTracingMainPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		
	}

	void PathTracingMainPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("PathTracingMainPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		const PB::IAccelerationStructure* asArr = *m_desc.tlas;
		info.m_commandContext->CmdBuildAccelerationStructureToTraceRaysBarrier(&asArr, 1);

		info.m_commandContext->CmdBindPipeline(m_shadowRaytracingPipeline);

		PB::u32 prevAccumTexIndex = m_currentAccumTexIndex;
		m_currentAccumTexIndex = m_currentAccumTexIndex == 0 ? 1 : 0;

		PB::UniformBufferView uniformViews[] = { m_desc.viewConstView, m_desc.worldConstantsBuffer->GetViewAsUniformBuffer() };
		PB::ResourceView resources[] =
		{
			(*m_desc.tlasInstanceIndexBuffer)->GetViewAsStorageBuffer(),
			Mesh::GetMeshLibraryView(),
			!m_desc.useCameraRays ? transientTextures[m_normalGBufferIndex]->GetDefaultSRV() : 0,
			!m_desc.useCameraRays ? transientTextures[m_specAndRoughGBufferIndex]->GetDefaultSRV() : 0,
			!m_desc.useCameraRays ? transientTextures[m_depthBufferIndex]->GetDefaultSRV() : 0,
			transientTextures[m_motionVectorsIndex]->GetDefaultSRV(),
			m_noiseTextureView,
			m_skyboxView,
			m_skySampler,
			m_shadowAccumTextures[prevAccumTexIndex]->GetDefaultSIV(), // Previous frame's accum tex
			m_shadowAccumTextures[m_currentAccumTexIndex]->GetDefaultSIV(), // current frame's accum tex
			transientTextures[m_outShadowMaskIndex]->GetDefaultSIV(), // Shadow output tex
			transientTextures[m_outPenumbraMaskIndex]->GetDefaultSIV(), // Penumbra output tex
			m_reflectionAccumTextures[prevAccumTexIndex]->GetDefaultSIV(), // Previous frame's accum tex
			m_reflectionAccumTextures[m_currentAccumTexIndex]->GetDefaultSIV(), // current frame's accum tex
			transientTextures[m_outReflectionIndex]->GetDefaultSIV() // Reflection output tex
		};

		PB::BindingLayout bindings{};
		bindings.m_uniformBufferCount = _countof(uniformViews);
		bindings.m_uniformBuffers = uniformViews;
		bindings.m_resourceCount = _countof(resources);
		bindings.m_resourceViews = resources;

		info.m_commandContext->CmdBindResources(bindings);
		info.m_commandContext->CmdBindAccelerationStructure(*m_desc.tlas);

		info.m_commandContext->CmdTraceRays(m_renderWidth, m_renderHeight, 1);

		info.m_commandContext->CmdEndLastLabel();
	}

	void PathTracingMainPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void PathTracingMainPass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_useReusableCommandLists = false;
		nodeDesc.m_computeOnlyPass = true;
		nodeDesc.m_renderWidth = 1;
		nodeDesc.m_renderHeight = 1;

		m_renderWidth = targetResolution.x;
		m_renderHeight = targetResolution.y;

		for (auto& tex : m_shadowAccumTextures)
		{
			if(tex)
				m_renderer->FreeTexture(tex);
			tex = nullptr;
		}
		for (auto& tex : m_reflectionAccumTextures)
		{
			if (tex)
				m_renderer->FreeTexture(tex);
			tex = nullptr;
		}

		PB::TextureDesc accumTextureDesc{};
		accumTextureDesc.m_name = "ShadowMaskAccum";
		accumTextureDesc.m_format = PB::ETextureFormat::R8G8_UNORM;
		accumTextureDesc.m_width = Math::RoundUp<PB::u32>(m_renderWidth, 2) / 2;
		accumTextureDesc.m_height = m_renderHeight;
		accumTextureDesc.m_usageStates = PB::ETextureState::STORAGE;
		m_shadowAccumTextures[0] = m_renderer->AllocateTexture(accumTextureDesc);
		m_shadowAccumTextures[1] = m_renderer->AllocateTexture(accumTextureDesc);

		accumTextureDesc.m_name = "ReflectionAccum";
		accumTextureDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		m_reflectionAccumTextures[0] = m_renderer->AllocateTexture(accumTextureDesc);
		m_reflectionAccumTextures[1] = m_renderer->AllocateTexture(accumTextureDesc);

		if (m_desc.useCameraRays == false)
		{
			m_normalGBufferIndex = uint8_t(nodeDesc.m_transientTextures.Count());
			TransientTextureDesc& normalDesc = nodeDesc.m_transientTextures.PushBackInit();
			normalDesc.m_format = PB::ETextureFormat::A2R10G10B10_UNORM;
			normalDesc.m_width = m_renderWidth;
			normalDesc.m_height = m_renderHeight;
			normalDesc.m_name = "G_Normal";
			normalDesc.m_initialUsage = PB::ETextureState::SAMPLED;
			normalDesc.m_usageFlags = PB::ETextureState::SAMPLED;

			m_depthBufferIndex = uint8_t(nodeDesc.m_transientTextures.Count());
			TransientTextureDesc& depthReadDesc = nodeDesc.m_transientTextures.PushBackInit();
			depthReadDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
			depthReadDesc.m_width = m_renderWidth;
			depthReadDesc.m_height = m_renderHeight;
			depthReadDesc.m_name = "G_Depth";
			depthReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
			depthReadDesc.m_usageFlags = PB::ETextureState::SAMPLED;

			m_specAndRoughGBufferIndex = uint8_t(nodeDesc.m_transientTextures.Count());
			TransientTextureDesc& specAndRoughDesc = nodeDesc.m_transientTextures.PushBackInit();
			specAndRoughDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
			specAndRoughDesc.m_width = m_renderWidth;
			specAndRoughDesc.m_height = m_renderHeight;
			specAndRoughDesc.m_name = "G_SpecAndRough";
			specAndRoughDesc.m_initialUsage = PB::ETextureState::SAMPLED;
			specAndRoughDesc.m_usageFlags = PB::ETextureState::SAMPLED;
		}

		m_outShadowMaskIndex = uint8_t(nodeDesc.m_transientTextures.Count());
		TransientTextureDesc& outDesc = nodeDesc.m_transientTextures.PushBackInit();
		outDesc.m_format = PB::ETextureFormat::R8_UNORM;
		outDesc.m_width = m_renderWidth;
		outDesc.m_height = m_renderHeight;
		outDesc.m_name = "ShadowMask";
		outDesc.m_initialUsage = PB::ETextureState::STORAGE;
		outDesc.m_finalUsage = PB::ETextureState::STORAGE;
		outDesc.m_usageFlags = PB::ETextureState::STORAGE;

		m_outPenumbraMaskIndex = uint8_t(nodeDesc.m_transientTextures.Count());
		TransientTextureDesc& outPenumbraDesc = nodeDesc.m_transientTextures.PushBackInit();
		outPenumbraDesc.m_format = PB::ETextureFormat::R8_UNORM;
		outPenumbraDesc.m_width = m_renderWidth;
		outPenumbraDesc.m_height = m_renderHeight;
		outPenumbraDesc.m_name = "ShadowPenumbraMask";
		outPenumbraDesc.m_initialUsage = PB::ETextureState::STORAGE;
		outPenumbraDesc.m_finalUsage = PB::ETextureState::STORAGE;
		outPenumbraDesc.m_usageFlags = PB::ETextureState::STORAGE;

		m_outReflectionIndex = uint8_t(nodeDesc.m_transientTextures.Count());
		TransientTextureDesc& reflectionTextureDesc = nodeDesc.m_transientTextures.PushBackInit();
		reflectionTextureDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		reflectionTextureDesc.m_width = m_renderWidth;
		reflectionTextureDesc.m_height = m_renderHeight;
		reflectionTextureDesc.m_name = "RT_Reflections";
		reflectionTextureDesc.m_initialUsage = PB::ETextureState::STORAGE;
		reflectionTextureDesc.m_finalUsage = PB::ETextureState::STORAGE;
		reflectionTextureDesc.m_usageFlags = PB::ETextureState::STORAGE;

		m_motionVectorsIndex = uint8_t(nodeDesc.m_transientTextures.Count());
		TransientTextureDesc& motionVectorsDesc = nodeDesc.m_transientTextures.PushBackInit();
		motionVectorsDesc.m_format = PB::ETextureFormat::R16G16_FLOAT;
		motionVectorsDesc.m_width = m_renderWidth;
		motionVectorsDesc.m_height = m_renderHeight;
		motionVectorsDesc.m_name = "G_MotionVectors";
		motionVectorsDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		motionVectorsDesc.m_finalUsage = PB::ETextureState::SAMPLED;
		motionVectorsDesc.m_usageFlags = PB::ETextureState::SAMPLED;

		builder->AddNode(nodeDesc);
	}
};