#include "ManagedInstanceBuffer.h"
#include "Engine.ParaBlit/ICommandContext.h"
#include "Engine.Math/Scalar.h"
#include "Shader.h"
#include <cassert>

namespace Eng
{
	ManagedInstanceBuffer::~ManagedInstanceBuffer()
	{
		FreeUploadBuffers();

		if (m_buffer != nullptr)
		{
			m_renderer->FreeBuffer(m_buffer);
			m_buffer = nullptr;
		}

		if (m_cpuInstanceData != nullptr)
		{
			m_allocator->Free(reinterpret_cast<void*>(m_cpuInstanceData));
			m_cpuInstanceData = nullptr;
		}

		m_cpuInstanceData = nullptr;
		m_renderer = nullptr;
		m_allocator = nullptr;
	}

	void ManagedInstanceBuffer::Init(PB::IRenderer* renderer, CLib::Allocator* allocator, Desc& desc)
	{
		m_desc = desc;
		m_renderer = renderer;
		m_allocator = allocator;

		m_bitfieldSize = m_desc.m_copyAll ? 0 : GetBitFieldSize(m_desc.m_elementCapacity);

		PB::u32 instanceDataSize = m_desc.m_elementSize * m_desc.m_elementCapacity;

		PB::BufferObjectDesc bufferDesc;
		bufferDesc.m_name = "ManagedInstanceBuffer::instanceBuffer";
		bufferDesc.m_bufferSize = instanceDataSize;
		bufferDesc.m_options = 0;
		bufferDesc.m_usage = m_desc.m_usage | PB::EBufferUsage::COPY_DST;
		m_buffer = m_renderer->AllocateBuffer(bufferDesc);

		AllocateUploadBuffers();

		// Compute copy shader
		if(m_desc.m_copyAll == false)
		{
			// Cull module
			{
				PB::ShaderModule cullModule = Eng::Shader(m_renderer, desc.m_cullShaderName, desc.m_cullPermutationKey, m_allocator, true).GetModule();
				PB::ComputePipelineDesc pipelineDesc{};
				pipelineDesc.m_computeModule = cullModule;

				m_instanceCullPipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
			}

			// Populate module
			{
				PB::ShaderModule populateModule = Eng::Shader(m_renderer, desc.m_populateShaderName, desc.m_populatePermutationKey, m_allocator, true).GetModule();
				PB::ComputePipelineDesc pipelineDesc{};
				pipelineDesc.m_computeModule = populateModule;

				m_instancePopulatePipeline = m_renderer->GetPipelineCache()->GetPipeline(pipelineDesc);
			}
		}
	}

	void ManagedInstanceBuffer::Resize(PB::u32 newElementCapacity)
	{
		if (m_desc.m_elementCapacity == newElementCapacity)
			return;

		PB::u32 oldInstanceDataSize = m_desc.m_elementSize * m_desc.m_elementCapacity;
		PB::u32 oldBitfieldSize = m_bitfieldSize;

		m_bitfieldSize = m_desc.m_copyAll ? 0 : GetBitFieldSize(newElementCapacity);

		PB::u32 instanceDataSize = m_desc.m_elementSize * newElementCapacity;
		PB::u32 cpuDataSize = instanceDataSize + m_bitfieldSize;
		PB::u32 minInstanceDataSize = std::min<PB::u32>(oldInstanceDataSize, instanceDataSize);
		PB::u32 minBitfieldSize = std::min<PB::u32>(oldBitfieldSize, m_bitfieldSize);

		// CPU buffer:
		{
			PB::u8* newCpuData = reinterpret_cast<PB::u8*>(m_allocator->Alloc(cpuDataSize));
			if (m_cpuInstanceData != nullptr)
			{
				std::memset(newCpuData, 0, cpuDataSize); // Zero initialize
				std::memcpy(newCpuData, m_cpuInstanceData, minInstanceDataSize); // Copy instances
				std::memcpy(newCpuData + instanceDataSize, m_cpuInstanceData + oldInstanceDataSize, minBitfieldSize); // Copy bit fields

				m_allocator->Free(reinterpret_cast<void*>(m_cpuInstanceData));
			}
			m_cpuInstanceData = newCpuData;
		}

		m_desc.m_elementCapacity = newElementCapacity;
		m_elementCount = std::min<PB::u32>(m_elementCount, newElementCapacity);

		// GPU Buffer:
		if (m_buffer != nullptr)
		{
			m_renderer->FreeBuffer(m_buffer);

			PB::BufferObjectDesc desc;
			desc.m_bufferSize = instanceDataSize;
			desc.m_options = 0;
			desc.m_usage = m_desc.m_usage | PB::EBufferUsage::COPY_DST;
			m_buffer = m_renderer->AllocateBuffer(desc);
		}

		AllocateUploadBuffers();
	}

