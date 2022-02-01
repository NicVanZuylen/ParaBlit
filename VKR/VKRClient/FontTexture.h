#pragma once
#include "IRenderer.h"

namespace PBClient
{
	class FontTexture
	{
	public:

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

		PB::Float4 GetCharRect(char c)
		{
			return m_glyphRectData[c];
		}

	private:

		void GenerateFontData(const char* ttfPath);

		PB::IRenderer* m_renderer;
		PB::ITexture* m_fontTexture = nullptr;
		PB::Float4* m_glyphRectData = nullptr;
		PB::IBufferObject* m_charDataBuffer = nullptr;
	};
}