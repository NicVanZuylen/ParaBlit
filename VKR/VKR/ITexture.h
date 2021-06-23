#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"

namespace PB
{
	class ITexture;
	class IRenderer;

	enum class ETextureInitOptions : u32
	{
		PB_TEXTURE_INIT_NONE = 0,				// Simply don't initialize the contents of the texture. 
		PB_TEXTURE_INIT_ZERO_INITIALIZE = 1,	// Initialize with empty data, ideal for render targets and other textures not read before they are written to. This is Probably the fastest initialization option.
		PB_TEXTURE_INIT_USE_DATA = 1 << 1,		// Use user-provided data to fill the texture.
	};
	PB_DEFINE_ENUM_FIELD(TextureInitOptionFlags, ETextureInitOptions, u32)

	inline ETextureInitOptions operator | (ETextureInitOptions a, ETextureInitOptions b)
	{
		return static_cast<ETextureInitOptions>(static_cast<int>(a) | static_cast<int>(b));
	}

	struct TextureDataDesc
	{
		// Not required unless using the intialization option: PB_TEXTURE_INIT_USE_DATA.
		void* m_data = nullptr; 
		u32 m_size = 0;

		// Always required.
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		bool m_aliasOther = false;
		u8 m_pad0 = 0;
	};

	struct TextureDesc
	{
		TextureDataDesc m_data;
		ETextureState m_initialState = ETextureState::NONE;
		TextureStateFlags m_usageStates = ETextureState::NONE;
		TextureInitOptionFlags m_initOptions = ETextureInitOptions::PB_TEXTURE_INIT_NONE;
		u32 m_width = 0;
		u32 m_height = 0;
	};

	struct TextureViewDesc
	{
		ITexture* m_texture = nullptr;							// Can be left as null if the image is never sampled.
		SubresourceRange m_subresources = {};					// Specifies which subresources of the image to include in the view.
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		ETextureState m_expectedState = ETextureState::NONE;
		u64 m_pad0 = 0;
		// TODO: ONGOING: Add additional view parameters as needed.

		bool operator == (const TextureViewDesc& other) const;
	};

	enum class ESamplerFilter : u8
	{
		NEAREST,
		BILINEAR,
	};

	enum class ESamplerRepeatMode : u8
	{
		REPEAT,
		MIRRORED_REPEAT,
		CLAMP_EDGE,
		CLAMP_BORDER,
	};

	enum class ESamplerBorderColor : u8
	{
		WHITE,
		BLACK
	};

	struct SamplerDesc
	{
		ESamplerFilter m_filter = ESamplerFilter::BILINEAR;
		ESamplerFilter m_mipFilter = ESamplerFilter::BILINEAR;
		ESamplerRepeatMode m_repeatMode = ESamplerRepeatMode::REPEAT;
		ESamplerBorderColor m_borderColor = ESamplerBorderColor::BLACK;
		float m_anisotropyLevels = 0.0f;

		bool operator == (const SamplerDesc& other) const;
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

		/*
		Description: Get whether or not the provided texture will be a sufficient base texture for this texture to alias.
		Return Type: bool
		Param:
			ITexture* baseTexture: The base texture to check.
		*/
		PARABLIT_INTERFACE bool CanAlias(ITexture* baseTexture) = 0;

		/*
		Description: Use this texture as an alias of the provided texture, which will share the base texture's memory.
		Param:
			ITexture* baseTexture: The base texture to alias.
		*/
		PARABLIT_INTERFACE void AliasTexture(ITexture* baseTexture) = 0;

		PARABLIT_INTERFACE ResourceView GetDefaultSRV() = 0;

		PARABLIT_INTERFACE TextureView GetDefaultRTV() = 0;

		PARABLIT_INTERFACE ResourceView GetView(TextureViewDesc& viewDesc) = 0;

		PARABLIT_INTERFACE ResourceView GetDefaultSIV() = 0;

		PARABLIT_INTERFACE ResourceView GetViewAsStorageImage(TextureViewDesc& viewDesc) = 0;

		PARABLIT_INTERFACE TextureView GetRenderTargetView(TextureViewDesc& viewDesc) = 0;
	};
}