	void ManagedInstanceBuffer::SwapStaging()
	{
		if (++m_currentStagingIndex == m_stagingBuffers.Count())
			m_currentStagingIndex = 0;
	}

	void ManagedInstanceBuffer::FlushToBuffer(FlushDesc& desc, PB::ICommandContext* dstCmdContext)
	{
		PB::u32 flushSize = m_desc.m_elementSize * m_elementCount;

		bool stagingNeedsUpload = m_currentStagingIndex != m_lastUsedStagingIndex;
		stagingNeedsUpload &= !desc.skipStagingUpload;

		PB::IBufferObject* stagingBuffer = m_stagingBuffers[m_currentStagingIndex];

		if (stagingNeedsUpload == true)
		{
			PB::u8* stagingMapped = stagingBuffer->Map(0, flushSize);
			std::memcpy(stagingMapped, GetInstanceDataBaseAddress(), flushSize);
			stagingBuffer->Unmap();
		}

		if (m_desc.m_copyAll == false)
		{
			PB::IBufferObject* bitfieldBuffer = m_bitfieldBuffers[m_currentStagingIndex];

			if (stagingNeedsUpload == true)
			{
				PB::u8* bitfieldMapped = bitfieldBuffer->Map(0, m_bitfieldSize);
				std::memcpy(bitfieldMapped, GetBitFields(), m_bitfieldSize);
				bitfieldBuffer->Unmap();
			}

			// Copy staging buffer to the final buffer using a compute shader which will also defragment instance data.
			// Do this with an upload context to ensure the copy is complete before the buffer is used.
			{
				PB::BufferViewDesc dstViewDesc(desc.dstBufferOffset, desc.dstBuffer->GetSize() - desc.dstBufferOffset);

				CLib::Vector<PB::ResourceView, 8> resources =
				{
					stagingBuffer->GetViewAsStorageBuffer(),
					desc.dstBuffer->GetViewAsStorageBuffer(dstViewDesc),
					bitfieldBuffer->GetViewAsStorageBuffer(),
					m_cullBitfieldBuffer->GetViewAsStorageBuffer(),
					m_counterBuffer->GetViewAsStorageBuffer()
				};

				PB::BindingLayout bindings{};
				if (desc.additionalBindings != nullptr)
				{
					uint32_t baseResourceCount = resources.Count();
					resources.SetCount(baseResourceCount + desc.additionalBindings->m_resourceCount);
					std::memcpy(resources.Data() + baseResourceCount, desc.additionalBindings->m_resourceViews, sizeof(PB::ResourceView) * desc.additionalBindings->m_resourceCount);

					bindings.m_uniformBufferCount = desc.additionalBindings->m_uniformBufferCount;
					bindings.m_uniformBuffers = desc.additionalBindings->m_uniformBuffers;
				}

				bindings.m_resourceCount = resources.Count();
				bindings.m_resourceViews = resources.Data();

				PB::ICommandContext* cmdContext = (dstCmdContext != nullptr) ? dstCmdContext : m_renderer->GetThreadUploadContext();

				uint32_t groupCount = m_elementCount / ComputeWorkGroupSize;
				groupCount += (m_elementCount % ComputeWorkGroupSize > 0) ? 1 : 0;

				cmdContext->CmdBeginLabel("ManagedInstanceBuffer::flush", { 1.0f, 1.0f, 1.0f, 1.0f });

				if (desc.cullPipeline != 0)
				{

					// Cull stage
					{
						cmdContext->CmdBeginLabel("ManagedInstanceBuffer::cull", { 1.0f, 1.0f, 1.0f, 1.0f });

						cmdContext->CmdBindPipeline(desc.cullPipeline);
						cmdContext->CmdBindResources(bindings);
						cmdContext->CmdDispatch(groupCount, 1, 1);

						cmdContext->CmdEndLastLabel();
					}

					// Memory barriers between Cull & Populate stages
					{
						PB::BufferMemoryBarrier barriers[] =
						{
							PB::BufferMemoryBarrier(stagingBuffer, PB::EMemoryBarrierType::COMPUTE_SHADER_WRITE_TO_COMPUTE_SHADER_READ),
							PB::BufferMemoryBarrier(m_cullBitfieldBuffer, PB::EMemoryBarrierType::COMPUTE_SHADER_WRITE_TO_COMPUTE_SHADER_READ),
							PB::BufferMemoryBarrier(desc.dstBuffer, PB::EMemoryBarrierType::COMPUTE_SHADER_WRITE_TO_COMPUTE_SHADER_READ),
							PB::BufferMemoryBarrier(m_counterBuffer, PB::EMemoryBarrierType::COMPUTE_SHADER_WRITE_TO_COMPUTE_SHADER_READ),
						};
						cmdContext->CmdBufferBarrier(barriers, PB_ARRAY_LENGTH(barriers));
					}
				}

				// Populate stage
				{
					cmdContext->CmdBeginLabel("ManagedInstanceBuffer::populate", { 1.0f, 1.0f, 1.0f, 1.0f });

					cmdContext->CmdBindPipeline(desc.populatePipeline);
					cmdContext->CmdBindResources(bindings);
					cmdContext->CmdDispatch(groupCount, 1, 1);

					cmdContext->CmdEndLastLabel();
				}

				cmdContext->CmdEndLastLabel();

				if (dstCmdContext == nullptr)
				{
					m_renderer->ReturnThreadUploadContext(cmdContext);
				}
			}
		}
		else
		{
			PB::ICommandContext* cmdContext = m_renderer->GetThreadUploadContext();

			PB::CopyRegion region;
			region.m_srcOffset = 0;
			region.m_dstOffset = desc.dstBufferOffset;
			region.m_size = flushSize;
			cmdContext->CmdCopyBufferToBuffer(stagingBuffer, desc.dstBuffer, &region, 1);

			m_renderer->ReturnThreadUploadContext(cmdContext);
		}

		m_lastUsedStagingIndex = m_currentStagingIndex;
		if (m_desc.m_autoSwapStagingOnFlush == true)
		{
			SwapStaging();
		}
	}

