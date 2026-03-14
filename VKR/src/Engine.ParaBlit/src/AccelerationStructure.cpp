#include "AccelerationStructure.h"
#include "ParaBlitDefs.h"
#include "Renderer.h"
#include "Device.h"
#include "CommandContext.h"

namespace PB
{
	PFN_vkGetAccelerationStructureBuildSizesKHR AccelerationStructure::vkGetAccelerationStructureBuildSizesKHRFunc;
	PFN_vkCreateAccelerationStructureKHR AccelerationStructure::vkCreateAccelerationStructureKHRFunc;
	PFN_vkDestroyAccelerationStructureKHR AccelerationStructure::vkDestroyAccelerationStructureKHRFunc;
	PFN_vkGetAccelerationStructureDeviceAddressKHR AccelerationStructure::vkGetAccelerationStructureDeviceAddressKHRFunc;

	class ASDeferredDeletion : public DeferredDeletion
	{
	public:

		ASDeferredDeletion(AccelerationStructure* as)
			: m_as(as)
		{
		}
		~ASDeferredDeletion() = default;

		void OnDelete(CLib::Allocator& allocator) override
		{
			m_as->Release();

			allocator.Free(m_as);
			allocator.Free(this);
		}

	private:

		AccelerationStructure* m_as;
	};

