#include "ReflectionBlurPass.h"
#include "RenderGraph/RenderGraph.h"
#include "BlurHelper.h"

#include <Engine.Math/Scalar.h>

namespace Eng
{
	ReflectionBlurPass::ReflectionBlurPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
	{
		PB::BufferObjectDesc blurConstantsDesc{};
		blurConstantsDesc.m_bufferSize = sizeof(BlurConstants);
		blurConstantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
		m_blurConstants = m_renderer->AllocateBuffer(blurConstantsDesc);

		PB::SamplerDesc blurSamplerDesc;
		blurSamplerDesc.m_anisotropyLevels = 1.0f;
		blurSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		blurSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
		blurSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
		m_blurImageSampler = m_renderer->GetSampler(blurSamplerDesc);
	}

	ReflectionBlurPass::~ReflectionBlurPass()
	{
		if (m_reusableCmdList)
			m_renderer->FreeCommandList(m_reusableCmdList);

		m_renderer->FreeBuffer(m_blurConstants);
	}

	void ReflectionBlurPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void ReflectionBlurPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		info.m_commandContext->CmdBeginLabel("ReflectionBlurPass", { 1.0f, 1.0f, 1.0f, 1.0f });

		auto RecordPass = [&]()
		{
			uint32_t dispatchRenderWidth = m_targetResolution.x;
			uint32_t dispatchRenderHeight = m_targetResolution.y;
			constexpr uint32_t GaussianKernelSizeMinusOne = GaussianKernelSize - 1;
			constexpr uint32_t WorkGroupX = 1;
			constexpr uint32_t WorkGroupY = 256;

			PB::CommandContextDesc scopedContextDesc{};
			scopedContextDesc.m_renderer = m_renderer;
			scopedContextDesc.m_usage = PB::ECommandContextUsage::COMPUTE;
			scopedContextDesc.m_flags = PB::ECommandContextFlags::REUSABLE;

			PB::SCommandContext scopedContext(m_renderer);
			scopedContext->Init(scopedContextDesc);
			scopedContext->Begin();

			constexpr const float TwoPi = 2.0f * 3.14159f;
			constexpr const float KernelSampleCount = (float)GaussianKernelSize;

			BlurConstants* blurConstants = reinterpret_cast<BlurConstants*>(m_blurConstants->BeginPopulate());
			blurConstants->m_depthDiscontinuityThreshold = 0.001f;
			blurConstants->m_depthScaleFactor = 16.0f;

			float sigma = KernelSampleCount / 3.0f;
			float doubleSigmaSqr = 2 * (sigma * sigma);
			for (uint32_t i = 0; i < GaussianKernelSize; ++i)
			{
				float iFloat = static_cast<float>(i);
				blurConstants->m_weights[i].x = Math::Exp(-(iFloat * iFloat) / doubleSigmaSqr);
			}
			blurConstants->m_guassianNormPart = 1.0f / (sigma * Math::Sqrt(TwoPi)); // First half of the guassian function. Multiplying by this will normalize the blur colour samples.

			m_blurConstants->EndPopulate();

			enum class EBlurPermutation
			{
				PERMUTATION_HORIZONTAL,
				PERMUTATION_KERNEL_SIZE,
				PERMUTATION_RAY_TRACED_SHADOWS,
				PERMUTATION_END
			};
			AssetEncoder::ShaderPermutationTable<EBlurPermutation> permTable{};
			permTable.SetPermutation(EBlurPermutation::PERMUTATION_KERNEL_SIZE, uint8_t(BlurKernelSizeToEnum(GaussianKernelSize)));

			PB::ComputePipelineDesc blurPipelineDesc{};
			permTable.SetPermutation(EBlurPermutation::PERMUTATION_HORIZONTAL, 0);
			blurPipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_reflection_blur", permTable.GetKey(), m_allocator, true).GetModule();
			PB::Pipeline verticalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(blurPipelineDesc);

			permTable.SetPermutation(EBlurPermutation::PERMUTATION_HORIZONTAL, 1);
			blurPipelineDesc.m_computeModule = Shader(m_renderer, "Shaders/GLSL/cs_reflection_blur", permTable.GetKey(), m_allocator, true).GetModule();
			PB::Pipeline horizontalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(blurPipelineDesc);

			PB::ResourceView resourceViews[]
			{
				0,
				m_blurImageSampler,
				0,
				transientTextures[m_depthBufferIndex]->GetDefaultSRV(),
				transientTextures[m_roughnessBufferIndex]->GetDefaultSRV()
			};

			PB::ResourceView bufASRV = transientTextures[m_blurBufferIndex]->GetDefaultSRV();
			PB::ResourceView bufBSRV = transientTextures[m_blurAuxBufferIndex]->GetDefaultSRV();

			PB::ResourceView bufASIV = transientTextures[m_blurBufferIndex]->GetDefaultSIV();
			PB::ResourceView bufBSIV = transientTextures[m_blurAuxBufferIndex]->GetDefaultSIV();

			PB::UniformBufferView blurConstantsView = m_blurConstants->GetViewAsUniformBuffer();
			PB::BindingLayout bindings;
			bindings.m_uniformBufferCount = 1;
			bindings.m_uniformBuffers = &blurConstantsView;
			bindings.m_resourceCount = _countof(resourceViews);
			bindings.m_resourceViews = resourceViews;

			// The blur shader uses some of it's work group invocations to store excess off-edge samples of count: (GaussianKernelSize - 1) * 2.
			// This reduces the amount of pixels each work group writes to by that amount, so we divide our screen resolution by that new amount.
			uint32_t tileDim = WorkGroupY - (2 * GaussianKernelSizeMinusOne);
			uint32_t workGroupCountV = (dispatchRenderHeight / tileDim) + (dispatchRenderHeight % tileDim > 0 ? 1 : 0);
			uint32_t workGroupCountH = (dispatchRenderWidth / tileDim) + (dispatchRenderWidth % tileDim > 0 ? 1 : 0);

			// Vertical blur pass.
			scopedContext->CmdBindPipeline(verticalBlurPipeline);
			resourceViews[0] = bufASRV;
			resourceViews[2] = bufBSIV;
			scopedContext->CmdBindResources(bindings);
			scopedContext->CmdDispatch(dispatchRenderWidth / WorkGroupX, workGroupCountV, 1);

			scopedContext->CmdTransitionTexture(transientTextures[m_blurBufferIndex], PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE);
			scopedContext->CmdTransitionTexture(transientTextures[m_blurAuxBufferIndex], PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED);

			// Horizontal blur pass.
			scopedContext->CmdBindPipeline(horizontalBlurPipeline);
			resourceViews[0] = bufBSRV;
			resourceViews[2] = bufASIV;
			scopedContext->CmdBindResources(bindings);
			scopedContext->CmdDispatch(workGroupCountH, dispatchRenderHeight / WorkGroupX, 1);

			uint32_t totalGroupCountV = (dispatchRenderWidth / WorkGroupX) * workGroupCountV;
			uint32_t totalGroupCountH = (dispatchRenderHeight / WorkGroupX) * workGroupCountH;

			scopedContext->End();
			return scopedContext->Return();
		};

