#pragma once
#include "RenderGraphNode.h"
#include "Shader.h"

#include <unordered_map>

namespace PBClient
{
	class FontTexture;
}

class RenderGraphBuilder;

class TextRenderPass : public RenderGraphBehaviour
{
public:

	using TextHandle = void*;

	TextRenderPass(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~TextRenderPass();

	void OnPrePass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPassBegin(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void OnPostPass(const RenderGraphInfo& info, PB::RenderTargetView* renderTargetViews, PB::ITexture** transientTextures) override;

	void AddToRenderGraph(RenderGraphBuilder* builder);

	TextHandle AddText(const char* string, PBClient::FontTexture* font, PB::Float2 linePosition);

	void SetOutputTexture(PB::ITexture* tex);

private:

	struct CharInstance
	{
		uint16_t m_char; // ASCII index of the character, used to select the glyph info from the glyph buffer.
		uint16_t m_font; // Used to select which font and subsequent glyph buffer to use.
		uint32_t m_packedPosition; // Two half precision float values for screen-space coordinates;
	};

	struct FontData
	{
		PB::Float4 m_fontColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		PB::ResourceView m_fontTextureView = 0;
		PB::ResourceView m_glyphBufferView = 0;
		PB::u32 m_pad[2];
	};

	struct Text
	{
		struct CharRegion
		{
			uint32_t m_start;
			uint32_t m_end;
		};

		CLib::Vector<CharRegion> m_instanceRegions; // Regions of instance data used by this text. Text may occupy multiple regions to avoid fragmentation in instance data.
	};

	struct TextRenderConstants
	{
		PB::Float2 m_renderDimensions;
	};

	PB::IBufferObject* m_charInstanceBuffer = nullptr;
	PB::IBufferObject* m_localCharInstanceBuffer = nullptr;
	PB::IBufferObject* m_fontDataBuffer = nullptr;
	PB::IBufferObject* m_localFontDataBuffer = nullptr;
	PB::IBufferObject* m_textRenderConstants = nullptr;
	PB::ResourceView m_fontSampler = 0;
	CharInstance* m_localCharInstanceData = nullptr;
	FontData* m_localFontData = nullptr;
	uint32_t m_charBufEnd = 0;
	uint32_t m_fontBufEnd = 0;
	CLib::Vector<PB::CopyRegion, 16, 16> m_pendingInstanceCopies{};
	CLib::Vector<PB::CopyRegion, 16, 16> m_pendingFontDataCopies{};
	CLib::Vector<Text*, 16, 16> m_textAllocations;
	std::unordered_map<PBClient::FontTexture*, PB::u32> m_fonts;
	PB::ITexture* m_outputTexture = nullptr;
};