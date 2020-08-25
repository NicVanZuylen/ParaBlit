#pragma once
#include "IRenderer.h"

#ifndef ATTACHMENT_E
#define ATTACHMENT_E

enum ETextureProperties 
{
	TEXTURE_PROPERTIES_NONE = 0,
	TEXTURE_PROPERTIES_INPUT_ATTACHMENT = 1,
	TEXTURE_PROPERTIES_TRANSFER_SRC = 2
};

enum EAttachmentType 
{
	ATTACHMENT_COLOR,
	ATTACHMENT_DEPTH_STENCIL,
	ATTACHMENT_SWAP_CHAIN
};

#endif

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