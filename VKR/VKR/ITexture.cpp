#include "ITexture.h"
#include "Texture.h"

namespace PB
{
	// TODO: Support for custom memory allocators to allocate internal ParaBlit classes.
	ITexture* AllocateTexture()
	{
		return reinterpret_cast<ITexture*>(new Texture);
	}

	void FreeTexture(ITexture* texture)
	{
		Texture* internalTex = reinterpret_cast<Texture*>(texture);
		internalTex->Destroy();
		delete internalTex;
	}
}
