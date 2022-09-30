#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ICommandContext.h"
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
	uint8_t m_arraySize = 1;
	uint8_t m_renderMip = 0;
	uint8_t m_renderArrayLayer = 0;
	PB::ETextureFormat m_format = PB::ETextureFormat::UNKNOWN;
	AttachmentFlags m_flags = EAttachmentFlags::NONE;
};

struct TransientTextureDesc
{
	const char* m_name = nullptr;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	PB::ETextureFormat m_format = PB::ETextureFormat::UNKNOWN;
	PB::TextureStateFlags m_usageFlags = PB::ETextureState::NONE;
	PB::ETextureState m_initialUsage = PB::ETextureState::NONE;
	PB::ETextureState m_finalUsage = PB::ETextureState::NONE;
	uint8_t m_mipCount = 1;
	uint8_t m_arraySize = 1;
};

struct NodeDesc
{
	RenderGraphBehaviour* m_behaviour = nullptr;
	CLib::Vector<AttachmentDesc, 8> m_attachments{ 8 };
	CLib::Vector<TransientTextureDesc, 8> m_transientTextures{ 8 };
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

struct RTTransitionData
{
	PB::ITexture* m_texture;
	PB::ETextureState m_oldState;
	PB::ETextureState m_newState;
	PB::SubresourceRange m_subresourceRange;
};

// Contains all necessary information to run a single render pass.
struct RenderGraphRuntimeNode
{
	CLib::Vector<PB::RenderTargetView, 8, 8> m_renderTargetViews;
	CLib::Vector<PB::Float4, 8, 8> m_clearColors;
	CLib::Vector<PB::ITexture*, 8, 8> m_transientTextures;
	CLib::Vector<RTTransitionData, 8, 8> m_transitions;

	// Only valid for non-dynamic render passes.
	PB::Framebuffer m_framebuffer = nullptr;
	PB::RenderPass m_renderPass = nullptr;

	uint32_t m_renderWidth = 0;
	uint32_t m_renderHeight = 0;
	bool m_useReusableCommandLists = false;

	RenderGraphBehaviour* m_behaviour = nullptr;
	RenderGraphRuntimeNode* m_next = nullptr;
};


class RenderGraphBuilder
{
public:

	RenderGraphBuilder(PB::IRenderer* renderer, CLib::Allocator* allocator);

	~RenderGraphBuilder();

	void AddNode(const NodeDesc& desc);

	RenderGraph* Build(bool useDynamicRenderPasses);

private:

	struct TransientTextureMeta
	{
		TransientTextureMeta() = default;

		TransientTextureMeta(const AttachmentDesc& desc);
		TransientTextureMeta(const TransientTextureDesc& desc);

		const char* m_name = nullptr;
		uint32_t m_width;
		uint32_t m_height;
		PB::TextureStateFlags m_usage;
		PB::ETextureFormat m_format;
		AttachmentFlags m_flags; 
		uint32_t m_mipCount;
		uint32_t m_arraySize;
	};

	struct InternalBuildNode
	{
		struct PassAttachmentUsageData
		{
			const char* m_attachmentName;
			PB::EAttachmentUsage m_usage;
			PB::Float4 m_clearColor;
			uint8_t mipLevel = 0;
			uint8_t arrayLayer = 0;
			bool m_clear = false;
		};

		struct PassTextureUsageData
		{
			const char* m_transientTextureName;
			PB::ETextureState m_initialUsage;
			PB::ETextureState m_finalUsage;
		};

		std::vector<PassAttachmentUsageData> m_attachments{};
		std::vector<PassTextureUsageData> m_transientTextures{};
		RenderGraphBehaviour* m_behaviour = nullptr;
		uint32_t m_renderWidth = 0;
		uint32_t m_renderHeight = 0;
		bool m_useReusableCommandLists = false;
		bool m_computeOnlyPass = false;
	};

	// Tracks the usage of attachments through multiple passes.
	struct TextureUsageData
	{
		CLib::String<8, char> m_name;
		PB::ITexture* m_texture = nullptr;
		uint32_t m_firstPassIndex = ~0u;			// Index of the first pass where this texture is used.
		uint32_t m_mostRecentPassIndex = ~0u;		// Index of the most recent pass this texture is used in so far (during build step).
		PB::ETextureState m_firstUsageState;
		PB::ETextureState m_mostRecentUsageState;
		
		TransientTextureMeta m_meta;
	};

	void TextureDescFromTransientMeta(const TransientTextureMeta& meta, PB::TextureDesc& outDesc);

	void UpdateTextureUsageData(const TransientTextureMeta& meta, PB::ETextureState initialUsage, uint32_t timePoint);

	void RecordInitialTransitions();

	size_t GetSizeFromAttachMeta(const TransientTextureMeta& meta);

	PB::ITexture* FindTexture(const TransientTextureMeta& meta, RenderGraph* graph);

	PB::ITexture* CreateTexture(const TransientTextureMeta& meta);

	PB::RenderTargetView GetTextureView(const TextureUsageData& meta, PB::ETextureState expectedState, uint8_t mip, uint8_t arraylayer);

	PB::IRenderer* m_renderer = nullptr;
	CLib::Allocator* m_allocator;

	std::map<size_t, TextureUsageData> m_freeList;
	std::unordered_map<std::string, TextureUsageData> m_texUsageDatas;

	std::vector<InternalBuildNode> m_buildNodes;
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

	RenderGraphRuntimeNode* m_start = nullptr;
};