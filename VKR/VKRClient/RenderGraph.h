#pragma once
#include "IRenderer.h"
#include "ICommandContext.h"
#include "CLib/String.h"

#include "RenderGraphNode.h"

#include <unordered_map>

enum class EAttachmentFlags
{
	NONE = 0,
	COPY_SRC = 1 << 0,
	COPY_DST = 1 << 1
};
using AttachmentFlags = PB::BitField<EAttachmentFlags>;

struct AttachmentDesc
{
	const char* m_name = nullptr;
	uint32_t m_width;
	uint32_t m_height;
	PB::Float4 m_clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	PB::EAttachmentUsage m_usage;
	PB::ETextureFormat m_format;
	AttachmentFlags m_flags = EAttachmentFlags::NONE;
};

struct NodeDesc
{
	RenderGraphBehaviour* m_behaviour = nullptr;
	AttachmentDesc m_attachments[8]{};
	uint32_t m_attachmentCount = 0;
	uint32_t m_renderWidth = 0;
	uint32_t m_renderHeight = 0;
};

class RenderGraph;

// Texture resource used by the render graph. These are shared between passes where possible.
struct RenderGraphTexture
{
	PB::ITexture* m_texture = nullptr;
	PB::TextureDesc m_desc;
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
		PB::ETextureStateFlags m_usage;
		PB::ETextureFormat m_format;
		AttachmentFlags m_flags; 
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

	PB::ITexture* FindTexture(const AttachmentMeta& meta, RenderGraph* graph);

	PB::ITexture* CreateTexture(const AttachmentMeta& meta);

	PB::TextureView GetTextureView(const AttachmentMeta& meta, PB::ETextureState expectedState);

	PB::IRenderer* m_renderer = nullptr;
	CLib::Allocator* m_allocator;

	std::vector<AttachmentMeta> m_freeList;
	std::unordered_map<const char*, NamedAttachmentMeta> m_namedAttachments;

	CLib::Vector<InternalBuildNode> m_buildNodes;
};

// Contains all necessary information to run a single render pass.
struct RenderGraphExecuteNode
{
	PB::ETextureState m_attachmentStates[8]{};
	PB::TextureView m_attachmentViews[8]{};
	PB::ITexture* m_attachments[8]{};
	PB::Float4 m_clearColors[8]{};
	PB::Framebuffer m_framebuffer = nullptr;
	PB::RenderPass m_renderPass = nullptr;
	uint32_t m_attachmentCount = 0;
	uint32_t m_renderWidth = 0;
	uint32_t m_renderHeight = 0;

	RenderGraphBehaviour* m_behaviour = nullptr;
	RenderGraphExecuteNode* m_next = nullptr;
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