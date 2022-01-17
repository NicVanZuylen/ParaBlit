#include "Texture.h"
#include <iostream>
#include <string>

#pragma warning(push)
#pragma warning(disable : 26451 6262 26819) // Warnings coming from stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace PBClient
{
	Texture::Texture(PB::IRenderer* renderer, const char* filePath, bool srgb)
	{
		m_renderer = renderer;

		// Load image...
		stbi_uc* data = stbi_load(filePath, &m_width, &m_height, &m_channelCount, STBI_rgb_alpha);

		if (data)
		{
			std::cout << "Texture: Successfully loaded image: " << filePath << std::endl;

			auto size = m_width * m_height * sizeof(uint32_t);

			PB::TextureDataDesc dataDesc{};
			dataDesc.m_data = data;
			dataDesc.m_size = size;

			PB::TextureDesc textureDesc{};
			textureDesc.m_data = &dataDesc;
			textureDesc.m_format = srgb ? PB::ETextureFormat::R8G8B8A8_SRGB : PB::ETextureFormat::R8G8B8A8_UNORM;
			textureDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
			textureDesc.m_usageStates = PB::ETextureState::SAMPLED;
			textureDesc.m_width = m_width;
			textureDesc.m_height = m_height;

			m_texture = m_renderer->AllocateTexture(textureDesc);
			m_ownsTexture = true;

			// Data is no longer needed here.
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Texture: Failed to load image: " << filePath << std::endl;
		}
	}

	Texture::Texture(PB::IRenderer* renderer, const char** filePaths, bool srgb, PB::u32 mipCount)
	{
		m_renderer = renderer;

		// Load image...
		stbi_uc* data[6]
		{
			stbi_load(filePaths[0], &m_width, &m_height, &m_channelCount, STBI_rgb_alpha),
			stbi_load(filePaths[1], &m_width, &m_height, &m_channelCount, STBI_rgb_alpha),
			stbi_load(filePaths[2], &m_width, &m_height, &m_channelCount, STBI_rgb_alpha),
			stbi_load(filePaths[3], &m_width, &m_height, &m_channelCount, STBI_rgb_alpha),
			stbi_load(filePaths[4], &m_width, &m_height, &m_channelCount, STBI_rgb_alpha),
			stbi_load(filePaths[5], &m_width, &m_height, &m_channelCount, STBI_rgb_alpha)
		};

		uint32_t faceSizeBytes = m_width * m_height * sizeof(uint32_t);

		PB::TextureDataDesc dataDescs[6];
		PB::TextureDesc textureDesc{};
		textureDesc.m_dimension = PB::ETextureDimension::DIMENSION_CUBE;
		textureDesc.m_format = srgb ? PB::ETextureFormat::R8G8B8A8_SRGB : PB::ETextureFormat::R8G8B8A8_UNORM;
		textureDesc.m_usageStates = PB::ETextureState::SAMPLED;
		textureDesc.m_width = m_width;
		textureDesc.m_height = m_height;
		textureDesc.m_mipCount = mipCount;

		PB::TextureDataDesc* firstValidFace = nullptr;
		PB::TextureDataDesc* prevValidFace = nullptr;
		for (uint32_t face = 0; face < 6; ++face)
		{
			PB::TextureDataDesc& faceDataDesc = dataDescs[face];

			if (data[face] != nullptr)
			{
				std::cout << "Texture: Successfully loaded image for cube face: " << face << " from: " << filePaths[face] << std::endl;

				faceDataDesc.m_data = data[face];
				faceDataDesc.m_size = faceSizeBytes;
				faceDataDesc.m_arrayLayer = face;

				if (firstValidFace == nullptr)
					firstValidFace = &faceDataDesc;

				if(prevValidFace != nullptr)
					prevValidFace->m_next = &faceDataDesc;
				prevValidFace = &faceDataDesc;
			}
			else
			{
				std::cout << "Texture: Failed to load image for cube face: " << face << " from: " << filePaths[face] << std::endl;
			}
		}
		
		if (firstValidFace)
		{
			textureDesc.m_data = firstValidFace;
			textureDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
			if(mipCount > 1)
				textureDesc.m_initOptions |= PB::ETextureInitOptions::PB_TEXTURE_INIT_GEN_MIPMAPS;
		}
		m_texture = m_renderer->AllocateTexture(textureDesc);
		m_ownsTexture = true;

		for (auto& ptr : data)
		{
			if (ptr)
				stbi_image_free(ptr);
		}
	}

	Texture::~Texture()
	{
		if (m_ownsTexture && m_texture)
			m_renderer->FreeTexture(m_texture);
		m_texture = nullptr;
		m_ownsTexture = false;
	}

	PB::ITexture* Texture::GetTexture()
	{
		return m_texture;
	}

	int Texture::GetWidth() const
	{
		return m_width;
	}

	int Texture::GetHeight() const
	{
		return m_height;
	}
}

#pragma warning(default : 26451 6262 26819) // Warnings coming from stb_image.h
#pragma warning(pop)