#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitInterface.h"

namespace PB
{
	class ITexture;
	class IRenderer;

	struct TextureViewDesc
	{
		ITexture* m_texture = nullptr;
		SubresourceRange m_subresources = {};								// Specifies which subresources of the image to include in the view.
		ETextureFormat m_format = PB_TEXTURE_FORMAT_UNKNOWN;
		u16 m_pad0 = 0;
		IRenderer* m_renderer = nullptr;
		// TODO: ONGOING: Add additional view parameters as needed.

		bool operator == (const TextureViewDesc& other) const;
	};

	using TextureView = void*;

	class ITextureViewCache
	{
	public:

		PARABLIT_INTERFACE TextureView GetView(const TextureViewDesc& desc) = 0;
	};
}