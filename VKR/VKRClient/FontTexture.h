#pragma once
#include <cstdint>
#include "IRenderer.h"

namespace PBClient
{
	class FontTexture
	{
	public:

		struct GlyphData
		{
			PB::Float4 m_rect; // Position and size of tex coordinate region of character.
			float m_advancePix; // Advance of X position in pixels before the next character in a line.
			float m_offsetXPix;
			float m_offsetYPix;
			float m_widthPix;
			float m_heightPix;
		};

		FontTexture(PB::IRenderer* renderer, const char* ttfPath, uint32_t fontHeight);

		~FontTexture();

		PB::ITexture* GetFontTexture()
		{
			return m_fontTexture;
		}

		PB::IBufferObject* GetGlyphBuffer()
		{
			return m_charDataBuffer;
		}

		GlyphData GetGlyphData(char c)
		{
			return m_glyphData[c];
		}

		uint32_t GetWidth() { return m_fontTexWidth; }
		uint32_t GetHeight() { return m_fontTexHeight; }
		uint32_t GetFontHeight() { return m_fontHeight; };

	private:

		void GenerateFontData(const char* ttfPath);

		PB::IRenderer* m_renderer;
		PB::ITexture* m_fontTexture = nullptr;
		uint32_t m_fontTexWidth = 0;
		uint32_t m_fontTexHeight = 0;
		uint32_t m_fontHeight = 0;
		GlyphData* m_glyphData = nullptr;
		PB::IBufferObject* m_charDataBuffer = nullptr;
	};
}