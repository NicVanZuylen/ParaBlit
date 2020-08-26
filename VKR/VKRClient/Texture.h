#pragma once
#include "IRenderer.h"

namespace PBClient
{
	class Texture
	{
	public:

		/*
		Constructor: Construct as a texture loaded from an image file.
		Param:
			PB::IRenderer* renderer: The renderer this texture will by use.
			const char* filePath: File path of the image to load.
		*/
		Texture(PB::IRenderer* renderer, const char* filePath);

		~Texture();

		/*
		Description: Get the internal parablit texture.
		*/
		PB::ITexture* GetTexture();

		/*
		Description: Get the width in pixels of the texture.
		Return Type: int
		*/
		int GetWidth() const;

		/*
		Description: Get the height in pixels of the texture.
		Return Type: int
		*/
		int GetHeight() const;

	protected:

		unsigned char* m_data;
		PB::IRenderer* m_renderer;
		PB::ITexture* m_texture;

		int m_width;
		int m_height;
		int m_channelCount;
		bool m_ownsTexture;
	};
}