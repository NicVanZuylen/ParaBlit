#include "RenderPassCache.h"
#include "MurmurHash3.h"
#include "CLib/Vector.h"
#include "PBUtil.h"
#include "ParaBlitDebug.h"
#include "Device.h"

namespace PB
{
	bool RenderPassDesc::operator==(const RenderPassDesc& desc) const
	{
		return !(m_attachmentCount != desc.m_attachmentCount || m_subpassCount != desc.m_subpassCount
			|| memcmp(m_attachments, desc.m_attachments, sizeof(m_attachments)) != 0
			|| memcmp(m_subpasses, desc.m_subpasses, sizeof(m_subpasses)) != 0)
			|| m_isDynamic != desc.m_isDynamic;
	}

	size_t RenderPassHasher::operator()(const RenderPassDesc& desc) const
	{
		PB_STATIC_ASSERT(sizeof(AttachmentDesc) % 16 == 0, "AttachmentDesc does not meet optimal alignment requirements for hashing.");
		PB_STATIC_ASSERT(sizeof(SubpassDesc) % 16 == 0, "SubpassDesc does not meet optimal alignment requirements for hashing.");

		auto attachmentHash = MurmurHash3_x64_64(desc.m_attachments, sizeof(AttachmentDesc) * desc.m_attachmentCount, 0);
		return MurmurHash3_x64_64(desc.m_subpasses, sizeof(SubpassDesc) * desc.m_subpassCount, attachmentHash);
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
		{
			if (pass.first.m_isDynamic)
				delete reinterpret_cast<DynamicRenderPass*>(pass.second);
			else
				vkDestroyRenderPass(m_device->GetHandle(), reinterpret_cast<VkRenderPass>(pass.second), nullptr);
		}
		m_cache.clear();
	}

	RenderPass RenderPassCache::GetRenderPass(const RenderPassDesc& desc)
	{
		// Look for the render pass in the map using the desc as the key. If a matching render pass is not found, create a new one.
		auto it = m_cache.find(desc);
		if (it == m_cache.end())
		{
			void* newPass = desc.m_isDynamic ? (void*)CreateRenderPassDynamic(desc) : (void*)CreateRenderPass(desc);
			if (newPass != VK_NULL_HANDLE)
			{
				std::pair<RenderPassDesc, void*> newPair{ desc, newPass };
				m_cache.insert(newPair);
			}
			return newPass;
		}
		else
			return it->second;
	}

	inline void InitializeAttachmentDesc(VkAttachmentDescription& desc, const AttachmentDesc& pbDesc)
	{
		PB_ASSERT(pbDesc.m_expectedState != ETextureState::NONE);
		PB_ASSERT(pbDesc.m_finalState != ETextureState::NONE);
		PB_ASSERT(pbDesc.m_format != ETextureFormat::UNKNOWN);

		desc.initialLayout = ConvertPBStateToImageLayout(pbDesc.m_expectedState);
		desc.finalLayout = ConvertPBStateToImageLayout(pbDesc.m_finalState);
		desc.format = ConvertPBFormatToVkFormat(pbDesc.m_format);
		switch (pbDesc.m_loadAction)
		{
		case EAttachmentAction::NONE:
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			break;
		case EAttachmentAction::CLEAR:
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			break;
		case EAttachmentAction::LOAD:
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			break;
		default:
			PB_ASSERT_MSG(false, "Invalid attachment format provided.");
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			break;
		}
		desc.storeOp = pbDesc.m_keepContents ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.stencilLoadOp = desc.loadOp;
		desc.stencilStoreOp = desc.storeOp;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.flags = 0;
	}

	inline VkRenderPass RenderPassCache::CreateRenderPass(const RenderPassDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_attachmentCount > 0 && desc.m_attachmentCount <= (desc.m_subpassCount * 8), "Too little or too many attachments specified.");
		PB_ASSERT_MSG(desc.m_subpassCount > 0, "Too little or too many subpasses specified.");
		PB_ASSERT(desc.m_attachments);
		PB_ASSERT(desc.m_subpasses);

		if (!desc.m_subpasses || !desc.m_attachments)
			return VK_NULL_HANDLE;

		constexpr u32 softAttachmentLimit = 8;
		constexpr u32 softSubpassLimit = 4;

		// TODO: Most of these temporary arrays can live permanently as members. Making them members would avoid initialization & allocation costs.

		// Unique attachment info.
		struct AttachmentData
		{
			u8 m_lastSubpass = 255;
			VkPipelineStageFlags m_lastSubpassStage = 0;
		};
		CLib::Vector<VkAttachmentDescription, softAttachmentLimit, softAttachmentLimit> attachmentDescs(desc.m_attachmentCount);
		CLib::Vector<AttachmentData, softAttachmentLimit, softAttachmentLimit> attachmentDatas(desc.m_attachmentCount);

		// Attachment references
		CLib::Vector<VkAttachmentReference, softAttachmentLimit, softAttachmentLimit> colorRefs(softAttachmentLimit);
		CLib::Vector<VkAttachmentReference, softAttachmentLimit, softAttachmentLimit> dsRefs(softAttachmentLimit);
		CLib::Vector<VkAttachmentReference, softAttachmentLimit, softAttachmentLimit> readRefs(softAttachmentLimit);

