#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "CLib/Vector.h"
#include "CLib/Allocator.h"
#include "CLib/FixedBlockAllocator.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"

#include <mutex>
#include <thread>

namespace Eng
{
	class AssetStreamer;

	enum class EStreamableResourceType : uint8_t
	{
		INVALID,
		MESH,
	};

	using ResourceID = uint64_t;

	static_assert(sizeof(ResourceID) == sizeof(uint64_t));

	struct StreamableHandle
	{
		enum class EBindingType : uint32_t
		{
			NONE,
			SRV,
			STORAGE
		};

		static constexpr uint32_t TypeShift = 56;
		static constexpr uint32_t IDShift = 64 - TypeShift;

		StreamableHandle(AssetEncoder::AssetID id, EStreamableResourceType type, EBindingType bindingType)
		{
			m_id = id;
			m_id |= (uint64_t(type) << TypeShift);
			m_bindingType = bindingType;
		}

		AssetEncoder::AssetID GetID() const { return (m_id << IDShift) >> IDShift; }

		EStreamableResourceType GetType() const { return EStreamableResourceType(m_id >> TypeShift); }

		ResourceID m_id = 0;
		EBindingType m_bindingType = EBindingType::NONE;
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
			FREE,
			PENDING_STREAMING,
			STREAMING,
			DONE
		};


		void AddResource(const StreamableHandle& desc)
		{
			m_resources.PushBack(desc);
		};

		EStreamingStatus GetStatus() const { return m_status; }

		/*
		Description: Begin streaming in the assets in this batch.
		Param:
			bool block: If true, this will block execution on the current thread until streaming is complete.
		*/
		void BeginStreaming(bool block = false);

		void EndStreamingAndDelete();

		/*
		Description: Notify the streaming system that the resources in this streaming batch will be needed this frame. This will prevent them from being unloaded too soon. 
		*/
		void NotifyUsage() { m_usedThisFrame = true; };

	private:

		friend class AssetStreamer;

		// Output location of loaded resource binding indices.
		StreamingBatch* m_next = nullptr;
		AssetStreamer* m_streamer = nullptr;
		uint32_t* bindingsDst = nullptr;
		EStreamingStatus m_status = EStreamingStatus::PENDING_STREAMING;
		bool m_usedThisFrame = false;
		CLib::Vector<StreamableHandle, 16, 16> m_resources;
	};

	class AssetStreamer
	{
	public:

		using ResourceHandle = void*;

		AssetStreamer() = default;
		~AssetStreamer();

		void Init(PB::IRenderer* renderer, const char* databasePath);

		void Shutdown();

		void NextFrame() { ++m_currentFrame; };

		StreamingBatch* AllocStreamingBatch(uint32_t reserveResourceCount = 1);

		ResourceHandle GetResourceHandle(const StreamableHandle& handle);

	private:

		struct ResourceData
		{
			ResourceHandle m_handle = nullptr;
			size_t m_lastUsedFrame = ~size_t(0);
		};

		void Update();

		void FreeBatch(StreamingBatch* batch);

		static void UpdateThread(AssetStreamer* streamer);

		void AddStreamingBatch(StreamingBatch* batch);

		bool LoadResource(const StreamableHandle& handle, ResourceHandle& resHandle);

		friend class StreamingBatch;

		PB::IRenderer* m_renderer = nullptr;
		std::mutex m_pendingLock;
		std::mutex m_streamingLock;
		std::thread m_streamingThread;
		std::unordered_map<ResourceID, ResourceData> m_resourceMap;
		AssetEncoder::AssetBinaryDatabaseReader* m_meshReader;
		CLib::FixedBlockAllocator m_streamingBatchAllocator{ sizeof(StreamingBatch), sizeof(StreamingBatch) * 256 };
		CLib::Allocator m_resourceAllocator;
		CLib::Vector<StreamingBatch*, 1, 256> m_freeBatches;
		StreamingBatch* m_liveBatches = nullptr;
		StreamingBatch* m_pendingBatches = nullptr;
		size_t m_currentFrame = 0;
		bool m_activeStreamingThread = false;
	};
}