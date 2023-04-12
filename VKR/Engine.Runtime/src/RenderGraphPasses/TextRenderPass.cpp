#include "TextRenderPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Resource/FontTexture.h"

#pragma warning(push, 0)
#define GLM_FORCE_CTOR_INIT // Required to ensure glm constructors actually initialize vectors/matrices etc.
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/packing.hpp"
#include "glm/gtc/type_ptr.hpp"
#pragma warning(pop)

namespace Eng
{
	TextRenderPass::TextRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator) : RenderGraphBehaviour(renderer, allocator)
	{
		m_renderer = renderer;
		m_allocator = allocator;

		PB::BufferObjectDesc charInstanceBufferDesc{};
		charInstanceBufferDesc.m_bufferSize = sizeof(CharInstance) * MaxCharCount;
		charInstanceBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
		m_charInstanceBuffer = m_renderer->AllocateBuffer(charInstanceBufferDesc);

		charInstanceBufferDesc.m_usage = PB::EBufferUsage::COPY_SRC;
		charInstanceBufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE;
		m_localCharInstanceBuffer = m_renderer->AllocateBuffer(charInstanceBufferDesc);
		m_localCharInstanceData = reinterpret_cast<CharInstance*>(m_localCharInstanceBuffer->Map(0, charInstanceBufferDesc.m_bufferSize));

		PB::BufferObjectDesc fontDataBufferDesc{};
		fontDataBufferDesc.m_bufferSize = sizeof(FontData) * 256;
		fontDataBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
		m_fontDataBuffer = m_renderer->AllocateBuffer(fontDataBufferDesc);

		fontDataBufferDesc.m_usage = PB::EBufferUsage::COPY_SRC;
		fontDataBufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE;
		m_localFontDataBuffer = m_renderer->AllocateBuffer(fontDataBufferDesc);
		m_localFontData = reinterpret_cast<FontData*>(m_localFontDataBuffer->Map(0, fontDataBufferDesc.m_bufferSize));

		PB::BufferObjectDesc constantsDesc{};
		constantsDesc.m_bufferSize = sizeof(TextRenderConstants);
		constantsDesc.m_usage = PB::EBufferUsage::UNIFORM | PB::EBufferUsage::COPY_DST;
		constantsDesc.m_options = 0;
		m_textRenderConstants = m_renderer->AllocateBuffer(constantsDesc);

		PB::SamplerDesc fontSamplerDesc{};
		fontSamplerDesc.m_filter = PB::ESamplerFilter::BILINEAR;
		fontSamplerDesc.m_mipFilter = PB::ESamplerFilter::BILINEAR;
		fontSamplerDesc.m_repeatMode = PB::ESamplerRepeatMode::CLAMP_EDGE;
		m_fontSampler = m_renderer->GetSampler(fontSamplerDesc);
	}

	TextRenderPass::~TextRenderPass()
	{
		for (auto& text : m_textAllocations)
			m_allocator->Free(text);
		m_textAllocations.Clear();

		if (m_charInstanceBuffer)
		{
			m_renderer->FreeBuffer(m_charInstanceBuffer);
			m_charInstanceBuffer = nullptr;
		}

		if (m_localCharInstanceBuffer)
		{
			m_localCharInstanceBuffer->Unmap();
			m_renderer->FreeBuffer(m_localCharInstanceBuffer);
			m_localCharInstanceBuffer = nullptr;
		}

		if (m_fontDataBuffer)
		{
			m_renderer->FreeBuffer(m_fontDataBuffer);
			m_fontDataBuffer = nullptr;
		}

		if (m_localFontDataBuffer)
		{
			m_localFontDataBuffer->Unmap();
			m_renderer->FreeBuffer(m_localFontDataBuffer);
			m_localFontDataBuffer = nullptr;
		}

		if (m_textRenderConstants)
		{
			m_renderer->FreeBuffer(m_textRenderConstants);
			m_textRenderConstants = nullptr;
		}
	}

	void TextRenderPass::OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (m_pendingInstanceCopies.Count() > 0)
		{
			info.m_commandContext->CmdCopyBufferToBuffer(m_localCharInstanceBuffer, m_charInstanceBuffer, m_pendingInstanceCopies.Data(), m_pendingInstanceCopies.Count());
			m_pendingInstanceCopies.Clear();
		}

		if (m_pendingFontDataCopies.Count() > 0)
		{
			info.m_commandContext->CmdCopyBufferToBuffer(m_localFontDataBuffer, m_fontDataBuffer, m_pendingFontDataCopies.Data(), m_pendingFontDataCopies.Count());
			m_pendingFontDataCopies.Clear();
		}
	}

	void TextRenderPass::OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		auto renderWidth = m_renderer->GetSwapchain()->GetWidth();
		auto renderHeight = m_renderer->GetSwapchain()->GetHeight();

		PB::GraphicsPipelineDesc textPipelineDesc{};
		textPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::VERTEX] = Eng::Shader(m_renderer, "Shaders/GLSL/vs_char", m_allocator, true).GetModule();
		textPipelineDesc.m_shaderModules[PB::EGraphicsShaderStage::FRAGMENT] = Eng::Shader(m_renderer, "Shaders/GLSL/fs_char", m_allocator, true).GetModule();
		textPipelineDesc.m_attachmentCount = 1;
		textPipelineDesc.m_cullMode = PB::EFaceCullMode::FRONT;
		textPipelineDesc.m_depthCompareOP = PB::ECompareOP::ALWAYS;
		textPipelineDesc.m_depthWriteEnable = false;
		textPipelineDesc.m_stencilTestEnable = false;
		textPipelineDesc.m_renderArea = { 0, 0, 0, 0 };
		textPipelineDesc.m_renderPass = info.m_renderPass;

		auto& blendState = textPipelineDesc.m_colorBlendStates[0];
		blendState = PB::GraphicsPipelineDesc::DefaultBlendState();
		blendState.m_srcColor = PB::EBlendFactor::SRC_ALPHA;
		blendState.m_dstColor = PB::EBlendFactor::ONE_MINUS_SRC_ALPHA;

		PB::Pipeline textPipeline = m_renderer->GetPipelineCache()->GetPipeline(textPipelineDesc);

		info.m_commandContext->CmdBindPipeline(textPipeline);

		PB::ResourceView textResources[]
		{
			m_charInstanceBuffer->GetViewAsStorageBuffer(),
			m_fontDataBuffer->GetViewAsStorageBuffer(),
			m_fontSampler
		};

		PB::UniformBufferView constantsView = m_textRenderConstants->GetViewAsUniformBuffer();

		PB::BindingLayout bindings{};
		bindings.m_uniformBufferCount = 1;
		bindings.m_uniformBuffers = &constantsView;
		bindings.m_resourceCount = _countof(textResources);
		bindings.m_resourceViews = textResources;

		info.m_commandContext->CmdBindResources(bindings);
		info.m_commandContext->CmdSetViewport({ 0, 0, renderWidth, renderHeight }, 0.0f, 1.0f);
		info.m_commandContext->CmdSetScissor({ 0, 0, renderWidth, renderHeight });
		info.m_commandContext->CmdDraw(6, m_charBufEnd);
	}

	void TextRenderPass::OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures)
	{
		if (!m_outputTexture)
			return;

		// Transition color and output to correct layouts...
		info.m_commandContext->CmdTransitionTexture(transientTextures[0], PB::ETextureState::COLORTARGET, PB::ETextureState::COPY_SRC);
		info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::PRESENT, PB::ETextureState::COPY_DST);

		// Copy color to output.
		info.m_commandContext->CmdCopyTextureToTexture(transientTextures[0], m_outputTexture);

		// Transition output texture to present.
		info.m_commandContext->CmdTransitionTexture(m_outputTexture, PB::ETextureState::COPY_DST, PB::ETextureState::PRESENT);
	}

	void TextRenderPass::AddToRenderGraph(RenderGraphBuilder* builder)
	{
		NodeDesc nodeDesc{};
		nodeDesc.m_behaviour = this;
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
		targetDesc.m_flags = EAttachmentFlags::COPY_SRC;

		// Declaring this should allow the texture to be used as a copy source after rendering.
		TransientTextureDesc& targetReadDesc = nodeDesc.m_transientTextures.PushBackInit();
		targetReadDesc.m_format = m_renderer->GetSwapchain()->GetImageFormat();
		targetReadDesc.m_width = nodeDesc.m_renderWidth;
		targetReadDesc.m_height = nodeDesc.m_renderHeight;
		targetReadDesc.m_name = "MergedOutput";
		targetReadDesc.m_mipCount = 1;
		targetReadDesc.m_initialUsage = PB::ETextureState::COLORTARGET;
		targetReadDesc.m_finalUsage = PB::ETextureState::COPY_SRC;
		targetReadDesc.m_usageFlags = PB::ETextureState::COLORTARGET | PB::ETextureState::COPY_SRC;

		builder->AddNode(nodeDesc);

		TextRenderConstants* constantsData = reinterpret_cast<TextRenderConstants*>(m_textRenderConstants->BeginPopulate());

		float w = static_cast<float>(nodeDesc.m_renderWidth);
		float h = static_cast<float>(nodeDesc.m_renderHeight);

		glm::mat4 projMat = glm::ortho(0.0f, w, 0.0f, h);
		memcpy(constantsData->m_proj, glm::value_ptr(projMat), sizeof(PB::Float4) * 4);

		constantsData->m_renderDimensions = PB::Float2(float(nodeDesc.m_renderWidth), float(nodeDesc.m_renderHeight));
		m_textRenderConstants->EndPopulate();
	}

	TextRenderPass::TextHandle TextRenderPass::AddText(const char* string, Eng::FontTexture* font, PB::Float2 linePosition)
	{
		uint32_t charCount = uint32_t(strlen(string));

		// Set up font data...
		uint32_t fontIdx;
		auto fontIt = m_fonts.find(font);
		if (fontIt == m_fonts.end())
		{
			fontIdx = m_fontBufEnd++;

			FontData& fontData = m_localFontData[fontIdx];
			fontData.m_fontColor = { 1.0f, 0.0f, 0.0f, 1.0f };
			fontData.m_fontTextureView = font->GetFontTexture()->GetDefaultSRV();
			fontData.m_glyphBufferView = font->GetGlyphBuffer()->GetViewAsStorageBuffer();
			fontData.m_invTextureResolution = PB::Float2(1.0f / float(font->GetWidth()), 1.0f / float(font->GetHeight()));

			PB::CopyRegion& copyRegion = m_pendingFontDataCopies.PushBack();
			copyRegion.m_srcOffset = fontIdx * sizeof(FontData);
			copyRegion.m_dstOffset = copyRegion.m_srcOffset;
			copyRegion.m_size = sizeof(FontData);

			m_fonts.insert({ font, fontIdx });
		}
		else
		{
			fontIdx = fontIt->second;
		}

		// Make local & device allocations for text...
		Text* newText = m_textAllocations.PushBack() = m_allocator->Alloc<Text>();
		newText->m_fontTexture = font;
		newText->m_totalRegionSize = charCount;
		uint32_t offset = m_charBufEnd;
		m_charBufEnd += charCount;

		Text::CharRegion& newRegion = newText->m_instanceRegions.PushBack();
		newRegion.m_start = offset;
		newRegion.m_end = newRegion.m_start + charCount;

		// Generate char data locally and schedule a copy to the device.
		glm::vec2 curCharPos(linePosition.x, -linePosition.y);
		for (uint32_t i = 0; i < charCount; ++i)
		{
			Eng::FontTexture::GlyphData charGlyphData = font->GetGlyphData(string[i]);

			glm::vec2 halfDim(charGlyphData.m_widthPix * 0.5f, charGlyphData.m_heightPix * 0.5f);
			glm::vec2 localCharPos(charGlyphData.m_offsetXPix + charGlyphData.m_widthPix, charGlyphData.m_heightPix + charGlyphData.m_offsetYPix);
			localCharPos -= halfDim;

			CharInstance& instance = m_localCharInstanceData[newRegion.m_start + i];
			instance.m_char = string[i];
			instance.m_font = fontIdx;
			instance.m_packedPosition = glm::packHalf2x16(curCharPos + localCharPos);

			curCharPos.x += charGlyphData.m_advancePix;
		}

		PB::CopyRegion& copyRegion = m_pendingInstanceCopies.PushBack();
		copyRegion.m_srcOffset = offset * sizeof(CharInstance);
		copyRegion.m_dstOffset = copyRegion.m_srcOffset;
		copyRegion.m_size = sizeof(CharInstance) * charCount;

		return newText;
	}

	void TextRenderPass::TextReplace(TextHandle textHandle, const char* string, PB::Float2 linePosition)
	{
		uint32_t newCharCount = uint32_t(strlen(string));
		Text& text = *reinterpret_cast<Text*>(textHandle);

		auto it = m_fonts.find(text.m_fontTexture);
		assert(it != m_fonts.end());
		uint32_t fontIdx = it->second;

		if (text.m_totalRegionSize < newCharCount)
		{
			uint32_t newRegionSize = newCharCount - text.m_totalRegionSize;
			Text::CharRegion& newRegion = text.m_instanceRegions.PushBack();
			newRegion.m_start = m_charBufEnd;
			newRegion.m_end = newRegion.m_start + newRegionSize;

			assert(newRegion.m_start + newCharCount < MaxCharCount);
			m_charBufEnd += newRegionSize;
		}

		uint32_t totalCharCount = 0;
		glm::vec2 curCharPos(linePosition.x, -linePosition.y);
		for (auto& region : text.m_instanceRegions)
		{
			auto regionSize = region.m_end - region.m_start;

			for (uint32_t i = 0; i < regionSize; ++i, ++totalCharCount)
			{
				char c;
				if (totalCharCount >= newCharCount)
				{
					c = ' ';
				}
				else
				{
					c = string[totalCharCount];
				}

				Eng::FontTexture::GlyphData charGlyphData = text.m_fontTexture->GetGlyphData(c);

				glm::vec2 halfDim(charGlyphData.m_widthPix * 0.5f, charGlyphData.m_heightPix * 0.5f);
				glm::vec2 localCharPos(charGlyphData.m_offsetXPix + charGlyphData.m_widthPix, charGlyphData.m_heightPix + charGlyphData.m_offsetYPix);
				localCharPos -= halfDim;

				CharInstance& instance = m_localCharInstanceData[region.m_start + i];
				instance.m_char = c;
				instance.m_font = fontIdx;
				instance.m_packedPosition = glm::packHalf2x16(curCharPos + localCharPos);

				curCharPos.x += charGlyphData.m_advancePix;
			}

			PB::CopyRegion& copyRegion = m_pendingInstanceCopies.PushBack();
			copyRegion.m_srcOffset = region.m_start * sizeof(CharInstance);
			copyRegion.m_dstOffset = copyRegion.m_srcOffset;
			copyRegion.m_size = sizeof(CharInstance) * regionSize;
		}
		text.m_totalRegionSize = totalCharCount;
	}

	void TextRenderPass::SetOutputTexture(PB::ITexture* tex)
	{
		m_outputTexture = tex;
	}
};