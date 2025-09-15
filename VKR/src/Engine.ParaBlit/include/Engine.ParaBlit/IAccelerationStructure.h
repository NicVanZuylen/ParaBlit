#pragma once
#include "ParaBlitInterface.h"
#include "ParaBlitDefs.h"

namespace PB
{
	class IRenderer;
	class IBufferObject;
	class ICommandContext;

	enum class AccelerationStructureType
	{
		BOTTOM_LEVEL,
		TOP_LEVEL,
		INVALID
	};

	struct ASGeometryTrianglesDesc
	{
		u64 vertexDataOffsetBytes = 0;
		u32 vertexStrideBytes = 0;
		u32 maxVertexCount = 0;
		u64 indexDataOffsetBytes = 0;
		u32 maxPrimitiveCount = 0;
		bool useHostVertexData = false;
		bool useHostIndexData = false;

		union
		{
			struct
			{
				IBufferObject* deviceVertexData;
				u8* hostVertexData;
			};
			u64 vertexDataPtr = 0;
		};

		union
		{
			struct
			{
				IBufferObject* deviceIndexData;
				u8* hostIndexData;
			};
			u64 indexDataPtr = 0;
		};
	};

	/*
	Description: Acceleration structure instance data format as consumed when building a top level acceleration structure (TLAS).
	*/
	struct AccelerationStructureInstance
	{
		float m_transformMatrix[3][4];
		u32 m_instanceCustomIndex : 24;
		u32 m_mask : 8;
		u32 m_instanceShaderBindingTableRecordOffset : 24;
		u32 m_flags : 8;
		u64 m_accelerationStructureAddress;
	};

	struct ASGeometryInstancesDesc
	{
		u64 instanceDataOffsetBytes = 0;
		u32 maxInstanceCount = 0;
		bool useHostInstanceData = false;

		union
		{
			struct
			{
				IBufferObject* deviceInstanceData;
				AccelerationStructureInstance* hostInstanceData;
			};
			u64 instanceDataPtr = 0;
		};
	};

	union ASGeometryDesc
	{
		ASGeometryDesc() {}

		union
		{
			ASGeometryInstancesDesc instances;
			ASGeometryTrianglesDesc triangles{};
		};
	};

	struct AccelerationStructureDesc
	{
		AccelerationStructureType type = AccelerationStructureType::INVALID;
		ASGeometryDesc* geometryInputs = nullptr;
		u32 geometryInputCount = 0;
	};

	class IAccelerationStructure
	{
	public:

		PARABLIT_INTERFACE void Create(PB::IRenderer* renderer, const AccelerationStructureDesc& desc) = 0;

		PARABLIT_INTERFACE void Build(PB::ICommandContext* commandContext, u32* primitiveCounts = nullptr) = 0;

		PARABLIT_INTERFACE void Build(u32* primitiveCounts = nullptr) = 0;

		PARABLIT_INTERFACE u64 GetDeviceAddress() = 0;
	};
}