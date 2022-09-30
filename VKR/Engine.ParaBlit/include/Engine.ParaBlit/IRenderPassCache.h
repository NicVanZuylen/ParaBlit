#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	struct AttachmentUsageDesc
	{
		AttachmentUsageFlags m_usage = EAttachmentUsage::NONE;
		u8 m_attachmentIdx = 0;
		ETextureFormat m_attachmentFormat = ETextureFormat::UNKNOWN;
	};

	struct SubpassDesc
	{
		AttachmentUsageDesc m_attachments[8];
	};

	struct AttachmentDesc
	{
		ETextureFormat m_format = ETextureFormat::UNKNOWN;
		ETextureState m_expectedState = ETextureState::NONE;
		ETextureState m_finalState = ETextureState::NONE;
		EAttachmentAction m_loadAction = EAttachmentAction::NONE;
		bool m_keepContents = true;
		u8 m_pad[8]{};
	};

	struct RenderPassDesc
	{
		u16 m_attachmentCount = 0;
		u16 m_subpassCount = 0;
		AttachmentDesc m_attachments[8];
		SubpassDesc m_subpasses[8];
		bool m_isDynamic = false;

		bool operator == (const RenderPassDesc& desc) const;
	};

	using RenderPass = void*;

	class IRenderPassCache
	{
	public:

		PARABLIT_INTERFACE RenderPass GetRenderPass(const RenderPassDesc& desc) = 0;

	private:

	};
};