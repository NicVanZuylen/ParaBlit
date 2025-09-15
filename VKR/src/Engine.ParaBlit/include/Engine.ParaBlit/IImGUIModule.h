#pragma once
#include "ParaBlitDefs.h"
#include "ParaBlitInterface.h"
#include "ITexture.h"

#include "imgui.h"

namespace PB
{
	class ICommandContext;

	struct ImGuiTextureData
	{
		ImTextureRef ref;
		unsigned int width;
		unsigned int height;
	};

	class IImGUIModule
	{
	public:

		PARABLIT_INTERFACE void RenderDrawData(ImDrawData* drawData, ICommandContext* cmdContext, Pipeline pipeline = 0) = 0;

		PARABLIT_INTERFACE void AddTexture(ImGuiTextureData& outData, ITexture* texture, const TextureViewDesc& viewDesc, const SamplerDesc& samplerDesc) = 0;

		PARABLIT_INTERFACE void RemoveTexture(ImGuiTextureData& data) = 0;
	};
}