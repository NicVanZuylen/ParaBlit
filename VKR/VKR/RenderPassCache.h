#pragma once
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"
#include "Texture.h"

#include <unordered_map>

namespace PB 
{
	enum EAttachmentUsage : u8
	{
		PB_ATTACHMENT_USAGE_NONE = 0,
		PB_ATTACHMENT_USAGE_COLOR = 1,
		PB_ATTACHMENT_USAGE_DEPTHSTENCIL = 1 << 1,
		PB_ATTACHMENT_USAGE_READ = 1 << 2
	};

	enum EAttachmentAction : u8
	{
		PB_ATTACHMENT_START_ACTION_NONE,
		PB_ATTACHMENT_START_ACTION_CLEAR,
		PB_ATTACHMENT_START_ACTION_LOAD
	};

	struct AttachmentUsageDesc
	{
		EAttachmentUsage m_usage = PB_ATTACHMENT_USAGE_COLOR;
		u8 m_attachmentIdx = -1;
		ETextureFormat m_attachmentFormat = PB_TEXTURE_FORMAT_UNKNOWN;
	};

	struct alignas(sizeof(uint64_t) * 2) SubpassDesc
	{
		AttachmentUsageDesc m_attachments[8];
	};

	struct alignas(sizeof(uint64_t) * 2) AttachmentDesc
	{
		/*EAttachmentUsage m_usage = PB_ATTACHMENT_USAGE_COLOR;
		ETextureState preState = PB_TEXTURE_STATE_RENDERTARGET;
		ETextureState postState = PB_TEXTURE_STATE_RENDERTARGET;*/
		ETextureFormat m_format = PB_TEXTURE_FORMAT_UNKNOWN;
		ETextureState m_expectedState = PB_TEXTURE_STATE_NONE;
		ETextureState m_finalState = PB_TEXTURE_STATE_NONE;
		EAttachmentAction m_loadAction = PB_ATTACHMENT_START_ACTION_NONE;
		bool m_keepContents = true;
		//u16 m_subpasses = 0; // Bit field where bits represent corresponding subpass indices.
		//bool m_tempAttachment = false;
	};

	struct RenderPassDesc
	{
		RenderPassDesc(AttachmentDesc* attachments, SubpassDesc* subpasses, u32 attachmentCount, u32 subpassCount);
		/*u16 m_subpassCount = 0;
		u16 m_attachmentUsageCount = 0;
		u16 m_attachmentCount = 0;
		AttachmentDesc* m_attachments = nullptr;
		AttachmentUsageDesc* m_attachmentUsages = nullptr;*/
		u16 m_attachmentCount = 0;
		u16 m_subpassCount = 0;
		AttachmentDesc* m_attachments = nullptr;
		SubpassDesc* m_subpasses = nullptr;

		bool operator == (const RenderPassDesc& desc) const;
	};

	struct RenderPassHasher
	{
		// Hash function for the map.
		size_t operator()(const RenderPassDesc& desc) const;
	};

	class Device;

	class RenderPassCache
	{
	public:

		RenderPassCache();

		~RenderPassCache();

		PARABLIT_API void Init(Device* device);

		PARABLIT_API void Destroy();

		PARABLIT_API VkRenderPass GetRenderPass(const RenderPassDesc& desc);

	private:

		PARABLIT_API inline VkRenderPass CreateRenderPass(const RenderPassDesc& desc);

		std::unordered_map<RenderPassDesc, VkRenderPass, RenderPassHasher> m_cache;
		Device* m_device = nullptr;
	};
}