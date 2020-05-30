#include "RenderPassCache.h"
#include "MurmurHash3.h"
#include "DynamicArray.h"
#include "PBUtil.h"
#include "ParaBlitDebug.h"
#include "Device.h"

namespace PB
{
	RenderPassDesc::RenderPassDesc(AttachmentDesc* attachments, SubpassDesc* subpasses, u32 attachmentCount, u32 subpassCount)
	{
		m_attachmentCount = attachmentCount;
		m_subpassCount = subpassCount;
		m_attachments = attachments;
		m_subpasses = subpasses;
	}

	bool RenderPassDesc::operator==(const RenderPassDesc& desc) const
	{
		return !(m_attachmentCount != desc.m_attachmentCount || m_subpassCount != desc.m_subpassCount
			|| memcmp(m_attachments, desc.m_attachments, sizeof(AttachmentDesc) * m_attachmentCount) != 0
			|| memcmp(m_subpasses, desc.m_subpasses, sizeof(SubpassDesc) * m_subpassCount) != 0);
	}

	size_t RenderPassHasher::operator()(const RenderPassDesc& desc) const
	{
		auto attachmentHash = MurmurHash3_x64_64(desc.m_attachments, sizeof(AttachmentDesc) * desc.m_attachmentCount, 0);
		auto subpassHash = MurmurHash3_x64_64(desc.m_subpasses, sizeof(SubpassDesc) * desc.m_subpassCount, 0);
		size_t hashes[] = { attachmentHash, subpassHash };
		return MurmurHash3_x64_64(hashes, _countof(hashes) * sizeof(size_t), 0);
	}

	RenderPassCache::RenderPassCache()
	{

	}

	RenderPassCache::~RenderPassCache()
	{

	}

	void RenderPassCache::Init(Device* device)
	{
		m_device = device;
	}

	void RenderPassCache::Destroy()
	{
		for (auto& pass : m_cache)
			vkDestroyRenderPass(m_device->GetHandle(), pass.second, nullptr);
		m_cache.clear();
	}

	VkRenderPass RenderPassCache::GetRenderPass(const RenderPassDesc& desc)
	{
		// Look for the render pass in the map using the desc as the key. If a matching render pass is not found, create a new one.
		auto it = m_cache.find(desc);
		if (it == m_cache.end())
		{
			auto newPass = CreateRenderPass(desc);
			m_cache[desc] = newPass;
			return newPass;
		}
		else
			return it->second;
	}

	inline void InitializeAttachmentDesc(VkAttachmentDescription& desc, const AttachmentDesc& pbDesc)
	{
		desc.initialLayout = ConvertPBStateToImageLayout(pbDesc.m_expectedState);
		desc.finalLayout = ConvertPBStateToImageLayout(pbDesc.m_finalState);
		desc.format = ConvertPBFormatToVkFormat(pbDesc.m_format);
		switch (pbDesc.m_loadAction)
		{
		case PB_ATTACHMENT_START_ACTION_NONE:
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			break;
		case PB_ATTACHMENT_START_ACTION_CLEAR:
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			break;
		case PB_ATTACHMENT_START_ACTION_LOAD:
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			break;
		default:
			PB_NOT_IMPLEMENTED;
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			break;
		}
		desc.storeOp = pbDesc.m_keepContents ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.flags = 0;
	}

