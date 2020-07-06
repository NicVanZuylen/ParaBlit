#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	enum ETextureInitOptions
	{
		PB_TEXTURE_INIT_NONE = 0,				// Simply don't initialize the contents of the texture. 
		PB_TEXTURE_INIT_ZERO_INITIALIZE = 1,	// Initialize with empty data, ideal for render targets and other textures not read before they are written to. This is Probably the fastest initialization option.
		PB_TEXTURE_INIT_USE_DATA = 1 << 1		// Use user-provided data to fill the texture.
	};

	inline ETextureInitOptions operator | (ETextureInitOptions a, ETextureInitOptions b)
	{
		return static_cast<ETextureInitOptions>(static_cast<int>(a) | static_cast<int>(b));
	}

	struct TextureDataDesc
	{
		// Not required unless using the intialization option: PB_TEXTURE_INIT_USE_DATA.
		void* m_data = nullptr; 
		u16 m_size = 0;

		// Always required.
		ETextureFormat m_format = PB_TEXTURE_FORMAT_UNKNOWN;
	};

	struct TextureDesc
	{
		TextureDataDesc m_data;
		ETextureState m_initialState = PB_TEXTURE_STATE_NONE;
		ETextureStateFlags m_usageStates = PB_TEXTURE_STATE_NONE;
		ETextureInitOptions m_initOptions = PB_TEXTURE_INIT_NONE;
		u32 m_width = 0;
		u32 m_height = 0;
	};

	class IRenderer;

	class ITexture
	{
	public:

		/*
		Description: Create a new texture using a provided renderer and TextureDesc.
		Param:
			IRenderer* renderer: The renderer this texture will belong to and be usable by.
			const TextureDesc& desc: Structure describing the texture's properties.
		*/
		PARABLIT_INTERFACE void Create(IRenderer* renderer, const TextureDesc& desc) = 0;
	};

	PARABLIT_API ITexture* AllocateTexture();

	PARABLIT_API void FreeTexture(ITexture* texture);
}