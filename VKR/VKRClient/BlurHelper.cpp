#include "BlurHelper.h"
#include "Shader.h"
#include "IRenderer.h"
#include "ICommandContext.h"

#pragma warning(push, 0)
#include "glm/glm.hpp"
#pragma warning(pop)

using namespace PBClient;

void BlurHelper::Init(PB::IRenderer* renderer, PB::u32 kernelSize)
{
	m_renderer = renderer;
	m_kernelSize = kernelSize;

	m_verticalPassModule = Shader(m_renderer, "Shaders/GLSL/cs_blur_v_fast", nullptr, true).GetModule();
	m_horizontalPassModule = Shader(m_renderer, "Shaders/GLSL/cs_blur_h_fast", nullptr, true).GetModule();

	PB::BufferObjectDesc constantsDesc{};
	constantsDesc.m_bufferSize = sizeof(BlurConstants) + (sizeof(PB::Float4) * kernelSize);
	constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
	m_blurConstantsBuffer = m_renderer->AllocateBuffer(constantsDesc);

	constexpr const float TwoPi = 2.0f * 3.14159f;
	float kernelSampleCount = (float)kernelSize;

	BlurConstants* blurConstants = reinterpret_cast<BlurConstants*>(m_blurConstantsBuffer->BeginPopulate());
	PB::Float4* weights = reinterpret_cast<PB::Float4*>(&reinterpret_cast<PB::u8*>(blurConstants)[sizeof(BlurConstants)]);

	float sigma = kernelSampleCount / 3.0f;
	float doubleSigmaSqr = 2 * (sigma * sigma);
	for (PB::u32 i = 0; i < kernelSize; ++i)
	{
		float iFloat = static_cast<float>(i);
		weights[i].x = glm::exp(-(iFloat * iFloat) / doubleSigmaSqr);
	}

	blurConstants->m_guassianNormPart = 1.0f / (sigma * glm::sqrt(TwoPi)); // First half of the guassian function. Multiplying by this will normalize the blur colour samples.
	m_blurConstantsBuffer->EndPopulate();

	PB::SamplerDesc blurSamplerDesc{};
	blurSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
	blurSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
	blurSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_BORDER;
	m_blurSampler = m_renderer->GetSampler(blurSamplerDesc);
}

BlurHelper::~BlurHelper()
{
	m_renderer->FreeBuffer(m_blurConstantsBuffer);
}

void BlurHelper::Encode(PB::ICommandContext* cmdContext, PB::Uint2 dstResolution, const BlurImageParams& imageParams)
{
	constexpr PB::u32 WorkGroupX = 2;
	constexpr PB::u32 WorkGroupY = 512;

	PB::ResourceView resourceViews[]
	{
		0,					// Src image
		m_blurSampler,		// Src sampler
		0,					// Dst storage
	};

	PB::TextureViewDesc srcViewDesc{};
	srcViewDesc.m_texture = imageParams.m_src;
	srcViewDesc.m_format = imageParams.m_imageFormat;
	srcViewDesc.m_subresources.m_baseMip = imageParams.m_srcMip;
	srcViewDesc.m_expectedState = PB::ETextureState::SAMPLED;

	PB::TextureViewDesc buffer0ViewDesc{};
	buffer0ViewDesc.m_texture = imageParams.m_buffer0;
	buffer0ViewDesc.m_format = imageParams.m_imageFormat;
	buffer0ViewDesc.m_subresources.m_baseMip = imageParams.m_buffer0Mip;
	buffer0ViewDesc.m_expectedState = PB::ETextureState::STORAGE;

	PB::TextureViewDesc buffer1ViewDesc{};
	buffer1ViewDesc.m_texture = imageParams.m_buffer1;
	buffer1ViewDesc.m_format = imageParams.m_imageFormat;
	buffer1ViewDesc.m_subresources.m_baseMip = imageParams.m_buffer1Mip;
	buffer1ViewDesc.m_expectedState = PB::ETextureState::SAMPLED;

	PB::ResourceView srvA = imageParams.m_src->GetView(srcViewDesc);
	PB::ResourceView srvB = imageParams.m_buffer1->GetView(buffer1ViewDesc);

	buffer1ViewDesc.m_expectedState = PB::ETextureState::STORAGE;

	PB::ResourceView sivA = imageParams.m_buffer0->GetViewAsStorageImage(buffer0ViewDesc);
	PB::ResourceView sivB = imageParams.m_buffer1->GetViewAsStorageImage(buffer1ViewDesc);

	PB::UniformBufferView blurConstantsView = m_blurConstantsBuffer->GetViewAsUniformBuffer();
	PB::BindingLayout bindings;
	bindings.m_uniformBufferCount = 1;
	bindings.m_uniformBuffers = &blurConstantsView;
	bindings.m_resourceCount = _countof(resourceViews);
	bindings.m_resourceViews = resourceViews;

	uint32_t kernelSizeMinusOne = m_kernelSize - 1;

	// The blur shader uses some of it's work group invocations to store excess off-edge samples of count: (GaussianKernelSize - 1) * 2.
	// This reduces the amount of pixels each work group writes to by that amount, so we divide our screen resolution by that new amount.
	uint32_t tileDim = WorkGroupY - (2 * kernelSizeMinusOne);
	uint32_t workGroupCountV = (dstResolution.y / tileDim) + (dstResolution.y % tileDim > 0 ? 1 : 0);
	uint32_t workGroupCountH = (dstResolution.x / tileDim) + (dstResolution.x % tileDim > 0 ? 1 : 0);

	PB::ComputePipelineDesc pipelineDesc{};
	pipelineDesc.m_computeModule = m_verticalPassModule;
	PB::Pipeline verticalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
	pipelineDesc.m_computeModule = m_horizontalPassModule;
	PB::Pipeline horizontalBlurPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);

	PB::SubresourceRange subresources{};
	subresources.m_baseMip = imageParams.m_buffer1Mip;

	// Vertical blur pass.
	cmdContext->CmdBindPipeline(verticalBlurPipeline);
	resourceViews[0] = srvA;
	resourceViews[2] = sivB;
	cmdContext->CmdBindResources(bindings);
	cmdContext->CmdDispatch(dstResolution.x / WorkGroupX, workGroupCountV, 1);

	cmdContext->CmdTransitionTexture(imageParams.m_buffer0, PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE, subresources);
	cmdContext->CmdTransitionTexture(imageParams.m_buffer1, PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED, subresources);

	// Horizontal blur pass.
	cmdContext->CmdBindPipeline(horizontalBlurPipeline);
	resourceViews[0] = srvB;
	resourceViews[2] = sivA;
	cmdContext->CmdBindResources(bindings);
	cmdContext->CmdDispatch(workGroupCountH, dstResolution.y / WorkGroupX, 1);

	subresources.m_baseMip = imageParams.m_buffer0Mip;
	cmdContext->CmdTransitionTexture(imageParams.m_buffer0, PB::ETextureState::STORAGE, PB::ETextureState::SAMPLED, subresources);
	subresources.m_baseMip = imageParams.m_buffer1Mip;
	cmdContext->CmdTransitionTexture(imageParams.m_buffer1, PB::ETextureState::SAMPLED, PB::ETextureState::STORAGE, subresources);
}
