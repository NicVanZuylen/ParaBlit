#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"
#include "Engine.AssetPipeline/TextureShared.h"

namespace CLib
{
	class Allocator;
}

namespace Eng
{
	class Texture
	{
	public:

		static AssetEncoder::AssetBinaryDatabaseReader s_textureDatabaseLoader;

		/*
		Constructor: Construct as a texture loaded from an image file.
		Param:
			PB::IRenderer* renderer: The renderer this texture will by use.
			const char* filePath: File path of the image to load.
		*/
		Texture(PB::IRenderer* renderer, CLib::Allocator* allocator, const char* filePath, bool srgb = true, bool convertToCube = false, bool loadFromDatabase = false);

		Texture(PB::IRenderer* renderer);

		/*
		Constructor: Construct as a cube map where each faces is loaded from a separate file.
		Param:
			PB::IRenderer* renderer: The renderer this texture will by use.
			const char** filePaths: Array of file paths for each cube face.
		*/
		Texture(PB::IRenderer* renderer, CLib::Allocator* allocator, const char** filePaths, bool srgb = true, PB::u32 mipCount = 1);

		Texture(PB::IRenderer* renderer, CLib::Allocator* allocator, const char* filePath, AssetPipeline::EConvolutedMapType mapType);

		~Texture();

		/*
		Description: Load and initialize a texture from a texture database.
		Param:
			AssetEncoder::AssetID: The asset ID used for loading the texture.
			AssetEncoder::AssetBinaryDatabaseReader* reader: Database reader the texture will be loaded using.
		*/
		void Load2D(AssetEncoder::AssetID assetID, AssetEncoder::AssetBinaryDatabaseReader* reader = &s_textureDatabaseLoader);

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

		uint32_t GetMipCount() const { return m_mipCount; }

		bool IsCompressed() const { return m_isCompressed; }

	protected:

		void ConvertToCube(bool isHdr);

		PB::IRenderer* m_renderer = nullptr;
		CLib::Allocator* m_allocator = nullptr;
		PB::ITexture* m_texture = nullptr;
		PB::ITexture* m_cubeGenSrcTexture = nullptr;
		PB::IBufferObject* m_cubeGenConstantsBuffers[6]{};

		int m_width = 0;
		int m_height = 0;
		int m_channelCount = 0;
		uint32_t m_mipCount = 1;
		bool m_ownsTexture = false;
		bool m_isCompressed = false;
	};
}