		// Subpass data
		struct SubpassData
		{
			VkAccessFlags dstAccess;
		};
		CLib::Vector<VkSubpassDescription, softSubpassLimit, softSubpassLimit> subpassDescs(desc.m_subpassCount);
		subpassDescs.SetCount(desc.m_subpassCount);
		CLib::Vector<SubpassData, softSubpassLimit, softSubpassLimit> subpassDatas(desc.m_subpassCount);
		subpassDatas.SetCount(desc.m_subpassCount);

		CLib::Vector<u8, softSubpassLimit, softSubpassLimit> subpassColorCounts(desc.m_subpassCount);
		CLib::Vector<u8, softSubpassLimit, softSubpassLimit> subpassDSCounts(desc.m_subpassCount);
		CLib::Vector<u8, softSubpassLimit, softSubpassLimit> subpassReadCounts(desc.m_subpassCount);

		// Initialize all attachment descs.
		for (u32 i = 0; i < desc.m_attachmentCount; ++i)
		{
			if (i >= attachmentDescs.Count())
				attachmentDescs.PushBack({});
			InitializeAttachmentDesc(attachmentDescs[i], desc.m_attachments[i]);
		}

		CLib::Vector<VkSubpassDependency, softSubpassLimit, softSubpassLimit> totalDeps(desc.m_subpassCount);

		CLib::Vector<VkSubpassDependency, softSubpassLimit, softSubpassLimit> subpassDeps(desc.m_subpassCount);
		CLib::Vector<bool, softSubpassLimit, softSubpassLimit> depsDirty(desc.m_subpassCount);
		for (u32 i = 0; i < desc.m_subpassCount; ++i)
		{
			subpassColorCounts.PushBack(colorRefs.Count());
			subpassDSCounts.PushBack(dsRefs.Count());
			subpassReadCounts.PushBack(readRefs.Count());

			VkPipelineStageFlags subpassDstStages = 0;

			for (u32 j = 0; j < softAttachmentLimit; ++j)
			{
				auto& pbUsage = desc.m_subpasses[i].m_attachments[j];
				if (pbUsage.m_usage == EAttachmentUsage::NONE)
					break;

				PB_ASSERT_MSG(pbUsage.m_attachmentIdx < desc.m_attachmentCount, "Invalid attachment index provided.");

				VkPipelineStageFlags stage = 0; // Pipeline stage this attachment will be used in.
				VkAccessFlags access = 0; // Access to the attachment required for it's usage here.

				VkAttachmentReference newRef;
				newRef.attachment = pbUsage.m_attachmentIdx;
				switch ((EAttachmentUsage)pbUsage.m_usage)
				{
				case EAttachmentUsage::COLOR:
					newRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					colorRefs.PushBack(newRef);
					break;
				case EAttachmentUsage::DEPTHSTENCIL:
					newRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					dsRefs.PushBack(newRef);
					break;
				case EAttachmentUsage::READ_ONLY_DEPTHSTENCIL:
					newRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
					dsRefs.PushBack(newRef);
					break;
				case EAttachmentUsage::READ:
					newRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					access |= VK_ACCESS_SHADER_READ_BIT;
					readRefs.PushBack(newRef);
					break;
				default:
					PB_NOT_IMPLEMENTED;
					break;
				}

				subpassDstStages |= stage;

				auto& attachment = attachmentDatas[pbUsage.m_attachmentIdx];
				// Attachment was used in a previous subpass.
				if (attachment.m_lastSubpass < i && attachment.m_lastSubpassStage > 0)
				{
					depsDirty[attachment.m_lastSubpass] = true;

					auto& dep = subpassDeps[attachment.m_lastSubpass];
					dep.srcSubpass = attachment.m_lastSubpass;
					dep.dstSubpass = i;
					dep.srcAccessMask |= access;
					//dep.dstAccessMask |= access; // ??? TODO: Should we be setting the access to attachments for future subpasses here?
					dep.srcStageMask |= attachment.m_lastSubpassStage;
					dep.dstStageMask |= stage;
					dep.dependencyFlags = 0;

					// dstAccess should be whatever future subpass will require of this one.
					subpassDatas[dep.srcSubpass].dstAccess |= access;
				}
				attachment.m_lastSubpass = i;
				attachment.m_lastSubpassStage = stage;
			}

			// If this is the first subpass we should add the external dependency instead of dependencies on other subpasses.
			if (i == 0)
			{
				VkSubpassDependency dep;
				dep.srcSubpass = VK_SUBPASS_EXTERNAL;
				dep.dstSubpass = i;
				dep.srcAccessMask = 0;
				dep.dstAccessMask = subpassDatas[i].dstAccess;
				dep.srcStageMask = subpassDstStages;
				dep.dstStageMask = subpassDstStages;
				dep.dependencyFlags = 0;

				totalDeps.PushBack(dep);
			}
			else
			{
				// Add any dependencies on previous subpass to the overall dependency list here.
				for (u32 j = 0; j < desc.m_subpassCount; ++j)
				{
					if (depsDirty[j])
					{
						depsDirty[j] = false;
						totalDeps.PushBack(subpassDeps[j]);
						memset(&subpassDeps[j], 0, sizeof(VkSubpassDependency));
					}
				}
			}

			auto& subpassDesc = subpassDescs[i];
			subpassDesc.flags = 0;
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.colorAttachmentCount = colorRefs.Count() - subpassColorCounts[i];
			subpassDesc.inputAttachmentCount = readRefs.Count() - subpassReadCounts[i];
			subpassDesc.preserveAttachmentCount = 0;
			subpassDesc.pColorAttachments = &colorRefs[subpassColorCounts[i]];
			subpassDesc.pDepthStencilAttachment = (dsRefs.Count() > subpassDSCounts[i]) ? &dsRefs[subpassDSCounts[i]] : nullptr;
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
		PB_ERROR_CHECK(vkCreateRenderPass(m_device->GetHandle(), &renderpassInfo, nullptr, &newPass));
		PB_ASSERT(newPass);
		return newPass;
	}

