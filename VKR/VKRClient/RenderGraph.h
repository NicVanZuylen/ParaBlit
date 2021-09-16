#pragma once
#include "IRenderer.h"
#include "ICommandContext.h"
#include "CLib/String.h"

#include "RenderGraphNode.h"

#include <map>
#include <unordered_map>

enum class EAttachmentFlags
{
	NONE = 0,
	COPY_SRC = 1 << 0,			// Target will be copied to within the same pass.
	COPY_DST = 1 << 1,			// Target will be copied from within the same pass.
	SECONDARY_SAMPLED = 1 << 2,	// Target will also be sampled from within the same pass.
	SECONDARY_STORAGE = 1 << 3,	// Target will also be used as a storage image within the same pass.
	CLEAR = 1 << 4,				// Target will be cleared at the beginning the first render pass which uses it.
};
PB_DEFINE_ENUM_FIELD(AttachmentFlags, EAttachmentFlags, PB::u32)

struct AttachmentDesc
{
	const char* m_name = nullptr;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	PB::Float4 m_clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	PB::EAttachmentUsage m_usage = PB::EAttachmentUsage::NONE;
	uint8_t m_mipCount = 1;
	PB::ETextureFormat m_format = PB::ETextureFormat::UNKNOWN;
	AttachmentFlags m_flags = EAttachmentFlags::NONE;
};

struct NodeDesc
{
	RenderGraphBehaviour* m_behaviour = nullptr;
	AttachmentDesc m_attachments[8]{};
	uint32_t m_attachmentCount = 0;
	uint32_t m_renderWidth = 0;
	uint32_t m_renderHeight = 0;
	bool m_useReusableCommandLists = false;
	bool m_computeOnlyPass = false;
};

class RenderGraph;

// Texture resource used by the render graph. These are shared between passes where possible.
struct RenderGraphTexture
{
	PB::ITexture* m_texture = nullptr;
	PB::TextureDesc m_desc{};
};

// Contains all necessary information to run a single render pass.
struct RenderGraphExecuteNode
{
	PB::ETextureState m_attachmentPriorStates[8]{};
	PB::ETextureState m_attachmentStates[8]{};
	PB::SubresourceRange m_attachmentSubresourceRanges[8]{};
	PB::RenderTargetView m_attachmentViews[8]{};
	PB::ITexture* m_attachments[8]{};
	PB::Float4 m_clearColors[8]{};
	PB::Framebuffer m_framebuffer = nullptr;
	PB::RenderPass m_renderPass = nullptr;
	uint32_t m_attachmentCount = 0;
	uint32_t m_renderWidth = 0;
	uint32_t m_renderHeight = 0;
	bool m_useReusableCommandLists = false;

	RenderGraphBehaviour* m_behaviour = nullptr;
	RenderGraphExecuteNode* m_next = nullptr;
};


class RenderGraphBuilder
{
public:

	RenderGraphBuilder(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~RenderGraphBuilder();

	void AddNode(const NodeDesc& desc);

	RenderGraph* Build();

private:

	struct AttachmentMeta
	{
		PB::ITexture* m_texture = nullptr;
		const char* m_name = nullptr;
		uint32_t m_width;
		uint32_t m_height;
		PB::TextureStateFlags m_usage;
		PB::ETextureState* m_firstPriorState;
		PB::ETextureState m_lastUsage;
		PB::ETextureFormat m_format;
		AttachmentFlags m_flags; 
		uint32_t m_mipCount;
	};

	struct InternalBuildNode
	{
		AttachmentMeta m_attachments[8]{};
		PB::EAttachmentUsage m_usages[8]{};
		PB::Float4 m_clearColors[8]{};
		RenderGraphBehaviour* m_behaviour = nullptr;
		uint32_t m_attachmentCount = 0;
		uint32_t m_renderWidth = 0;
		uint32_t m_renderHeight = 0;
		bool m_useReusableCommandLists = false;
		bool m_computeOnlyPass = false;
	};

	// Tracks the usage of named attachments
	struct NamedAttachmentMeta
	{
		CLib::String<8, char> m_name;
		uint32_t m_earliestTimePoint = ~0u;
		uint32_t m_mostRecentTimepoint = ~0u;
		
		AttachmentMeta m_meta;
	};

	void TextureDescFromAttachmentMeta(const AttachmentMeta& meta, PB::TextureDesc& outDesc);

	void UpdateNamedAttachment(const AttachmentDesc& desc, uint32_t timePoint);

	void RecordInitialTransitions();

	size_t GetSizeFromAttachMeta(const AttachmentMeta& meta);

	PB::ITexture* FindTexture(const AttachmentMeta& meta, RenderGraph* graph);

	PB::ITexture* CreateTexture(const AttachmentMeta& meta);

	PB::RenderTargetView GetTextureView(const AttachmentMeta& meta, PB::ETextureState expectedState);

	PB::IRenderer* m_renderer = nullptr;
	CLib::Allocator* m_allocator;

	std::map<size_t, AttachmentMeta> m_freeList;
	std::unordered_map<const char*, NamedAttachmentMeta> m_namedAttachments;

	CLib::Vector<InternalBuildNode> m_buildNodes;
};

class RenderGraph
{
public:

	RenderGraph() = default;

	~RenderGraph();

	void Execute();

private:

	friend class RenderGraphBuilder;

	CLib::Vector<PB::ITexture*> m_baseTextures;
	CLib::Vector<PB::ITexture*> m_aliasTextures;
	RenderGraphInfo m_passInfo;

	RenderGraphExecuteNode* m_start = nullptr;
};