	inline VkRenderPass RenderPassCache::CreateRenderPass(const RenderPassDesc& desc)
	{
		PB_ASSERT(desc.m_attachmentCount > 0 && desc.m_attachmentCount <= (desc.m_subpassCount * 8), "Too little or too many attachments specified.");
		PB_ASSERT(desc.m_subpassCount > 0, "Too little or too many subpasses specified.");
		PB_ASSERT(desc.m_attachments);
		PB_ASSERT(desc.m_subpasses);

		constexpr u32 softAttachmentLimit = 8;
		constexpr u32 softSubpassLimit = 4;

		// TODO: Most of these temporary arrays can live permanently as members. Making them members would avoid initialization & allocation costs.

		// Unique attachment info.
		struct AttachmentData
		{
			u8 m_lastSubpass = 255;
			VkPipelineStageFlags m_lastSubpassStage = 0;
		};
		DynamicArray<VkAttachmentDescription, softAttachmentLimit> attachmentDescs(desc.m_attachmentCount, softAttachmentLimit);
		DynamicArray<AttachmentData, softAttachmentLimit> attachmentDatas(desc.m_attachmentCount, softAttachmentLimit);

		// Attachment references
		DynamicArray<VkAttachmentReference, softAttachmentLimit> colorRefs(softAttachmentLimit, softAttachmentLimit);
		DynamicArray<VkAttachmentReference, softAttachmentLimit> dsRefs(softAttachmentLimit, softAttachmentLimit);
		DynamicArray<VkAttachmentReference, softAttachmentLimit> readRefs(softAttachmentLimit, softAttachmentLimit);

		// Subpass data
		struct SubpassData
		{
			VkAccessFlags dstAccess;
		};
		DynamicArray<VkSubpassDescription, softSubpassLimit> subpassDescs(desc.m_subpassCount, softSubpassLimit);
		DynamicArray<SubpassData, softSubpassLimit> subpassDatas(desc.m_subpassCount, softSubpassLimit);

		DynamicArray<u8, softSubpassLimit> subpassColorCounts(desc.m_subpassCount, softSubpassLimit);
		DynamicArray<u8, softSubpassLimit> subpassDSCounts(desc.m_subpassCount, softSubpassLimit);
		DynamicArray<u8, softSubpassLimit> subpassReadCounts(desc.m_subpassCount, softSubpassLimit);

		// Initialize all attachment descs.
		for (u32 i = 0; i < desc.m_attachmentCount; ++i)
		{
			if (i >= attachmentDescs.Count())
				attachmentDescs.Push({});
			InitializeAttachmentDesc(attachmentDescs[i], desc.m_attachments[i]);
		}

		DynamicArray<VkSubpassDependency, softSubpassLimit> totalDeps(desc.m_subpassCount, softSubpassLimit);

		DynamicArray<VkSubpassDependency, softSubpassLimit> subpassDeps(desc.m_subpassCount, softSubpassLimit);
		DynamicArray<bool, softSubpassLimit> depsDirty(desc.m_subpassCount, softSubpassLimit);
		for (u32 i = 0; i < desc.m_subpassCount; ++i)
		{
			subpassColorCounts.Push(colorRefs.Count());
			subpassDSCounts.Push(dsRefs.Count());
			subpassReadCounts.Push(readRefs.Count());

			for (u32 j = 0; j < softAttachmentLimit; ++j)
			{
				auto& pbUsage = desc.m_subpasses[i].m_attachments[j];

				if (pbUsage.m_attachmentFormat == PB_TEXTURE_FORMAT_UNKNOWN)
					break;

				VkPipelineStageFlags stage = 0; // Pipeline stage this attachment will be used in.
				VkAccessFlags access = 0; // Access to the attachment required for it's usage here.

				VkAttachmentReference newRef;
				newRef.attachment = pbUsage.m_attachmentIdx;
				switch (pbUsage.m_usage)
				{
				case PB_ATTACHMENT_USAGE_COLOR:
					newRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					colorRefs.Push(newRef);
					break;
				case PB_ATTACHMENT_USAGE_DEPTHSTENCIL:
					newRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					dsRefs.Push(newRef);
					break;
				case PB_ATTACHMENT_USAGE_READ:
					newRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					access |= VK_ACCESS_SHADER_READ_BIT;
					readRefs.Push(newRef);
					break;
				default:
					PB_NOT_IMPLEMENTED;
					break;
				}

				// Attachment was used in a previous subpass.
				auto& attachment = attachmentDatas[pbUsage.m_attachmentIdx];
				if (attachment.m_lastSubpass < 255)
				{
					depsDirty[attachment.m_lastSubpass] = true;

					auto& dep = subpassDeps[attachment.m_lastSubpass];
					dep.srcSubpass = attachment.m_lastSubpass;
					dep.dstSubpass = i;
					dep.srcAccessMask |= access;
					//dep.dstAccessMask |= access; // ??? TODO: Should we be setting the access to attachments for future subpasses here?
					dep.srcStageMask |= attachment.m_lastSubpassStage;
					dep.dstStageMask |= stage;

					// dstAccess should be whatever future subpass will require of this one.
					subpassDatas[dep.srcSubpass].dstAccess |= access;
				}
				attachment.m_lastSubpass = i;
				attachment.m_lastSubpassStage = stage;
			}

			// Add any dependencies on previous subpass to the overall dependency list here.
			for (u32 j = 0; j < desc.m_subpassCount; ++j)
			{
				if (depsDirty[j])
				{
					depsDirty[j] = false;
					totalDeps.Push(subpassDeps[j]);
					memset(&subpassDeps[j], 0, sizeof(VkSubpassDependency));
				}
			}

			auto& subpassDesc = subpassDescs[i];
			subpassDesc.flags = 0;
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.colorAttachmentCount = colorRefs.Count() - subpassColorCounts[i];
			subpassDesc.inputAttachmentCount = readRefs.Count() - subpassReadCounts[i];
			subpassDesc.preserveAttachmentCount = 0;
			subpassDesc.pColorAttachments = &colorRefs[subpassColorCounts[i]];
			subpassDesc.pDepthStencilAttachment = &dsRefs[subpassDSCounts[i]];
			subpassDesc.pInputAttachments = &readRefs[subpassReadCounts[i]];
			subpassDesc.pPreserveAttachments = nullptr;
			subpassDesc.pResolveAttachments = nullptr;

			subpassDeps.Clear();
		}

		// Post-process access for future subpasses.
		for (auto& dep : totalDeps)
		{
			dep.dstAccessMask |= subpassDatas[dep.dstSubpass].dstAccess;
		}

		VkRenderPassCreateInfo renderpassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr };
		renderpassInfo.flags = 0;
		renderpassInfo.attachmentCount = attachmentDescs.Count();
		renderpassInfo.pAttachments = attachmentDescs.Data();
		renderpassInfo.dependencyCount = totalDeps.Count();
		renderpassInfo.pDependencies = totalDeps.Data();
		renderpassInfo.subpassCount = subpassDescs.Count();
		renderpassInfo.pSubpasses = subpassDescs.Data();

		VkRenderPass newPass = VK_NULL_HANDLE;
		PB_ERROR_CHECK(vkCreateRenderPass(m_device->GetHandle(), &renderpassInfo, nullptr, &newPass), "Failed to generate VkRenderPass");
		PB_ASSERT(newPass);
		return newPass;
	}
}
