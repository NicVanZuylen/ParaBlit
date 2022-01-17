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
		Texture(PB::IRenderer* renderer, const char* filePath, bool srgb = true);

		/*
		Constructor: Construct as a cube map where each faces is loaded from a separate file.
		Param:
			PB::IRenderer* renderer: The renderer this texture will by use.
			const char** filePaths: Array of file paths for each cube face.
		*/
		Texture(PB::IRenderer* renderer, const char** filePaths, bool srgb = true, PB::u32 mipCount = 1);

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

		PB::IRenderer* m_renderer = nullptr;
		PB::ITexture* m_texture = nullptr;

		int m_width = 0;
		int m_height = 0;
		int m_channelCount = 0;
		bool m_ownsTexture = false;
	};
}