	void AccelerationStructure::GetExtensionFunctions(VkInstance instance)
	{
		vkGetAccelerationStructureBuildSizesKHRFunc = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureBuildSizesKHR");
		vkCreateAccelerationStructureKHRFunc = (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(instance, "vkCreateAccelerationStructureKHR");
		vkDestroyAccelerationStructureKHRFunc = (PFN_vkDestroyAccelerationStructureKHR)vkGetInstanceProcAddr(instance, "vkDestroyAccelerationStructureKHR");
		vkGetAccelerationStructureDeviceAddressKHRFunc = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureDeviceAddressKHR");
	}

	void AccelerationStructure::Create(PB::IRenderer* renderer, const AccelerationStructureDesc& desc)
	{
		m_desc = desc;
		m_renderer = reinterpret_cast<PB::Renderer*>(renderer);

		PB::Device* device = m_renderer->GetDevice();
		if (device->GetDeviceLimitations()->m_supportRaytracing == false)
			return;

		PB_ASSERT(desc.type != AccelerationStructureType::INVALID);

		for (uint32_t i = 0; i < desc.geometryInputCount; ++i)
		{
			ASGeometryDesc& inGeo = desc.geometryInputs[i];

			if (desc.type == AccelerationStructureType::BOTTOM_LEVEL)
			{
				ASGeometryTrianglesDesc& inTriGeo = inGeo.triangles;
				auto& geo = m_geometries.PushBack();
				geo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				geo.pNext = nullptr;
				geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
				geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

				m_primitiveCounts.PushBack(inTriGeo.maxPrimitiveCount);

				VkAccelerationStructureGeometryTrianglesDataKHR& triangleData = geo.geometry.triangles;
				triangleData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				triangleData.pNext = nullptr;

				triangleData.indexType = VK_INDEX_TYPE_UINT32;
				triangleData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				triangleData.vertexStride = inTriGeo.vertexStrideBytes;
				triangleData.maxVertex = inTriGeo.maxVertexCount - 1;
				triangleData.transformData.deviceAddress = 0;

				PB_ASSERT(inTriGeo.indexDataPtr > 0);
				if (inTriGeo.useHostIndexData)
				{
					triangleData.indexData.hostAddress = inTriGeo.hostIndexData + inTriGeo.indexDataOffsetBytes;
				}
				else
				{
					triangleData.indexData.deviceAddress = reinterpret_cast<BufferObject*>(inTriGeo.deviceIndexData)->GetDeviceAddress() + inTriGeo.indexDataOffsetBytes;
				}

				PB_ASSERT(inTriGeo.maxVertexCount > 0);
				PB_ASSERT(inTriGeo.vertexDataPtr > 0);
				if (inTriGeo.useHostVertexData)
				{
					triangleData.vertexData.hostAddress = inTriGeo.hostVertexData + inTriGeo.vertexDataOffsetBytes;
				}
				else
				{
					triangleData.vertexData.deviceAddress = reinterpret_cast<BufferObject*>(inTriGeo.deviceVertexData)->GetDeviceAddress() + inTriGeo.vertexDataOffsetBytes;
				}
			}
			else if (desc.type == AccelerationStructureType::TOP_LEVEL)
			{
				ASGeometryInstancesDesc& inInstanceGeo = inGeo.instances;
				auto& geo = m_geometries.PushBack();
				geo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				geo.pNext = nullptr;
				geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
				geo.flags = 0;

				m_primitiveCounts.PushBack(inInstanceGeo.maxInstanceCount);

				VkAccelerationStructureGeometryInstancesDataKHR& instances = geo.geometry.instances;
				instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
				instances.pNext = nullptr;
				instances.arrayOfPointers = VK_FALSE;

				PB_ASSERT(inInstanceGeo.instanceDataPtr > 0);
				if (inInstanceGeo.useHostInstanceData)
				{
					instances.data.hostAddress = reinterpret_cast<u8*>(inInstanceGeo.hostInstanceData) + inInstanceGeo.instanceDataOffsetBytes;
				}
				else
				{
					instances.data.deviceAddress = reinterpret_cast<BufferObject*>(inInstanceGeo.deviceInstanceData)->GetDeviceAddress() + inInstanceGeo.instanceDataOffsetBytes;
				}
			}
		}

		m_geometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
		m_geometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		m_geometryInfo.pGeometries = m_geometries.Data();
		m_geometryInfo.ppGeometries = nullptr;
		m_geometryInfo.geometryCount = m_geometries.Count();
		m_geometryInfo.scratchData.hostAddress = nullptr;
		m_geometryInfo.type = GetVkAccelerationStructureType();
		m_geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		m_geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

		PB_ASSERT(m_primitiveCounts.Count() == m_geometries.Count());

		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr };
		vkGetAccelerationStructureBuildSizesKHRFunc(device->GetHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &m_geometryInfo, m_primitiveCounts.Data(), &buildSizesInfo);

		// Create buffer for acceleration structure storage
		{
			BufferObjectDesc storageBufferDesc{};
			storageBufferDesc.m_name = m_desc.type == AccelerationStructureType::BOTTOM_LEVEL ? "PB::AccelerationStructure::blasBuffer" : "PB::AccelerationStructure::tlasBuffer";
			storageBufferDesc.m_usage = PB::EBufferUsage::ACCELERATION_STRUCTURE_STORAGE | PB::EBufferUsage::MEMORY_ADDRESS_ACCESS;
			storageBufferDesc.m_options = 0;
			storageBufferDesc.m_bufferSize = buildSizesInfo.accelerationStructureSize;
			m_structureStorageBuffer = reinterpret_cast<BufferObject*>(m_renderer->AllocateBuffer(storageBufferDesc));
		}

		// Create build scratch buffer
		{
			BufferObjectDesc scratchBufferDesc{};
			scratchBufferDesc.m_name = "PB::AccelerationStructure::scratchBuffer";
			scratchBufferDesc.m_usage = PB::EBufferUsage::STORAGE | PB::EBufferUsage::MEMORY_ADDRESS_ACCESS;
			scratchBufferDesc.m_options = 0;
			scratchBufferDesc.m_bufferSize = buildSizesInfo.buildScratchSize;
			m_scratchBuffer = reinterpret_cast<BufferObject*>(m_renderer->AllocateBuffer(scratchBufferDesc));
		}
		m_geometryInfo.scratchData.deviceAddress = m_scratchBuffer->GetDeviceAddress();

		// Create acceleration structure
		{
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, nullptr };
			createInfo.type = GetVkAccelerationStructureType();
			createInfo.createFlags = 0;
			createInfo.buffer = m_structureStorageBuffer->GetHandle();
			createInfo.deviceAddress = 0;
			createInfo.offset = 0;
			createInfo.size = buildSizesInfo.accelerationStructureSize;

			PB_ERROR_CHECK(vkCreateAccelerationStructureKHRFunc(device->GetHandle(), &createInfo, nullptr, &m_handle));
			PB_BREAK_ON_ERROR;

			m_geometryInfo.dstAccelerationStructure = m_handle;
		}
	}

	void AccelerationStructure::Destroy()
	{
		m_renderer->AddDeferredDeletion(m_renderer->GetAllocator().Alloc<ASDeferredDeletion>(this));
	}

	void AccelerationStructure::Release()
	{
		if (m_handle != VK_NULL_HANDLE)
		{
			vkDestroyAccelerationStructureKHRFunc(m_renderer->GetDevice()->GetHandle(), m_handle, nullptr);
			m_handle = VK_NULL_HANDLE;
		}

		if (m_scratchBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_scratchBuffer);
		}

		if (m_structureStorageBuffer != nullptr)
		{
			m_renderer->FreeBuffer(m_structureStorageBuffer);
		}
	}

	void AccelerationStructure::Build(PB::ICommandContext* commandContext, u32* primitiveCounts)
	{
		CommandContext* cmdContextInternal = reinterpret_cast<CommandContext*>(commandContext);

		// The input vertex and index buffers are assumed to be copied/loaded into to immediately before building the acceleration structure.
		// Therefore we will make sure those copies are finished before building.
		{
			CLib::Vector<VkBufferMemoryBarrier, 2, 4> barriers;
			for (uint32_t i = 0; i < m_desc.geometryInputCount; ++i)
			{
				BufferObject* vertexBuffer = reinterpret_cast<BufferObject*>(m_desc.geometryInputs[i].triangles.deviceVertexData);
				BufferObject* indexBuffer = reinterpret_cast<BufferObject*>(m_desc.geometryInputs[i].triangles.deviceIndexData);

				VkBufferMemoryBarrier& vertexCopyToBuildBarrier = barriers.PushBack();
				vertexCopyToBuildBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				vertexCopyToBuildBarrier.pNext = nullptr;
				vertexCopyToBuildBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				vertexCopyToBuildBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
				vertexCopyToBuildBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				vertexCopyToBuildBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				vertexCopyToBuildBarrier.buffer = vertexBuffer->GetHandle();
				vertexCopyToBuildBarrier.offset = 0;
				vertexCopyToBuildBarrier.size = VK_WHOLE_SIZE;

				// Also add an identical barrier for index data.
				VkBufferMemoryBarrier& indexCopyToBuildBarrier = barriers.PushBack();
				indexCopyToBuildBarrier = vertexCopyToBuildBarrier;
				indexCopyToBuildBarrier.buffer = indexBuffer->GetHandle();
			}

			vkCmdPipelineBarrier
			(
				cmdContextInternal->GetCmdBuffer(),
				VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				0,
				0,
				nullptr, 
				barriers.Count(), 
				barriers.Data(),
				0, 
				nullptr
			);
		}

		cmdContextInternal->CmdBuildAccelerationStructure(this, primitiveCounts ? primitiveCounts : m_primitiveCounts.Data());
	}

	void AccelerationStructure::Build(u32* primitiveCounts)
	{
		CommandContext* internalContext = Renderer::t_threadResourceInitializationCommandContext.Get(m_renderer);
		Build(internalContext, primitiveCounts);
		m_renderer->ReturnThreadUploadContext(internalContext);
	}

	u64 AccelerationStructure::GetDeviceAddress()
	{
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr };
		addressInfo.accelerationStructure = m_handle;

		return vkGetAccelerationStructureDeviceAddressKHRFunc(m_renderer->GetDevice()->GetHandle(), &addressInfo);
	}
}
