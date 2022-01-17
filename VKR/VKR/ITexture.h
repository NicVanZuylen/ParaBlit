#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"

namespace PB
{
	class ITexture;
	class IRenderer;

	enum class ETextureDimension : u16
	{
		DIMENSION_NONE,
		DIMENSION_1D,
		DIMENSION_2D,
		DIMENSION_3D,
		DIMENSION_CUBE
	};

	enum class ETextureInitOptions : u16
	{
		PB_TEXTURE_INIT_NONE = 0,				// Simply don't initialize the contents of the texture. 
		PB_TEXTURE_INIT_ZERO_INITIALIZE = 1,	// Initialize with empty data, ideal for render targets and other textures not read before they are written to. This is Probably the fastest initialization option.
		PB_TEXTURE_INIT_USE_DATA = 1 << 1,		// Use user-provided data to fill the texture.
		PB_TEXTURE_INIT_GEN_MIPMAPS = 1 << 2,	// Use the graphics API to automatically generate mipmap contents for this texture.
	};
	PB_DEFINE_ENUM_FIELD(TextureInitOptionFlags, ETextureInitOptions, u16)

	inline ETextureInitOptions operator | (ETextureInitOptions a, ETextureInitOptions b)
	{
		return static_cast<ETextureInitOptions>(static_cast<int>(a) | static_cast<int>(b));
	}

	struct TextureDataDesc
	{
		void* m_data = nullptr;
		u32 m_size = 0;
		u16 m_mipLevel = 0;
		u16 m_arrayLayer = 0;
		TextureDataDesc* m_next = nullptr;
	};

	struct TextureDesc
	{
		TextureDataDesc* m_data = nullptr; // Not required unless created with init option: PB_TEXTURE_INIT_USE_DATA.
		ETextureState m_initialState = ETextureState::NONE;
		TextureStateFlags m_usageStates = ETextureState::NONE;
		ETextureDimension m_dimension = ETextureDimension::DIMENSION_2D;
		TextureInitOptionFlags m_initOptions = ETextureInitOptions::PB_TEXTURE_INIT_NONE;
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		bool m_aliasOther = false;
		u32 m_width = 0;
		u32 m_height = 0;
		u32 m_depth = 1;
		u32 m_mipCount = 1;
	};

	enum class ETextureViewType : u32
	{
		VIEW_TYPE_1D,
		VIEW_TYPE_2D,
		VIEW_TYPE_3D,
		VIEW_TYPE_CUBE
	};

	struct TextureViewDesc
	{
		ITexture* m_texture = nullptr;							// Can be left as null if the image is never sampled.
		SubresourceRange m_subresources = {};					// Specifies which subresources of the image to include in the view.
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		ETextureState m_expectedState = ETextureState::NONE;
		ETextureViewType m_type = ETextureViewType::VIEW_TYPE_2D;
		u32 m_pad0 = 0;
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
		float minLod = 0.0f;
		float maxLod = 1.0f;

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

		PARABLIT_INTERFACE RenderTargetView GetDefaultRTV() = 0;

		PARABLIT_INTERFACE ResourceView GetView(TextureViewDesc& viewDesc) = 0;

		PARABLIT_INTERFACE ResourceView GetDefaultSIV() = 0;

		PARABLIT_INTERFACE ResourceView GetViewAsStorageImage(TextureViewDesc& viewDesc) = 0;

		PARABLIT_INTERFACE RenderTargetView GetRenderTargetView(TextureViewDesc& viewDesc) = 0;
	};
}