	inline RenderPassCache::DynamicRenderPass* RenderPassCache::CreateRenderPassDynamic(const RenderPassDesc& desc)
	{
		PB_ASSERT_MSG(desc.m_attachmentCount > 0 && desc.m_attachmentCount <= (desc.m_subpassCount * 8), "Too little or too many attachments specified.");
		PB_ASSERT_MSG(desc.m_subpassCount == 1, "Too little or too many subpasses specified.");
		PB_ASSERT(desc.m_attachments);
		PB_ASSERT(desc.m_subpasses);

		if (!desc.m_subpasses || !desc.m_attachments)
			return nullptr;

		DynamicRenderPass* pass = new DynamicRenderPass;
		VkRenderingInfoKHR& renderingInfo = pass->m_renderingInfo;
		VkCommandBufferInheritanceRenderingInfoKHR& inheritanceInfo = pass->m_inheritanceInfo;
		auto& colorAttachmentInfos = pass->m_colorAttachmentInfos;

		bool hasDepthAttachment = false;
		for (u32 i = 0; i < _countof(SubpassDesc::m_attachments); ++i)
		{
			auto& attachUsage = desc.m_subpasses[0].m_attachments[i];
			auto& attach = desc.m_attachments[attachUsage.m_attachmentIdx];

			VkRenderingAttachmentInfoKHR* infoLocation = nullptr;
			if (attachUsage.m_usage == PB::EAttachmentUsage::COLOR)
			{
				pass->m_colorAttachmentFormats.PushBack(ConvertPBFormatToVkFormat(attach.m_format));
				infoLocation = &colorAttachmentInfos.PushBack();
			}
			else if (attachUsage.m_usage == PB::EAttachmentUsage::DEPTHSTENCIL || attachUsage.m_usage == PB::EAttachmentUsage::READ_ONLY_DEPTHSTENCIL)
			{
				inheritanceInfo.depthAttachmentFormat = ConvertPBFormatToVkFormat(attach.m_format);
				infoLocation = &pass->m_depthAttachmentInfo;
				hasDepthAttachment = true;
			}
			else
			{
				break;
			}

			PB_ASSERT(infoLocation);

			VkRenderingAttachmentInfoKHR& attachmentInfo = *infoLocation;
			attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			attachmentInfo.pNext = nullptr;
			attachmentInfo.imageView = VK_NULL_HANDLE;																			// Set at begin pass.
			attachmentInfo.imageLayout = ConvertPBStateToImageLayout(attach.m_expectedState);
			attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE_KHR;
			attachmentInfo.resolveImageView = VK_NULL_HANDLE;
			attachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentInfo.storeOp = attach.m_keepContents ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentInfo.clearValue = {};																					// Set at begin pass.

			switch (attach.m_loadAction)
			{
			case EAttachmentAction::NONE:
				attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				break;
			case EAttachmentAction::CLEAR:
				attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				break;
			case EAttachmentAction::LOAD:
				attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				break;
			default:
				PB_ASSERT_MSG(false, "Invalid attachment format provided.");
				attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				break;
			}
		}

		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
		renderingInfo.pNext = nullptr;
		renderingInfo.flags = 0; // VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR may be set at begin.
		renderingInfo.layerCount = 1;
		renderingInfo.viewMask = 0;
		renderingInfo.renderArea = {}; // Set at begin pass.
		renderingInfo.colorAttachmentCount = colorAttachmentInfos.Count();
		renderingInfo.pColorAttachments = colorAttachmentInfos.Data();
		renderingInfo.pDepthAttachment = hasDepthAttachment ? &pass->m_depthAttachmentInfo : nullptr;
		renderingInfo.pStencilAttachment = nullptr;

		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR;
		inheritanceInfo.pNext = nullptr;
		inheritanceInfo.viewMask = 0;
		inheritanceInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR;
		inheritanceInfo.colorAttachmentCount = pass->m_colorAttachmentFormats.Count();
		inheritanceInfo.pColorAttachmentFormats = pass->m_colorAttachmentFormats.Data();
		inheritanceInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
		inheritanceInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		return pass;
	}
}
