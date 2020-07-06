#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	struct AttachmentUsageDesc
	{
		EAttachmentUsage m_usage = PB_ATTACHMENT_USAGE_NONE;
		u8 m_attachmentIdx = -1;
		ETextureFormat m_attachmentFormat = PB_TEXTURE_FORMAT_UNKNOWN;
	};

	struct SubpassDesc
	{
		AttachmentUsageDesc m_attachments[8];
	};

	struct AttachmentDesc
	{
		ETextureFormat m_format = PB_TEXTURE_FORMAT_UNKNOWN;
		ETextureState m_expectedState = PB_TEXTURE_STATE_NONE;
		ETextureState m_finalState = PB_TEXTURE_STATE_NONE;
		EAttachmentAction m_loadAction = PB_ATTACHMENT_START_ACTION_NONE;
		bool m_keepContents = true;
		u8 m_pad[7];
	};

	struct RenderPassDesc
	{
		u16 m_attachmentCount = 0;
		u16 m_subpassCount = 0;
		AttachmentDesc* m_attachments = nullptr;
		SubpassDesc* m_subpasses = nullptr;

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