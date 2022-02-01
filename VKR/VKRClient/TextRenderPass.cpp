#include "TextRenderPass.h"
#include "RenderGraph.h"
#include "FontTexture.h"

#include <glm/packing.hpp>

TextRenderPass::TextRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
{
	m_renderer = renderer;
	m_allocator = allocator;

	PB::BufferObjectDesc charInstanceBufferDesc{};
	charInstanceBufferDesc.m_bufferSize = sizeof(CharInstance) * 16384;
	charInstanceBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
	m_charInstanceBuffer = m_renderer->AllocateBuffer(charInstanceBufferDesc);

	charInstanceBufferDesc.m_usage = PB::EBufferUsage::COPY_SRC;
	charInstanceBufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE;
	m_localCharInstanceBuffer = m_renderer->AllocateBuffer(charInstanceBufferDesc);
	m_localCharInstanceData = reinterpret_cast<CharInstance*>(m_localCharInstanceBuffer->BeginPopulate());

	PB::BufferObjectDesc fontDataBufferDesc{};
	fontDataBufferDesc.m_bufferSize = sizeof(FontData) * 256;
	fontDataBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
	m_fontDataBuffer = m_renderer->AllocateBuffer(fontDataBufferDesc);
}

TextRenderPass::~TextRenderPass()
{
	if (m_charInstanceBuffer)
	{
		m_renderer->FreeBuffer(m_charInstanceBuffer);
		m_charInstanceBuffer = nullptr;
	}

	if (m_localCharInstanceBuffer)
	{
		m_renderer->FreeBuffer(m_localCharInstanceBuffer);
		m_localCharInstanceBuffer = nullptr;
	}

	if (m_fontDataBuffer)
	{
		m_renderer->FreeBuffer(m_fontDataBuffer);
		m_fontDataBuffer = nullptr;
	}
}

void TextRenderPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void TextRenderPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{
	auto renderWidth = m_renderer->GetSwapchain()->GetWidth();
	auto renderHeight = m_renderer->GetSwapchain()->GetHeight();

	PB::GraphicsPipelineDesc textPipelineDesc{};
	textPipelineDesc.m_attachmentCount = 1;
	textPipelineDesc.m_colorBlendStates[0].m_enableBlending = false;
	textPipelineDesc.m_cullMode = PB::EFaceCullMode::NONE;
	textPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
	textPipelineDesc.m_depthWriteEnable = false;
	textPipelineDesc.m_stencilTestEnable = false;
	textPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
	textPipelineDesc.m_renderPass = info.m_renderPass;

	PB::Pipeline textPipeline = m_renderer->GetPipelineCache()->GetPipeline(textPipelineDesc);

	info.m_commandContext->CmdBindPipeline(textPipeline);

	PB::ResourceView textResources[]
	{
		m_charInstanceBuffer->GetViewAsStorageBuffer(),
		m_fontDataBuffer->GetViewAsStorageBuffer()
	};

	PB::BindingLayout bindings{};
	bindings.m_uniformBufferCount = 0;
	bindings.m_uniformBuffers = nullptr;
	bindings.m_resourceCount = _countof(textResources);
	bindings.m_resourceViews = textResources;

	info.m_commandContext->CmdBindResources(bindings);
	info.m_commandContext->SetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
	info.m_commandContext->SetScissor({ 0, 0, renderWidth, renderHeight });
	info.m_commandContext->CmdDraw(6, 1);
}

void TextRenderPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
{

}

void TextRenderPass::AddToRenderGraph(RenderGraphBuilder* builder)
{
	NodeDesc nodeDesc{};
	nodeDesc.m_useReusableCommandLists = false;
	nodeDesc.m_computeOnlyPass = false;
	nodeDesc.m_renderWidth = m_renderer->GetSwapchain()->GetWidth();
	nodeDesc.m_renderHeight = m_renderer->GetSwapchain()->GetHeight();

	AttachmentDesc& targetDesc = nodeDesc.m_attachments.PushBackInit();
	targetDesc.m_format = m_renderer->GetSwapchain()->GetImageFormat();
	targetDesc.m_width = nodeDesc.m_renderWidth;
	targetDesc.m_height = nodeDesc.m_renderHeight;
	targetDesc.m_name = "MergedOutput";
	targetDesc.m_usage = PB::EAttachmentUsage::COLOR;

	builder->AddNode(nodeDesc);
}

TextRenderPass::TextHandle TextRenderPass::AddText(const char* string, PBClient::FontTexture* font, PB::Float2 linePosition)
{
	uint32_t charCount = uint32_t(strlen(string));

	// Make local & device allocations for text...
	Text* newText = m_allocator->Alloc<Text>();
	uint64_t offset = m_charBufEnd;
	m_charBufEnd += charCount;

	Text::CharRegion& newRegion = newText->m_instanceRegions.PushBack();
	newRegion.m_start = offset;
	newRegion.m_end = newRegion.m_start + charCount;

	// Generate char data locally and schedule a copy to the device.
	float curXPos = linePosition.x;
	for (uint32_t i = 0; i < charCount; ++i)
	{
		PB::Float4 charRect = font->GetCharRect(string[i]);

		CharInstance& instance = m_localCharInstanceData[newRegion.m_start + i];
		instance.m_char = string[i];
		instance.m_font = 0;
		instance.m_packedPosition = glm::packHalf2x16(glm::vec2(curXPos, linePosition.y + charRect.y));

		curXPos += charRect.x;
	}

	PB::CopyRegion& copyRegion = m_pendingCopies.PushBack();
	copyRegion.m_srcOffset = offset * sizeof(CharInstance);
	copyRegion.m_dstOffset = copyRegion.m_srcOffset;
	copyRegion.m_size = sizeof(CharInstance) * charCount;

	return newText;
}

void TextRenderPass::SetOutputTexture(PB::ITexture* tex)
{
	m_outputTexture = tex;
}
