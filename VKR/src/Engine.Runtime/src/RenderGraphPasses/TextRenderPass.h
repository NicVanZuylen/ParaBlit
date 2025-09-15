#pragma once
#include "RenderGraph/RenderGraphNode.h"
#include "Resource/Shader.h"
#include "Engine.Math/Vector2.h"

#include <unordered_map>

namespace Eng
{
	class FontTexture;

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

		void AddToRenderGraph(RenderGraphBuilder* builder, Math::Vector2u targetResolution);

		TextHandle AddText(const char* string, Eng::FontTexture* font, PB::Float2 linePosition);

		void TextReplace(TextHandle textHandle, const char* string, PB::Float2 linePosition);

		void SetOutputTexture(PB::ITexture* tex);

	private:

		static constexpr const uint32_t MaxCharCount = 16384;

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
			PB::Float2 m_invTextureResolution{};
		};

		struct Text
		{
			struct CharRegion
			{
				uint32_t m_start;
				uint32_t m_end;
			};

			CLib::Vector<CharRegion> m_instanceRegions; // Regions of instance data used by this text. Text may occupy multiple regions to avoid fragmentation in instance data.
			Eng::FontTexture* m_fontTexture = nullptr;
			uint32_t m_totalRegionSize = 0;
		};

		struct TextRenderConstants
		{
			PB::Float4 m_proj[4];
			PB::Float2 m_renderDimensions;
		};

		Math::Vector2u m_targetResolution{};
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
		std::unordered_map<Eng::FontTexture*, PB::u32> m_fonts;
		PB::ITexture* m_outputTexture = nullptr;
	};
};