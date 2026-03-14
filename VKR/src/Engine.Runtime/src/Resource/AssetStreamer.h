#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "CLib/Vector.h"
#include "CLib/Allocator.h"
#include "CLib/FixedBlockAllocator.h"
#include "Engine.AssetEncoder/AssetBinaryDatabaseReader.h"

#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace Eng
{
	class AssetStreamer;

	enum class EStreamableResourceType : uint8_t
	{
		INVALID,
		MESH,
		TEXTURE,
	};

	using ResourceID = uint64_t;

	static_assert(sizeof(ResourceID) == sizeof(uint64_t));

	namespace StreamableSimple
	{
		enum ESimpleTextureType : uint8_t
		{
			NONE,
			SOLID_BLACK,
			SOLID_WHITE,
			MID_GRAY,
			FLAT_NORMAL_MAP,
			COUNT
		};

		static const AssetEncoder::AssetID SrvBlackHandle = ~AssetEncoder::AssetID(0) - SOLID_BLACK;
		static const AssetEncoder::AssetID SrvWhiteHandle = ~AssetEncoder::AssetID(0) - SOLID_WHITE;
		static const AssetEncoder::AssetID SrvMidGrayHandle = ~AssetEncoder::AssetID(0) - MID_GRAY;
		static const AssetEncoder::AssetID SrvFlatNormalMapHandle = ~AssetEncoder::AssetID(0) - FLAT_NORMAL_MAP;
	}

	struct StreamableHandle
	{
		enum class EBindingType : uint8_t
		{
			NONE,
			SRV,
			STORAGE
		};

		static constexpr uint32_t TypeShift = 56;
		static constexpr uint32_t IDShift = 64 - TypeShift;
		static constexpr uint8_t BindingSubresourceAll = ~uint8_t(0);

		StreamableHandle(AssetEncoder::AssetID id, EStreamableResourceType type, EBindingType bindingType, uint8_t bindingSubresource = BindingSubresourceAll)
		{
			if (id > ~AssetEncoder::AssetID(0) - StreamableSimple::ESimpleTextureType::COUNT)
			{
				AssetEncoder::AssetID simpleID = (~AssetEncoder::AssetID(0) - id);
				m_simpleType = StreamableSimple::ESimpleTextureType(simpleID);
			}

			m_id = id;
			m_id |= (uint64_t(type) << TypeShift);
			m_bindingType = bindingType;
			m_bindingSubresource = bindingSubresource;
		}

		AssetEncoder::AssetID GetID() const { return (m_id << IDShift) >> IDShift; }

		EStreamableResourceType GetType() const { return EStreamableResourceType(m_id >> TypeShift); }

		ResourceID m_id;
		EBindingType m_bindingType;
		uint8_t m_bindingSubresource;
		StreamableSimple::ESimpleTextureType m_simpleType = StreamableSimple::ESimpleTextureType::NONE;
	};

	class StreamingBatch
	{
	public:

		StreamingBatch(uint32_t reserveResourceCount = 1)
		{
			m_resources.Reserve(reserveResourceCount);
		}

		~StreamingBatch() = default;

		enum class EStreamingStatus : uint32_t
		{
			PENDING_DELETE,
			PENDING_STREAMING,
			STREAMING,
			IDLE
		};

		enum EBindingOutputLocation : uint8_t
		{
			CPU_BUFFER,
			GPU_BUFFER
		};

		void AddResource(const StreamableHandle& desc)
		{
			m_resources.PushBack(desc);
		};

		const StreamableHandle& GetResourceStreamingHandle(uint32_t index)
		{
			return m_resources[index];
		}

		void SetOutputBindingLocation(PB::ResourceView* outputBindings) 
		{
			m_bindingsDstCpu = outputBindings;
			m_bindingOutputLocation = EBindingOutputLocation::CPU_BUFFER;
		};

		void SetOutputBindingLocation(PB::IBufferObject* outputBuffer, size_t outputOffsetBytes)
		{
			m_bindingsDstGpu = outputBuffer;
			m_gpuBufferOffset = outputOffsetBytes;
			m_bindingOutputLocation = EBindingOutputLocation::GPU_BUFFER;
		}

		EStreamingStatus GetStatus() const { return m_status; }

		void WaitForStatus(EStreamingStatus status);

		/*
		Description: Begin streaming in the assets in this batch.
		Param:
			bool block: If true, this will block execution on the current thread until streaming is complete.
		*/
		void BeginStreaming(bool block = false);

		void WaitStreamingComplete();

		void EndStreamingAndDelete();

		/*
		Description: Notify the streaming system that the resources in this streaming batch will be needed this frame. This will prevent them from being unloaded too soon. 
		*/
		void NotifyUsage();

		uint32_t CalculateOutputBindingCount();

	private:

		friend class AssetStreamer;

		uint32_t GetMeshBindingCount(StreamableHandle::EBindingType bindingType, uint8_t bindingSubresource);
		uint32_t GetTextureBindingCount(StreamableHandle::EBindingType bindingType, uint8_t bindingSubresource);

		void WaitForFrames(int64_t waitFrameCount, size_t currentFrame);
		bool IsWaitingDone(size_t currentFrame);

		void Reset(AssetStreamer* streamer);

		// Output location of loaded resource binding indices.
		StreamingBatch* m_next = nullptr;
		AssetStreamer* m_streamer = nullptr;

		union
		{
			PB::ResourceView* m_bindingsDstCpu;
			PB::IBufferObject* m_bindingsDstGpu = nullptr;
		};
		size_t m_gpuBufferOffset = 0;

		EStreamingStatus m_status = EStreamingStatus::PENDING_STREAMING;
		EBindingOutputLocation m_bindingOutputLocation = EBindingOutputLocation::GPU_BUFFER;
		int64_t m_streamingWaitFrames = 0;
		int64_t m_streamingWaitBegin = 0;
		std::mutex m_streamingLock;
		std::condition_variable m_conditionVar;
		CLib::Vector<StreamableHandle, 4, 4> m_resources;
	};

	class AssetStreamer
	{
	public:

		using ResourceHandle = void*;

		AssetStreamer() = default;
		~AssetStreamer();

		void Init(PB::IRenderer* renderer, const char* databasePath);

		void Shutdown();

		void NextFrame();

		StreamingBatch* AllocStreamingBatch(uint32_t reserveResourceCount = 1);

		ResourceHandle GetResourceHandle(const StreamableHandle& handle);

	private:

		struct ResourceData
		{
			ResourceHandle m_handle = nullptr;
			size_t m_lastUsedFrame = ~size_t(0);
		};

		struct IndexUploadBuffer
		{
			PB::IBufferObject* m_buffer = nullptr;
			PB::ResourceView* m_mapped = nullptr;
			size_t m_maxIndexCount = 0;
			size_t m_allocatedIndexCount = 0;
		};

		using CopyPair = std::pair<PB::IBufferObject*, PB::IBufferObject*>;

		void Update();

		void FreeBatch(StreamingBatch* batch);

		static void UpdateThread(AssetStreamer* streamer);

		void AddStreamingBatch(StreamingBatch* batch);

		bool LoadResource(const StreamableHandle& handle, ResourceHandle& resHandle, PB::ResourceView*& outputBinding);

		bool LoadViews(const StreamableHandle& handle, ResourceHandle& resHandle, PB::ResourceView*& outputBinding);

		void AllocateNewUploadBuffer();

		PB::ResourceView* AllocateIndexUpload(PB::IBufferObject* dstBuffer, uint32_t dstOffset, uint32_t indexCount);

		friend class StreamingBatch;

		PB::IRenderer* m_renderer = nullptr;
		std::mutex m_pendingLock;
		std::mutex m_streamingLock;
		std::mutex m_freeBatchLock;
		std::thread m_streamingThread;
		std::unordered_map<ResourceID, ResourceData> m_resourceMap;
		std::map<CopyPair, CLib::Vector<PB::CopyRegion, 16, 16>> m_indexUploadMap;
		CLib::Vector<IndexUploadBuffer*, 0, 4> m_indexUploadSrcBuffers;
		AssetEncoder::AssetBinaryDatabaseReader* m_meshReader = nullptr;
		AssetEncoder::AssetBinaryDatabaseReader* m_textureReader = nullptr;
		CLib::FixedBlockAllocator m_streamingBatchAllocator{ sizeof(StreamingBatch), sizeof(StreamingBatch) * 256 };
		CLib::Allocator m_resourceAllocator;
		CLib::Vector<StreamingBatch*, 0, 256> m_freeBatches;
		StreamingBatch* m_liveBatches = nullptr;
		StreamingBatch* m_pendingBatches = nullptr;

		PB::ITexture* m_simpleBlackTexture = nullptr;
		PB::ITexture* m_simpleWhiteTexture = nullptr;
		PB::ITexture* m_simpleMidGrayTexture = nullptr;
		PB::ITexture* m_simpleFlatNormalTexture = nullptr;

		size_t m_currentFrame = 0;
		bool m_activeStreamingThread = false;
		bool m_indexUploadReady = false;
	};
}