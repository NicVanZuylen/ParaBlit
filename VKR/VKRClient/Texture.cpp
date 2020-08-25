#include "Texture.h"
#include <iostream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture(PB::IRenderer* renderer, const char* filePath) 
{
	m_renderer = renderer;
	m_texture = nullptr;
	m_data = nullptr;

	m_width = 0;
	m_height = 0;
	m_channelCount = 0;
	m_ownsTexture = false;

	if (!filePath)
		return;

	// Load image...
	m_data = stbi_load(filePath, &m_width, &m_height, &m_channelCount, STBI_rgb_alpha);

	if(m_data) 
	{
		std::cout << "Successfully loaded image: " << filePath << std::endl;

		auto size = m_width * m_height * 4;

		PB::TextureDesc textureDesc;
		textureDesc.m_data.m_data = m_data;
		textureDesc.m_data.m_format = PB::PB_TEXTURE_FORMAT_R8G8B8A8_UNORM;
		textureDesc.m_data.m_size = size; // TODO: Better format support

		textureDesc.m_initOptions = PB::PB_TEXTURE_INIT_USE_DATA;
		textureDesc.m_usageStates = PB::PB_TEXTURE_STATE_SAMPLED;
		textureDesc.m_width = m_width;
		textureDesc.m_height = m_height;

		m_texture = m_renderer->AllocateTexture(textureDesc);
		m_ownsTexture = true;

		// Data is no longer needed here.
		stbi_image_free(m_data);
	}
	else
	{
		std::cout << "Failed to load image: " << filePath << std::endl;
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