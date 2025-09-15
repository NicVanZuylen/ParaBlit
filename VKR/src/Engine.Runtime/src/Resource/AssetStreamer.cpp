#include "Resource/AssetStreamer.h"
#include "Resource/Mesh.h"
#include "Resource/Texture.h"

#define ASSET_STREAMER_VERBOSE 0

#if ASSET_STREAMER_VERBOSE
#define ASSET_STREAMER_LOG(format, values) printf_s(format, values);
#else
#define ASSET_STREAMER_LOG(format, values)
#endif

namespace Eng
{
	void StreamingBatch::WaitForStatus(EStreamingStatus status)
	{
		std::unique_lock lock(m_streamingLock);
		m_conditionVar.wait(lock, [&]() { return m_status == status; });
		lock.unlock();
	}

	void StreamingBatch::BeginStreaming(bool block)
	{
		{
			m_streamer->AddStreamingBatch(this);
			m_status = EStreamingStatus::STREAMING;
		}

		if (block)
		{
			WaitForStatus(EStreamingStatus::IDLE);
		}
	}

	void StreamingBatch::WaitStreamingComplete()
	{
		WaitForStatus(EStreamingStatus::IDLE);
	}

	void StreamingBatch::EndStreamingAndDelete()
	{
		m_status = EStreamingStatus::PENDING_DELETE;
	}

	void StreamingBatch::NotifyUsage()
	{
		if (m_status != EStreamingStatus::IDLE)
			return;

		m_status = EStreamingStatus::STREAMING;
	}

	uint32_t StreamingBatch::GetMeshBindingCount(StreamableHandle::EBindingType bindingType, uint8_t bindingSubresource)
	{
		switch (bindingType)
		{
		case Eng::StreamableHandle::EBindingType::STORAGE:
		{
			return (bindingSubresource == StreamableHandle::BindingSubresourceAll) ? 4 : 1;
		}
		default:
			return 0;
		}
	}

	uint32_t StreamingBatch::GetTextureBindingCount(StreamableHandle::EBindingType bindingType, uint8_t bindingSubresource)
	{
		switch (bindingType)
		{
			case Eng::StreamableHandle::EBindingType::SRV:
			case Eng::StreamableHandle::EBindingType::STORAGE:
				return 1;
			default:
				return 0;
		};
	}

	void StreamingBatch::WaitForFrames(int64_t waitFrameCount, size_t currentFrame)
	{
		m_streamingWaitBegin = int64_t(currentFrame);
		m_streamingWaitFrames = std::max<int64_t>(waitFrameCount, m_streamingWaitFrames);
	}

	bool StreamingBatch::IsWaitingDone(size_t currentFrame)
	{
		if ((int64_t(currentFrame) - m_streamingWaitBegin) >= m_streamingWaitFrames)
		{
			m_streamingWaitBegin = 0;
			m_streamingWaitFrames = 0;

			return true;
		}

		return false;
	}

	uint32_t StreamingBatch::CalculateOutputBindingCount()
	{
		uint32_t bindingCount = 0;
		for (const StreamableHandle& handle : m_resources)
		{
			switch (handle.GetType())
			{
			case EStreamableResourceType::MESH:
			{
				bindingCount += GetMeshBindingCount(handle.m_bindingType, handle.m_bindingSubresource);
				break;
			}
			case EStreamableResourceType::TEXTURE:
			{
				bindingCount += GetTextureBindingCount(handle.m_bindingType, handle.m_bindingSubresource);
				break;
			}
			default:
				bindingCount += 1;
				break;
			};
		}
		return bindingCount;
	}

	void StreamingBatch::Reset(AssetStreamer* streamer)
	{
		this->~StreamingBatch();
		new(this) StreamingBatch(4);
		m_streamer = streamer;
	}

	AssetStreamer::~AssetStreamer()
	{
	}

