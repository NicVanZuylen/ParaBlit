#pragma once
#include "IAccelerationStructure.h"
#include "IBufferObject.h"
#include "ParaBlitApi.h"
#include "ParaBlitDebug.h"
#include "CLib/Vector.h"

namespace PB
{
	class IRenderer;
	class Renderer;
	class BufferObject;

	class AccelerationStructure : public IAccelerationStructure
	{
	public:

		AccelerationStructure() = default;

		~AccelerationStructure() = default;

		static void GetExtensionFunctions(VkInstance instance);

		void Create(PB::IRenderer* renderer, const AccelerationStructureDesc& desc) override;

		void Release();

		void Destroy();

		void Build(ICommandContext* commandContext, u32* primitiveCounts = nullptr) override;

		void Build(u32* primitiveCounts = nullptr) override;

		VkAccelerationStructureKHR GetHandle() const { return m_handle; }

		u64 GetDeviceAddress() override;

		const VkAccelerationStructureBuildGeometryInfoKHR& GetGeometryBuildInfo() const { return m_geometryInfo; }

		u32 GetGeometryCount() const { return m_geometries.Count(); }

		const BufferObject* GetStorageBuffer() const { return m_structureStorageBuffer; }

	private:

		static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHRFunc;
		static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHRFunc;
		static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHRFunc;
		static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHRFunc;

		VkAccelerationStructureTypeKHR GetVkAccelerationStructureType() 
		{
			return (m_desc.type == AccelerationStructureType::BOTTOM_LEVEL) ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		}

		AccelerationStructureDesc m_desc{};
		PB::Renderer* m_renderer = nullptr;
		PB::BufferObject* m_scratchBuffer = nullptr;
		PB::BufferObject* m_structureStorageBuffer = nullptr;
		CLib::Vector<VkAccelerationStructureGeometryKHR, 1, 2> m_geometries;
		CLib::Vector<u32, 1, 8> m_primitiveCounts;

		VkAccelerationStructureBuildGeometryInfoKHR m_geometryInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, nullptr };
		VkAccelerationStructureKHR m_handle = VK_NULL_HANDLE;
	};
};

