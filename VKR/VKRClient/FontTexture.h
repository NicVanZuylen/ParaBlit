#pragma once
#include "IRenderer.h"

namespace PBClient
{
	class FontTexture
	{
	public:

		FontTexture(PB::IRenderer* renderer, const char* ttfPath);

		~FontTexture();

	private:

		void GenerateFontData(const char* ttfPath);

		PB::IRenderer* m_renderer;
		PB::ITexture* m_fontTexture = nullptr;
		PB::IBufferObject* m_charDataBuffer = nullptr;
	};
}