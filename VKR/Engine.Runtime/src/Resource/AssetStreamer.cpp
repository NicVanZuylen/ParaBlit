#include "Resource/AssetStreamer.h"
#include "Resource/Mesh.h"

namespace Eng
{
	void StreamingBatch::BeginStreaming(bool block)
	{
		{
			m_streamer->AddStreamingBatch(this);
			m_status = EStreamingStatus::STREAMING;
			m_usedThisFrame = true;
		}

		do {} while (block && m_status != EStreamingStatus::DONE);
	}

	void StreamingBatch::EndStreamingAndDelete()
	{
		m_status = EStreamingStatus::FREE;
	}

	AssetStreamer::~AssetStreamer()
	{
	}

	void AssetStreamer::Init(PB::IRenderer* renderer, const char* databasePath)
	{
		m_renderer = renderer;
		m_meshReader = &Mesh::s_meshDatabaseLoader;
		if (!m_meshReader->HasOpenFile())
		{
			std::string meshPath = databasePath;
			meshPath += "/meshes.adb";
			m_meshReader->OpenFile(meshPath.c_str());
		}

		m_activeStreamingThread = true;
		m_streamingThread = std::thread(UpdateThread, this);
	}

	void AssetStreamer::Shutdown()
	{
		m_activeStreamingThread = false;
		m_streamingThread.join();

		for (auto& batch : m_freeBatches)
		{
			m_streamingBatchAllocator.Free(batch);
		}
		m_freeBatches.Clear();

		for (auto& resource : m_resourceMap)
		{
			ResourceID id = resource.first;
			ResourceHandle handle = resource.second.m_handle;

			EStreamableResourceType type = EStreamableResourceType(id >> StreamableHandle::TypeShift);
			switch (type)
			{
			case Eng::EStreamableResourceType::INVALID:
				break;
			case Eng::EStreamableResourceType::MESH:
				m_resourceAllocator.Free(reinterpret_cast<Mesh*>(handle));
				break;
			default:
				break;
			}
		}

		m_resourceAllocator.DumpMemoryLeaks();
	}

	void AssetStreamer::FreeBatch(StreamingBatch* batch)
	{
		batch->m_next = nullptr;
		m_freeBatches.PushBack(batch);
	}

	void AssetStreamer::Update()
	{
		while (m_activeStreamingThread)
		{
			{
				std::lock_guard<std::mutex> lock(m_pendingLock);

				if (m_pendingBatches)
				{
					// Insert pending batches to the front of the live list.
					m_pendingBatches->m_next = m_liveBatches;
					m_liveBatches = m_pendingBatches;
					m_pendingBatches = nullptr;
				}
			}

			std::lock_guard<std::mutex> lock(m_streamingLock);

			StreamingBatch* prevBatch = nullptr;
			StreamingBatch* batch = m_liveBatches;
			while(batch != nullptr)
			{
				if (batch->m_status == StreamingBatch::EStreamingStatus::FREE)
				{
					if (prevBatch != nullptr)
					{
						prevBatch->m_next = batch->m_next;
					}
					else
					{
						m_liveBatches = batch->m_next;
					}

					FreeBatch(batch);
					continue;
				}

				if (batch->m_usedThisFrame == true)
				{
					batch->m_usedThisFrame = false;
					bool allLoaded = true;

					// Load all batch resources...
					for (const StreamableHandle& resource : batch->m_resources)
					{
						auto it = m_resourceMap.find(resource.m_id);
						if (it == m_resourceMap.end())
						{
							it = m_resourceMap.insert({ resource.m_id, {} }).first;
						}
						ResourceData& data = it->second;
						data.m_lastUsedFrame = m_currentFrame;

						if (data.m_handle == nullptr)
							allLoaded &= LoadResource(resource, data.m_handle);
					}

					batch->m_status = allLoaded ? StreamingBatch::EStreamingStatus::DONE : StreamingBatch::EStreamingStatus::STREAMING;
				}

				prevBatch = batch;
				batch = batch->m_next;
			}
		}

		// Shutdown.
		StreamingBatch* batch = m_pendingBatches;
		while (batch != nullptr)
		{
			m_freeBatches.PushBack(batch);
			batch = batch->m_next;
		}

		batch = m_liveBatches;
		while (batch != nullptr)
		{
			m_freeBatches.PushBack(batch);
			batch = batch->m_next;
		}
	}

	void AssetStreamer::UpdateThread(AssetStreamer* streamer)
	{
		streamer->Update();
	}

	void AssetStreamer::AddStreamingBatch(StreamingBatch* batch)
	{
		std::lock_guard<std::mutex> lock(m_pendingLock);
		batch->m_next = m_pendingBatches;
		m_pendingBatches = batch;
	}

	bool AssetStreamer::LoadResource(const StreamableHandle& handle, ResourceHandle& resHandle)
	{
		EStreamableResourceType type = handle.GetType();
		switch (type)
		{
		case Eng::EStreamableResourceType::INVALID:
			break;
		case Eng::EStreamableResourceType::MESH:
		{
			Mesh*& mesh = reinterpret_cast<Mesh*&>(resHandle);
			mesh = m_resourceAllocator.Alloc<Mesh>();
			mesh->Load(m_renderer, m_meshReader, handle.GetID());
			return true;
		}
		default:
			break;
		}

		return false;
	}

	StreamingBatch* AssetStreamer::AllocStreamingBatch(uint32_t reserveResourceCount)
	{
		StreamingBatch* batch = m_freeBatches.Count() > 0 ? m_freeBatches.PopBack() : m_streamingBatchAllocator.Alloc<StreamingBatch>();
		batch->m_streamer = this;

		return batch;
	}

	AssetStreamer::ResourceHandle AssetStreamer::GetResourceHandle(const StreamableHandle& handle)
	{
		std::lock_guard<std::mutex> lock(m_streamingLock);
		auto it = m_resourceMap.find(handle.m_id);
		if (it == m_resourceMap.end())
			return nullptr;

		return it->second.m_handle;
	}
}