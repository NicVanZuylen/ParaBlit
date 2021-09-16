#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	class Device;

	struct FramebufferDesc
	{
		RenderPass m_renderPass = nullptr;
		u32 m_width = 0;
		u32 m_height = 0;
		RenderTargetView m_attachmentViews[16]{};

		bool operator == (const FramebufferDesc& other) const;
	};

	class IFramebufferCache
	{
	public:

		PARABLIT_INTERFACE Framebuffer GetFramebuffer(const FramebufferDesc& desc) = 0;
	};
}