	void ManagedInstanceBuffer::FlushChanges(PB::ICommandContext* dstCmdContext, bool skipStagingUpload, PB::BindingLayout* additionalBindings)
	{
		FlushDesc desc;
		desc.cullPipeline = m_instanceCullPipeline;
		desc.populatePipeline = m_instancePopulatePipeline;
		desc.additionalBindings = additionalBindings;
		desc.dstBuffer = m_buffer;
		desc.dstBufferOffset = 0;
		desc.skipStagingUpload = skipStagingUpload;

		FlushToBuffer(desc, dstCmdContext);
	}

	ManagedInstanceBuffer::ManagedInstance ManagedInstanceBuffer::AddInstance()
	{
		uint32_t instanceIndex = m_elementCount;
		if(m_freeInstances.Count() > 0)
		{
			instanceIndex = m_freeInstances.PopBack();
			SetBit(instanceIndex, true);
			return instanceIndex;
		}

		if (++m_elementCount > m_desc.m_elementCapacity)
		{
			Resize(m_desc.m_elementCapacity * 2);
		}

		SetBit(instanceIndex, true);
		return instanceIndex;
	}

	void ManagedInstanceBuffer::RemoveInstance(ManagedInstance instance)
	{
		SetBit(instance, false);
		m_freeInstances.PushBack(instance);
	}

	void ManagedInstanceBuffer::SetInstanceEnable(ManagedInstance instance, bool enable)
	{
		SetBit(instance, enable);
	}

	PB::u32 ManagedInstanceBuffer::GetBitFieldSize(PB::u32 elementCapacity)
	{
		PB::u32 bitFieldSize = Math::RoundUp(elementCapacity, 32u) / 32u;
		bitFieldSize *= sizeof(PB::u32);

		return bitFieldSize;
	}

	bool ManagedInstanceBuffer::GetBit(size_t instanceIndex)
	{
		if (m_bitfieldSize == 0)
			return 0;

		uint32_t fieldIdx = instanceIndex / 32;
		uint32_t bitIdx = instanceIndex % 32;

		uint32_t* bitFields = reinterpret_cast<uint32_t*>(GetBitFields());
		return (bitFields[fieldIdx] & (1 << bitIdx)) > 0;
	}

	void ManagedInstanceBuffer::SetBit(size_t instanceIndex, bool val)
	{
		if (m_bitfieldSize == 0)
			return;

		size_t fieldIdx = instanceIndex / 32;
		size_t bitIdx = instanceIndex % 32;

		uint32_t mask = 1 << bitIdx;
		uint32_t* bitFields = reinterpret_cast<uint32_t*>(GetBitFields());
		if (val == true)
		{
			bitFields[fieldIdx] |= mask;
		}
		else
		{
			bitFields[fieldIdx] &= ~mask;
		}
	}

