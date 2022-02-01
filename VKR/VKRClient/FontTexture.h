#pragma once
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

		FontTexture(PB::IRenderer* renderer, const char* ttfPath);

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

	private:

		void GenerateFontData(const char* ttfPath);

		PB::IRenderer* m_renderer;
		PB::ITexture* m_fontTexture = nullptr;
		GlyphData* m_glyphData = nullptr;
		PB::IBufferObject* m_charDataBuffer = nullptr;
	};
}