	void AssetStreamer::Init(PB::IRenderer* renderer, const char* databasePath)
	{
		m_renderer = renderer;
		m_meshReader = &Mesh::s_meshDatabaseLoader;
		Mesh::InitializeMeshLibrary(m_renderer, &m_resourceAllocator);

		// Open data files.
		if (!m_meshReader->HasOpenFile())
		{
			std::string meshPath = databasePath;
			meshPath += "/meshes.adb";
			m_meshReader->OpenFile(meshPath.c_str());
		}

		m_textureReader = &Texture::s_textureDatabaseLoader;
		if (!m_textureReader->HasOpenFile())
		{
			std::string texturesPath = databasePath;
			texturesPath += "/textures.adb";
			m_textureReader->OpenFile(texturesPath.c_str());
		}

		// Create simple textures.
		PB::u8 texDataBlack[4] = { 0, 0, 0, 0 };
		PB::u8 texDataWhite[4] = { 255, 255, 255, 255 };
		PB::u8 texDataMidGray[4] = { 128, 128, 128, 128 };
		PB::u8 texDataFlatNormal[4] = { 128, 128, 255, 255 };

		PB::TextureDataDesc simpleDataDesc{};
		simpleDataDesc.m_size = sizeof(texDataBlack);

		PB::TextureDesc simpleTextureDesc{};
		simpleTextureDesc.m_width = 1;
		simpleTextureDesc.m_height = 1;
		simpleTextureDesc.m_format = PB::ETextureFormat::R8G8B8A8_UNORM;
		simpleTextureDesc.m_dimension = PB::ETextureDimension::DIMENSION_2D;
		simpleTextureDesc.m_initOptions = PB::ETextureInitOptions::PB_TEXTURE_INIT_USE_DATA;
		simpleTextureDesc.m_usageStates = PB::ETextureState::SAMPLED;
		simpleTextureDesc.m_data = &simpleDataDesc;

		simpleTextureDesc.m_name = "TextureSimple_Black";
		simpleDataDesc.m_data = texDataBlack;
		m_simpleBlackTexture = m_renderer->AllocateTexture(simpleTextureDesc);

		simpleTextureDesc.m_name = "TextureSimple_White";
		simpleDataDesc.m_data = texDataWhite;
		m_simpleWhiteTexture = m_renderer->AllocateTexture(simpleTextureDesc);

		simpleTextureDesc.m_name = "TextureSimple_MidGray";
		simpleDataDesc.m_data = texDataMidGray;
		m_simpleMidGrayTexture = m_renderer->AllocateTexture(simpleTextureDesc);

		simpleTextureDesc.m_name = "TextureSimple_FlatNormal";
		simpleDataDesc.m_data = texDataFlatNormal;
		m_simpleFlatNormalTexture = m_renderer->AllocateTexture(simpleTextureDesc);

		// Init streaming thread.
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
			case Eng::EStreamableResourceType::TEXTURE:
				m_resourceAllocator.Free(reinterpret_cast<Texture*>(handle));
				break;
			default:
				break;
			}
		}

		Mesh::DestroyMeshLibrary(&m_resourceAllocator);

		for (auto& indexUploadBuf : m_indexUploadSrcBuffers)
		{
			m_renderer->FreeBuffer(indexUploadBuf->m_buffer);
		}

		if (m_simpleBlackTexture)
		{
			m_renderer->FreeTexture(m_simpleBlackTexture);
			m_simpleBlackTexture = nullptr;
		}
		if (m_simpleWhiteTexture)
		{
			m_renderer->FreeTexture(m_simpleWhiteTexture);
			m_simpleWhiteTexture = nullptr;
		}
		if (m_simpleMidGrayTexture)
		{
			m_renderer->FreeTexture(m_simpleMidGrayTexture);
			m_simpleMidGrayTexture = nullptr;
		}
		if (m_simpleFlatNormalTexture)
		{
			m_renderer->FreeTexture(m_simpleFlatNormalTexture);
			m_simpleFlatNormalTexture = nullptr;
		}

		m_resourceAllocator.DumpMemoryLeaks();
	}

	void AssetStreamer::NextFrame()
	{
		Mesh::MeshLibraryUpdate();

		++m_currentFrame;
		m_indexUploadReady = true;
	}

	void AssetStreamer::FreeBatch(StreamingBatch* batch)
	{
		std::lock_guard<std::mutex> lock(m_freeBatchLock);
		batch->m_next = nullptr;
		m_freeBatches.PushBack(batch);
	}

	void AssetStreamer::Update()
	{
		while (m_activeStreamingThread)
		{
			std::lock_guard<std::mutex> lock(m_streamingLock);

			StreamingBatch* prevBatch = nullptr;
			StreamingBatch* batch = m_liveBatches;
			while(batch != nullptr)
			{
				std::lock_guard<std::mutex> lock(batch->m_streamingLock);

				if (batch->m_status == StreamingBatch::EStreamingStatus::PENDING_DELETE)
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
					batch = batch->m_next;
					continue;
				}
				else if (batch->m_status == StreamingBatch::EStreamingStatus::STREAMING && batch->m_streamingWaitFrames-- <= 0)
				{
					ASSET_STREAMER_LOG("BATCH: %p, STATUS=STREAMING\n", batch);

					// Load all batch resources...
					bool useGPUBindings = batch->m_bindingOutputLocation == StreamingBatch::EBindingOutputLocation::GPU_BUFFER;
					PB::ResourceView* bindingsDst = nullptr;
					if (useGPUBindings)
					{
						bindingsDst = AllocateIndexUpload(batch->m_bindingsDstGpu, batch->m_gpuBufferOffset, batch->CalculateOutputBindingCount());
						//batch->WaitForFrames(1, m_currentFrame); // Wait a single frame for binding index copies to complete before flagging streaming as done.
					}
					else
					{
						bindingsDst = batch->m_bindingsDstCpu;
					}

					bool allLoaded = batch->m_streamingWaitFrames <= 0;
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
						{
							allLoaded &= LoadResource(resource, data.m_handle, bindingsDst);
						}
						else if(bindingsDst != nullptr)
						{
							allLoaded &= LoadViews(resource, data.m_handle, bindingsDst);
						}
					}

					batch->m_status = allLoaded ? StreamingBatch::EStreamingStatus::IDLE : StreamingBatch::EStreamingStatus::STREAMING;
					batch->m_conditionVar.notify_one();
				}
				else if (batch->IsWaitingDone(m_currentFrame))
				{
					//batch->m_status = StreamingBatch::EStreamingStatus::IDLE;
				}

				prevBatch = batch;
				batch = batch->m_next;
			}

			// Append pending batches.
			{
				std::lock_guard<std::mutex> lock(m_pendingLock);

				if (m_pendingBatches)
				{
					if (prevBatch != nullptr)
					{
						prevBatch->m_next = m_pendingBatches;
					}
					else
					{
						m_liveBatches = m_pendingBatches;
					}
					m_pendingBatches = nullptr;
				}
			}

			// Upload any indices to GPU.
			if(m_indexUploadReady == true && m_indexUploadMap.empty() == false)
			{
				PB::ICommandContext* copyCmdContext = m_renderer->GetThreadUploadContext();

				for (auto& [srcDstPair, regions] : m_indexUploadMap)
				{
					copyCmdContext->CmdCopyBufferToBuffer(srcDstPair.first, srcDstPair.second, regions.Data(), regions.Count());
				}
				m_indexUploadMap.clear();

				for (IndexUploadBuffer*& uploadBuffer : m_indexUploadSrcBuffers)
				{
					uploadBuffer->m_buffer->Unmap();
					m_renderer->FreeBuffer(uploadBuffer->m_buffer);
					m_resourceAllocator.Free(uploadBuffer);
				}
				m_indexUploadSrcBuffers.Clear();

				m_indexUploadReady = false;
				m_renderer->ReturnThreadUploadContext(copyCmdContext);
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

	bool AssetStreamer::LoadResource(const StreamableHandle& handle, ResourceHandle& resHandle, PB::ResourceView*& outputBinding)
	{
		switch (handle.m_simpleType)
		{
		case StreamableSimple::ESimpleTextureType::SOLID_BLACK:
		case StreamableSimple::ESimpleTextureType::SOLID_WHITE:
		case StreamableSimple::ESimpleTextureType::MID_GRAY:
		case StreamableSimple::ESimpleTextureType::FLAT_NORMAL_MAP:
		{
			resHandle = nullptr;
			return outputBinding != nullptr ? LoadViews(handle, resHandle, outputBinding) : true;
		}
		default:
			break;
		}

		EStreamableResourceType type = handle.GetType();
		switch (type)
		{
		case Eng::EStreamableResourceType::INVALID:
			break;
		case Eng::EStreamableResourceType::MESH:
		{
			Mesh*& mesh = reinterpret_cast<Mesh*&>(resHandle);
			mesh = m_resourceAllocator.Alloc<Mesh>(handle.GetID(), m_meshReader);
			mesh->Load(m_renderer, m_meshReader);

			return outputBinding != nullptr ? LoadViews(handle, resHandle, outputBinding) : true;
		}
		case Eng::EStreamableResourceType::TEXTURE:
		{
			Texture*& texture = reinterpret_cast<Texture*&>(resHandle);
			texture = m_resourceAllocator.Alloc<Texture>(m_renderer);
			texture->Load2D(handle.GetID(), m_textureReader);

			return outputBinding != nullptr ? LoadViews(handle, resHandle, outputBinding) : true;
		}
		default:
			break;
		}

		return false;
	}

	bool AssetStreamer::LoadViews(const StreamableHandle& handle, ResourceHandle& resHandle, PB::ResourceView*& outputBinding)
	{
		assert(outputBinding != nullptr);
		
		if (resHandle == nullptr)
		{
			assert(handle.m_bindingType == StreamableHandle::EBindingType::SRV);

			switch (handle.m_simpleType)
			{
			case StreamableSimple::ESimpleTextureType::SOLID_BLACK:
			{
				*outputBinding = m_simpleBlackTexture->GetDefaultSRV();
				break;
			}
			case StreamableSimple::ESimpleTextureType::SOLID_WHITE:
			{
				*outputBinding = m_simpleWhiteTexture->GetDefaultSRV();
				break;
			}
			case StreamableSimple::ESimpleTextureType::MID_GRAY:
			{
				*outputBinding = m_simpleMidGrayTexture->GetDefaultSRV();
				break;
			}
			case StreamableSimple::ESimpleTextureType::FLAT_NORMAL_MAP:
			{
				*outputBinding = m_simpleFlatNormalTexture->GetDefaultSRV();
				break;
			}
			default:
				break;
			}

			outputBinding += 1;
			return true;
		}

		EStreamableResourceType type = handle.GetType();
		switch (type)
		{
		case Eng::EStreamableResourceType::INVALID:
			break;
		case Eng::EStreamableResourceType::MESH:
		{
			Mesh* mesh = reinterpret_cast<Mesh*&>(resHandle);

			switch (handle.m_bindingType)
			{
			case StreamableHandle::EBindingType::SRV:
			case StreamableHandle::EBindingType::STORAGE:
			{
				if (handle.m_bindingSubresource == 0)
					*outputBinding = mesh->GetVertexBuffer()->GetViewAsStorageBuffer();
				else if (handle.m_bindingSubresource == 1)
					*outputBinding = mesh->GetIndexBuffer()->GetViewAsStorageBuffer();
				else if (handle.m_bindingSubresource == 2)
					*outputBinding = mesh->GetMeshletBuffer()->GetViewAsStorageBuffer();
				else if (handle.m_bindingSubresource == 3)
				{
					outputBinding[0] = mesh->GetMeshletBuffer()->GetViewAsStorageBuffer();
					outputBinding[1] = mesh->GetMeshletPrimitiveBuffer()->GetViewAsStorageBuffer();
					outputBinding[2] = mesh->GetVertexBuffer()->GetViewAsStorageBuffer();
					outputBinding[3] = mesh->GetIndexBuffer()->GetViewAsStorageBuffer();
					outputBinding += 3;
				}
				else
				{
					*outputBinding = mesh->GetLibraryInstanceID();
				}

				outputBinding += 1;
				ASSET_STREAMER_LOG("OUTPUT MESH BINDING: %s\n", Mesh::s_meshDatabaseLoader.GetAssetName(mesh->GetAssetID()));
				break;
			}
			default:
				break;
			}

			return true;
		}
		case Eng::EStreamableResourceType::TEXTURE:
		{
			Texture* texture = reinterpret_cast<Texture*&>(resHandle);

			if (outputBinding != nullptr)
			{
				switch (handle.m_bindingType)
				{
				case StreamableHandle::EBindingType::SRV:
				{
					*outputBinding = texture->GetTexture()->GetDefaultSRV();
					outputBinding += 1;
					break;
				}
				case StreamableHandle::EBindingType::STORAGE:
				{
					*outputBinding = texture->GetTexture()->GetDefaultSIV();
					outputBinding += 1;
					break;
				}
				default:
					break;
				}
			}

			return true;
		}
		default:
			break;
		}

		return false;
	}

	void AssetStreamer::AllocateNewUploadBuffer()
	{
		constexpr PB::u32 MaxIndexCount = 1024;
		constexpr PB::u32 UploadBufferSize = MaxIndexCount * sizeof(PB::ResourceView);

		PB::BufferObjectDesc uploadBufferDesc{};
		uploadBufferDesc.m_bufferSize = UploadBufferSize;
		uploadBufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE;
		uploadBufferDesc.m_usage = PB::EBufferUsage::COPY_SRC;

		auto* buf = m_indexUploadSrcBuffers.PushBack(m_resourceAllocator.Alloc<IndexUploadBuffer>());
		buf->m_buffer = m_renderer->AllocateBuffer(uploadBufferDesc);
		buf->m_mapped = reinterpret_cast<PB::ResourceView*>(buf->m_buffer->Map(0, UploadBufferSize));
		buf->m_allocatedIndexCount = 0;
		buf->m_maxIndexCount = MaxIndexCount;
	}

	PB::ResourceView* AssetStreamer::AllocateIndexUpload(PB::IBufferObject* dstBuffer, uint32_t dstOffset, uint32_t indexCount)
	{
		static constexpr uint32_t MaxUploadRegionCount = 64;

		if (indexCount == 0)
			return nullptr;

		if (m_indexUploadSrcBuffers.Count() > 0)
		{
			auto* uploadBuffer = m_indexUploadSrcBuffers.Back();
			if (uploadBuffer->m_maxIndexCount - uploadBuffer->m_allocatedIndexCount < indexCount
				|| m_indexUploadMap[{ uploadBuffer->m_buffer, dstBuffer }].Count() >= MaxUploadRegionCount)
			{
				AllocateNewUploadBuffer();
			}
		}
		else
		{
			AllocateNewUploadBuffer();
		}

		auto* uploadBuffer = m_indexUploadSrcBuffers.Back();

		PB::CopyRegion copyRegion;
		copyRegion.m_srcOffset = uploadBuffer->m_allocatedIndexCount * sizeof(PB::ResourceView);
		copyRegion.m_dstOffset = dstOffset;
		copyRegion.m_size = indexCount * sizeof(PB::ResourceView);
		m_indexUploadMap[{ uploadBuffer->m_buffer, dstBuffer }].PushBack(copyRegion);

		PB::ResourceView* allocation = uploadBuffer->m_mapped + uploadBuffer->m_allocatedIndexCount;
		uploadBuffer->m_allocatedIndexCount += indexCount;
		return allocation;
	}

	StreamingBatch* AssetStreamer::AllocStreamingBatch(uint32_t reserveResourceCount)
	{
		std::lock_guard<std::mutex> lock(m_freeBatchLock);
		if (m_freeBatches.Count() > 0)
		{
			StreamingBatch* batch = m_freeBatches.PopBack();
			batch->Reset(this);
			return batch;
		}

		StreamingBatch* batch = m_streamingBatchAllocator.Alloc<StreamingBatch>();
		batch->m_streamer = this;

		return batch;
	}

	AssetStreamer::ResourceHandle AssetStreamer::GetResourceHandle(const StreamableHandle& handle)
	{
		auto it = m_resourceMap.find(handle.m_id);
		if (it == m_resourceMap.end())
			return nullptr;

		return it->second.m_handle;
	}
}