	PB::u32 ManagedInstanceBuffer::GetCounterBufferSize(PB::u32 elementCapacity)
	{
		PB::u32 computeWorkGroupCount = Math::RoundUp(elementCapacity, ComputeWorkGroupSize) / ComputeWorkGroupSize;

		return sizeof(PB::u32) * computeWorkGroupCount;
	}

	PB::IBufferObject* ManagedInstanceBuffer::AllocateStagingBuffer(size_t sizeBytes)
	{
		PB::u32 instanceDataSize = m_desc.m_elementSize * m_desc.m_elementCapacity;

		PB::BufferObjectDesc bufferDesc{};
		bufferDesc.m_name = "ManagedInstanceBuffer::stagingBuffer";
		bufferDesc.m_bufferSize = sizeBytes;
		bufferDesc.m_usage = m_desc.m_copyAll ? PB::EBufferUsage::COPY_SRC : PB::EBufferUsage::STORAGE;
		bufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE | PB::EBufferOptions::DEVICE_MEMORY;
		return m_renderer->AllocateBuffer(bufferDesc);
	}

	void ManagedInstanceBuffer::AllocateUploadBuffers()
	{
		FreeUploadBuffers();

		PB::u32 instanceDataSize = m_desc.m_elementSize * m_desc.m_elementCapacity;
		PB::u32 cpuDataSize = instanceDataSize + m_bitfieldSize;

		if (m_cpuInstanceData == nullptr)
		{
			m_cpuInstanceData = reinterpret_cast<PB::u8*>(m_allocator->Alloc(cpuDataSize));
			std::memset(m_cpuInstanceData, 0, cpuDataSize);
		}

		// Allocate one counter for each compute work group.
		m_counterBufferSize = GetCounterBufferSize(m_desc.m_elementCapacity);

		if (m_desc.m_copyAll == false)
		{			
			PB::BufferObjectDesc cullBitfieldDesc;
			cullBitfieldDesc.m_name = "ManagedInstanceBuffer::cullBitfieldBuffer";
			cullBitfieldDesc.m_bufferSize = m_bitfieldSize;
			cullBitfieldDesc.m_options = 0;
			cullBitfieldDesc.m_usage = PB::EBufferUsage::STORAGE;
			m_cullBitfieldBuffer = m_renderer->AllocateBuffer(cullBitfieldDesc);
		}

		PB::BufferObjectDesc bufferDesc{};
		uint32_t bufferCount = m_renderer->GetSwapchain()->GetImageCount();
		if (m_desc.m_copyAll == false)
		{
			bufferDesc = {};
			bufferDesc.m_name = "ManagedInstanceBuffer::bitfieldBuffer";
			bufferDesc.m_bufferSize = m_bitfieldSize;
			bufferDesc.m_options = PB::EBufferOptions::CPU_ACCESSIBLE | PB::EBufferOptions::DEVICE_MEMORY;
			bufferDesc.m_usage = PB::EBufferUsage::STORAGE;

			for (uint32_t i = 0; i < bufferCount; ++i)
			{
				m_bitfieldBuffers.PushBack(m_renderer->AllocateBuffer(bufferDesc));
			}

			bufferDesc = {};
			bufferDesc.m_name = "ManagedInstanceBuffer::counterBuffer";
			bufferDesc.m_bufferSize = m_counterBufferSize;
			bufferDesc.m_options = 0;
			bufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::COPY_DST;
			m_counterBuffer = m_renderer->AllocateBuffer(bufferDesc);
		}

		// Staging buffers
		{
			for (uint32_t i = 0; i < bufferCount; ++i)
			{
				m_stagingBuffers.PushBack(AllocateStagingBuffer(instanceDataSize));
			}
		}

		m_currentStagingIndex = 0;
		m_lastUsedStagingIndex = m_stagingBuffers.Count() - 1;
	}

	void ManagedInstanceBuffer::FreeUploadBuffers()
	{
		if (m_counterBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_counterBuffer);
			m_counterBuffer = nullptr;
		}

		if (m_cullBitfieldBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_cullBitfieldBuffer);
			m_cullBitfieldBuffer = nullptr;
		}

		for (auto& bitfieldBuffer : m_bitfieldBuffers)
		{
			if (bitfieldBuffer != nullptr)
			{
				m_renderer->FreeBuffer(bitfieldBuffer);
			}
		}
		m_bitfieldBuffers.Clear();

		for (auto& stagingBuffer : m_stagingBuffers)
		{
			if (stagingBuffer != nullptr)
			{
				m_renderer->FreeBuffer(stagingBuffer);
			}
		}
		m_stagingBuffers.Clear();
	}
}