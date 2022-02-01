#include "FontTexture.h"
#include "QuickIO.h"
#include "CLib/Allocator.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "glm/glm.hpp"

namespace PBClient
{
	FontTexture::FontTexture(PB::IRenderer* renderer, const char* ttfPath)
	{
		m_renderer = renderer;
		GenerateFontData(ttfPath);
	}

	FontTexture::~FontTexture()
	{
		if (m_charDataBuffer)
			m_renderer->FreeBuffer(m_charDataBuffer);

		if (m_fontTexture)
			m_renderer->FreeTexture(m_fontTexture);

		delete[] m_glyphRectData;
	}

	void FontTexture::GenerateFontData(const char* ttfPath)
	{
		uint64_t fileSize = 0;
		char* data = nullptr;
		QIO::Load(ttfPath, &data, &fileSize);

		if (data != nullptr)
		{
			stbtt_fontinfo fontInfo{};

			if (!stbtt_InitFont(&fontInfo, reinterpret_cast<unsigned char*>(data), 0))
			{
				delete[] data;
				return;
			}

			// ---------------------------------------------------------------------
			// Generate font texture.

			uint32_t fontHeight = 256;
			int fontScale = (int)fontHeight / 16;

			int textureWidth = glm::clamp<uint32_t>(fontScale * 256, 256, 2048);
			int textureHeight = glm::clamp<uint32_t>(fontScale * 256, 256, 2048);

			stbtt_bakedchar* bakedChars = new stbtt_bakedchar[256];

			// Format will be R8.
			uint32_t textureDataSize = textureWidth * textureHeight;
			uint8_t* fontTextureData = new uint8_t[textureDataSize];

			stbtt_BakeFontBitmap(reinterpret_cast<unsigned char*>(data), 0, static_cast<float>(fontHeight), fontTextureData, textureWidth, textureHeight, 0, 256, bakedChars);
			delete[] data;

			PB::TextureDataDesc dataDesc{};
			dataDesc.m_data = fontTextureData;
			dataDesc.m_size = textureDataSize;

			PB::TextureDesc texDesc{};
			texDesc.m_format = PB::ETextureFormat::R8_UNORM;
			texDesc.m_data = &dataDesc;
			texDesc.m_width = textureWidth;
			texDesc.m_height = textureHeight;
			texDesc.m_usageStates = PB::ETextureState::COPY_DST | PB::ETextureState::SAMPLED;
			texDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
			m_fontTexture = m_renderer->AllocateTexture(texDesc);
			delete[] fontTextureData;

			// ---------------------------------------------------------------------
			// Generate character data.

			m_glyphRectData = new PB::Float4[256];

			PB::BufferObjectDesc bufferDesc{};
			bufferDesc.m_bufferSize = sizeof(PB::Float4) * 256;
			bufferDesc.m_usage = PB::EBufferUsage::COPY_DST | PB::EBufferUsage::STORAGE;
			m_charDataBuffer = m_renderer->AllocateBuffer(bufferDesc);

			PB::Float4* charBufferData = reinterpret_cast<PB::Float4*>(m_charDataBuffer->BeginPopulate());

			for (uint32_t i = 0; i < 256;)
			{
				const stbtt_bakedchar& bakedChar = bakedChars[i];
				PB::Float4& charRect = charBufferData[i];

				charRect =
				{
					(float)bakedChar.x0 / (float)textureWidth,
					(float)bakedChar.y0 / (float)textureHeight,
					(float)bakedChar.x1 / (float)textureWidth,
					(float)bakedChar.y1 / (float)textureHeight
				};
				m_glyphRectData[i] = charRect;
				++i;
			}

			m_charDataBuffer->EndPopulate();

			delete[] bakedChars;
			// ---------------------------------------------------------------------
		}
	}
}