		if (!m_reusableCmdList)
			m_reusableCmdList = RecordPass();

		info.m_commandContext->CmdExecuteList(m_reusableCmdList);
		info.m_commandContext->CmdEndLastLabel();
	}

	void ReflectionBlurPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{

	}

	void ReflectionBlurPass::AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution)
	{
		if (m_reusableCmdList)
		{
			m_renderer->FreeCommandList(m_reusableCmdList);
			m_reusableCmdList = nullptr;
		}

		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
		nodeDesc.m_computeOnlyPass = true;
		nodeDesc.m_useReusableCommandLists = true;

		m_blurBufferIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& reflectionBufferDesc = nodeDesc.m_transientTextures.PushBackInit();
		reflectionBufferDesc.m_format = PB::ETextureFormat::R16G16B16A16_FLOAT;
		reflectionBufferDesc.m_width = targetResolution.x;
		reflectionBufferDesc.m_height = targetResolution.y;
		reflectionBufferDesc.m_name = "RT_Reflections";
		reflectionBufferDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		reflectionBufferDesc.m_finalUsage = PB::ETextureState::STORAGE;
		reflectionBufferDesc.m_usageFlags = reflectionBufferDesc.m_initialUsage | PB::ETextureState::STORAGE;

		m_blurAuxBufferIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& reflectionAuxBufferDesc = nodeDesc.m_transientTextures.PushBackInit();
		reflectionAuxBufferDesc.m_format = reflectionBufferDesc.m_format;
		reflectionAuxBufferDesc.m_width = targetResolution.x;
		reflectionAuxBufferDesc.m_height = targetResolution.y;
		reflectionAuxBufferDesc.m_name = "RT_ReflectionsBlurAuxBuffer";
		reflectionAuxBufferDesc.m_initialUsage = PB::ETextureState::STORAGE;
		reflectionAuxBufferDesc.m_finalUsage = PB::ETextureState::SAMPLED;
		reflectionAuxBufferDesc.m_usageFlags = reflectionAuxBufferDesc.m_initialUsage | PB::ETextureState::SAMPLED;

		m_depthBufferIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& depthReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		depthReadDesc.m_format = PB::ETextureFormat::D24_UNORM_S8_UINT;
		depthReadDesc.m_width = targetResolution.x;
		depthReadDesc.m_height = targetResolution.y;
		depthReadDesc.m_name = "G_Depth";
		depthReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		depthReadDesc.m_usageFlags = depthReadDesc.m_initialUsage;

		m_roughnessBufferIndex = nodeDesc.m_transientTextures.Count();
		TransientTextureDesc& roughnessReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		roughnessReadDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		roughnessReadDesc.m_width = targetResolution.x;
		roughnessReadDesc.m_height = targetResolution.y;
		roughnessReadDesc.m_name = "G_SpecAndRough";
		roughnessReadDesc.m_initialUsage = PB::ETextureState::SAMPLED;
		roughnessReadDesc.m_usageFlags = roughnessReadDesc.m_initialUsage;

		nodeDesc.m_renderWidth = reflectionBufferDesc.m_width;
		nodeDesc.m_renderHeight = reflectionBufferDesc.m_height;
		m_targetResolution = targetResolution;

		builder->AddNode(nodeDesc);
	}
};