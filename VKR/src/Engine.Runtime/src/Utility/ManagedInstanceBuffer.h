#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include <CLib/Allocator.h>
#include "CLib/Vector.h"

namespace Eng
{

	class ManagedInstanceBuffer
	{
	public:

		static constexpr PB::u32 ComputeWorkGroupSize = 256;

		using ManagedInstance = PB::u32;

		struct Desc
		{
			// Size of individual elements/instances within the buffer.
			PB::u32 m_elementSize = 0;

			// Maximum amount of instances the instance buffer can hold.
			PB::u32 m_elementCapacity = 0;

			// Name and permutation key of the compute shader used to cull instances.
			const char* m_cullShaderName = nullptr;
			uint64_t m_cullPermutationKey = 0;
			// Name and permutation key of the compute shader used to populate the GPU with instances which have passed the culling phase.
			const char* m_populateShaderName = nullptr;
			uint64_t m_populatePermutationKey = 0;

			// Buffer usage flags for the GPU-side instance buffer.
			PB::EBufferUsage m_usage = PB::EBufferUsage::STORAGE;

			// Whether or not to automatically swap the staging buffer upon flushing.
			// If false, swapping must be done explicitly by calling SwapStagingBuffer().
			bool m_autoSwapStagingOnFlush = true;

			// If true, copy all instance data using a GPU copy instead of a compute shader. Disabled instances are still copied.
			bool m_copyAll = false;
		};

		ManagedInstanceBuffer() = default;
		ManagedInstanceBuffer(PB::IRenderer* renderer, CLib::Allocator* allocator, Desc& desc)
		{
			Init(renderer, allocator, desc);
		}
		~ManagedInstanceBuffer();

		const Desc& GetDesc() { return m_desc; }
		PB::u32 GetInstanceCount() { return m_elementCount; }

		void Init(PB::IRenderer* renderer, CLib::Allocator* allocator, Desc& desc);
		void Resize(PB::u32 newElementCount);

		// Swap the staging buffer. If auto swapping is enabled this is not necessary and may cause redundant swaps.
		void SwapStaging();

		struct FlushDesc
		{
			PB::Pipeline cullPipeline;
			PB::Pipeline populatePipeline;
			PB::BindingLayout* additionalBindings;
			PB::IBufferObject* dstBuffer;
			PB::u32 dstBufferOffset;
			bool skipStagingUpload;
		};

		/*
		Description: Update the provided GPU-side buffer with the contents of the CPU buffer.
		Param:
			PB::IBufferObject* dstBuffer: Buffer to populate with this flush.
			PB::u32 dstBufferOffset: Write offset within the provided buffer.
			PB::BindingLayout* additionalBindings: Additional bindings for the cull and populate shaders, added after the base bindings.
		*/
		void FlushToBuffer(FlushDesc& desc, PB::ICommandContext* dstCmdContext = nullptr);

		/*
		Description: Update the interal GPU-side buffer with the contents of the CPU buffer.
		Param:
			PB::BindingLayout* additionalBindings: Additional bindings for the cull and populate shaders, added after the base bindings.
		*/
		void FlushChanges(PB::ICommandContext* dstCmdContext = nullptr, bool skipStagingUpload = false, PB::BindingLayout* additionalBindings = nullptr);

		ManagedInstance AddInstance();
		void RemoveInstance(ManagedInstance instance);
		void SetInstanceEnable(ManagedInstance instance, bool enable);
		inline PB::u8* GetInstanceData(ManagedInstance instance) { return GetInstanceDataBaseAddress() + (m_desc.m_elementSize * instance); }

		PB::IBufferObject* GetBuffer() const { return m_buffer; }

	private:

		inline PB::u8* GetBitFields() { return m_cpuInstanceData + (m_desc.m_elementSize * m_desc.m_elementCapacity); };
		PB::u32 GetBitFieldSize(PB::u32 elementCapacity);
		inline PB::u8* GetInstanceDataBaseAddress() { return m_cpuInstanceData; };
		bool GetBit(size_t instanceIndex);
		void SetBit(size_t instanceIndex, bool val);
		PB::u32 GetCounterBufferSize(PB::u32 elementCapacity);
		PB::IBufferObject* AllocateStagingBuffer(size_t sizeBytes);
		void AllocateUploadBuffers();
		void FreeUploadBuffers();

		Desc m_desc;
		CLib::Allocator* m_allocator = nullptr;
		PB::IRenderer* m_renderer = nullptr;
		PB::IBufferObject* m_buffer = nullptr;
		PB::IBufferObject* m_counterBuffer = nullptr;
		PB::IBufferObject* m_cullBitfieldBuffer = nullptr;
		PB::Pipeline m_instanceCullPipeline = 0;
		PB::Pipeline m_instancePopulatePipeline = 0;
		CLib::Vector<PB::IBufferObject*, 3> m_stagingBuffers;
		CLib::Vector<PB::IBufferObject*, 3> m_bitfieldBuffers;
		CLib::Vector<ManagedInstance, 0, 16> m_freeInstances;
		PB::u8* m_cpuInstanceData = nullptr;
		PB::u32 m_currentStagingIndex = 0;
		PB::u32 m_lastUsedStagingIndex = 0;
		PB::u32 m_elementCount = 0;
		PB::u32 m_bitfieldSize = 0;
		PB::u32 m_counterBufferSize = 0;
	};
}