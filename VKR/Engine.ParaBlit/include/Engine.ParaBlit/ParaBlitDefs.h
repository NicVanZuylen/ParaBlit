#pragma once

namespace PB
{
	// Shorthand for unsigned 8-bit integer.
	typedef unsigned char u8;
	// Shorthand for unsigned 16-bit integer.
	typedef unsigned short u16;
	// Shorthand for unsigned 32-bit integer.
	typedef unsigned int u32;
	// Shorthand for unsigned 64-bit integer.
	typedef unsigned long long u64;

	// Bitmask helper class for strongly typed 'enum class' enums.
	// Intended to replace C enums where ever possible, since C enums generate unscoped enum warnings in VS 2019+.
	template<typename T, typename Val_T = u32>
	class EnumField
	{
	public:

		inline EnumField() { m_values = Val_T(); }
		inline EnumField(const Val_T& values) { m_values = values; }
		inline EnumField(const T& values) { m_values = static_cast<Val_T>(values); }
		inline EnumField(const EnumField<T, Val_T>& values) { m_values = values.m_values; }
		inline ~EnumField() = default;

		inline Val_T operator | (const T& rhs) const { return m_values | static_cast<Val_T>(rhs); }
		inline Val_T operator & (const T& rhs) const { return m_values & static_cast<Val_T>(rhs); }
		inline Val_T operator |= (const T& rhs) { return m_values |= static_cast<Val_T>(rhs); }
		inline Val_T operator &= (const T& rhs) { return m_values &= static_cast<Val_T>(rhs); }
		inline Val_T operator = (const T& rhs) { return m_values = static_cast<Val_T>(rhs); }

		inline Val_T operator | (const EnumField<T, Val_T>& rhs) const { return m_values | rhs.m_values; }
		inline Val_T operator & (const EnumField<T, Val_T>& rhs) const { return m_values & rhs.m_values; }
		inline Val_T operator |= (const EnumField<T, Val_T>& rhs) { return m_values |= rhs.m_values; }
		inline Val_T operator &= (const EnumField<T, Val_T>& rhs) { return m_values &= rhs.m_values; };
		inline Val_T operator = (const EnumField<T, Val_T>& rhs) { return m_values = rhs.m_values; }

		inline Val_T operator = (const Val_T& rhs) { return m_values = rhs; }

		inline operator Val_T() const { return m_values; }
		inline operator T() const { return static_cast<T>(m_values); }

		inline bool operator == (const EnumField<T, Val_T>& rhs) { return m_values == rhs.m_values; };
		inline bool operator == (const T& rhs) { return m_values == static_cast<Val_T>(rhs); };
		inline bool operator == (const Val_T& rhs) { return m_values == rhs; };

	private:
		Val_T m_values;
	};

// Used to define bit field enum class types with basic operators for the enum class type.
#define PB_DEFINE_ENUM_FIELD(name, enumType, numType)																					\
	using name = PB::EnumField<enumType, numType>;																						\
	inline name operator | (const enumType& lhs, const enumType& rhs) { return static_cast<numType>(lhs) | static_cast<numType>(rhs); } \
	inline name operator & (const enumType& lhs, const enumType& rhs) { return static_cast<numType>(lhs) & static_cast<numType>(rhs); } \
														

	enum class EMemoryType : u16
	{
		HOST_VISIBLE,
		DEVICE_LOCAL,
		END_RANGE
	};

	enum class ETextureState : u16
	{
		NONE			=	0,
		STORAGE			=	1,
		COLORTARGET		=	1 << 1,
		DEPTHTARGET		=	1 << 2,
		READ_ONLY_DEPTH_STENCIL =	1 << 3,
		SAMPLED			=	1 << 4,
		COPY_SRC		=	1 << 5,
		COPY_DST		=	1 << 6,
		READBACK		=	1 << 7,
		PRESENT			=	1 << 8,
		MAX				=	1 << 9
	};
	PB_DEFINE_ENUM_FIELD(TextureStateFlags, ETextureState, u16)

	static constexpr u16 HDRStart = 1024;
	static constexpr u16 HDREnd = 2047;
	static constexpr u16 BlockCompressedStart = 2048;
	static constexpr u16 BlockCompressedEnd = 3071;
	enum class ETextureFormat : u16
	{
		UNKNOWN = 0,

		// SDR Formats
		R8_UNORM,
		R8G8_UNORM,
		R8G8B8_UNORM,
		R8G8B8A8_UNORM,
		B8G8R8A8_UNORM,
		R8_SRGB,
		R8G8_SRGB,
		R8G8B8_SRGB,
		R8G8B8A8_SRGB,
		B8G8R8A8_SRGB,

		// HDR Formats
		R16_FLOAT = HDRStart,
		R16G16_FLOAT,
		R16G16B16_FLOAT,
		R16G16B16A16_FLOAT,
		R32_FLOAT,
		R32G32_FLOAT,
		R32G32B32_FLOAT,
		R32G32B32A32_FLOAT,
		D16_UNORM,
		D16_UNORM_S8_UINT,
		D24_UNORM_S8_UINT,
		D32_FLOAT,
		D32_FLOAT_S8_UINT,

		// Block Compressed Formats
		BC3_SRGB = BlockCompressedStart,
		BC5_UNORM,
		BC6H_RGB_U16F,
		BC6H_RGB_S16F,
		BC7_UNORM,
		BC7_SRGB,
	};

	enum class EAttachmentUsage : u8
	{
		NONE					=	0,
		COLOR					=	1,
		DEPTHSTENCIL			=	1 << 1,
		READ_ONLY_DEPTHSTENCIL	=	1 << 2,
		READ					=	1 << 3,
		STORAGE					=	1 << 4,
	};
	PB_DEFINE_ENUM_FIELD(AttachmentUsageFlags, EAttachmentUsage, u8)

	enum class EAttachmentAction : u8
	{
		NONE,
		CLEAR,
		LOAD
	};

	enum class EBufferOptions : u32
	{
		ZERO_INITIALIZE		=	1,
		CPU_ACCESSIBLE		=	1 << 1,
		POOL_PLACED			=	1 << 2,
	};
	PB_DEFINE_ENUM_FIELD(BufferOptionFlags, EBufferOptions, u32)

	enum class EBufferUsage : u32
	{
		UNIFORM			=	1,
		STORAGE			=	1 << 1,
		COPY_SRC		=	1 << 2,
		COPY_DST		=	1 << 3,
		VERTEX			=	1 << 4,
		INDEX			=	1 << 5,
		INDIRECT_PARAMS =	1 << 6
	};
	PB_DEFINE_ENUM_FIELD(BufferUsageFlags, EBufferUsage, u32)

	enum class EVertexAttributeType : u16
	{
		NONE,
		FLOAT		=	sizeof(float),
		FLOAT2		=	sizeof(float) * 2,
		FLOAT3		=	sizeof(float) * 3,
		FLOAT4		=	sizeof(float) * 4,
		MAT4		=	sizeof(float) * 5
	};

	enum class ECompareOP : u8
	{
		ALWAYS,
		NEVER,
		LEQUAL,
		LESS,
		EQUAL,
		GREQUAL,
		GREATER
	};

	template<typename T>
	struct TVec4
	{
		union
		{
			struct { T r, g, b, a; };
			struct { T x, y, z, w; };
			struct { T u, v; };
			float m_data[4];
		};

		TVec4() = default;
		TVec4(T nX, T nY, T nZ, T nW) { x = nX; y = nY; z = nZ; w = nW; }

		inline T& operator[](const u32& index) { return m_data[index]; }
	};
	using Float4 = TVec4<float>;
	using Uint4 = TVec4<u32>;

	template<typename T>
	struct TVec3
	{
		union
		{
			struct { T r, g, b; };
			struct { T x, y, z; };
			struct { T u, v; };
			float m_data[3]{};
		};

		TVec3() = default;
		TVec3(T nX, T nY, T nZ) { x = nX; y = nY; z = nZ; };

		inline T& operator[](const u32& index) { return m_data[index]; }
	};
	using Float3 = TVec3<float>;
	using Uint3 = TVec3<u32>;

	template<typename T>
	struct TVec2
	{
		union
		{
			struct { T r, g; };
			struct { T x, y; };
			struct { T u, v; };
			float m_data[2]{};
		};

		TVec2() = default;
		TVec2(T nX, T nY) { x = nX; y = nY; }

		inline T& operator[](const u32& index) { return m_data[index]; }
	};
	using Float2 = TVec2<float>;
	using Uint2 = TVec2<u32>;

	struct Rect
	{
		int x, y;
		u32 w, h;
	};

	enum class ECmdContextState : u8
	{
		OPEN,
		RECORDING,
		PENDING_SUBMISSION,
		END_RANGE
	};

	enum class ECommandContextUsage : u8
	{
		GRAPHICS,
		COMPUTE,
		COPY,
		END_RANGE
	};

	enum class ECommandContextFlags : u8
	{
		NONE,
		PRIORITY		=	1,
		REUSABLE		=	1 << 1,
		END_RANGE
	};
	PB_DEFINE_ENUM_FIELD(CommandContextFlags, ECommandContextFlags, u8)

	enum EGraphicsShaderStage : u32
	{
		VERTEX,
		TASK,
		MESH,
		FRAGMENT,
		GRAPHICS_STAGE_COUNT
	};

	// Opaque view handle for uniform buffer views.
	using UniformBufferView = void*;

	// Opaque view handle used primarily for render target views, but can be used to store textures as generic resource views too, but a static cast is required to convert them back.
	using RenderTargetView = u64;

	// Resource view index which is sent to the shader via push constants upon binding. Used for Textures, Samplers and Storage Buffers.
	using ResourceView = u32;

	// Opaque handle for a render pass object.
	using RenderPass = void*;

	// Opaque handle for a framebuffer object.
	using Framebuffer = void*;

	// Opaque handle for a shader module.
	using ShaderModule = u64;

	// Opaque handle for a pipeline state object.
	using Pipeline = u64;

	// Defines a selection of subresources within a resource.
	struct SubresourceRange
	{
		bool operator == (const SubresourceRange& other) const;

		static SubresourceRange All()
		{
			SubresourceRange ret;
			ret.m_baseMip = 0;
			ret.m_mipCount = ~u16(0);
			ret.m_firstArrayElement = 0;
			ret.m_arrayCount = ~u16(0);

			return ret;
		}

		bool AllMipLevels() const { return m_mipCount == 0xFFFF; }
		bool AllArrayLayers() const { return m_arrayCount == 0xFFFF; }

		u16 m_baseMip = 0;
		u16 m_mipCount = 1;
		u16 m_firstArrayElement = 0;
		u16 m_arrayCount = 1;
	};

	// Contains views for binding to a pipeline state. 
	struct BindingLayout
	{
		u32 m_uniformBufferCount = 0;
		u32 m_resourceCount = 0;
		UniformBufferView* m_uniformBuffers = nullptr;
		ResourceView* m_resourceViews = nullptr;
	};

	struct DrawIndexedIndirectParams
	{
		u32 m_offset = 0;
		u32 m_indexCount = 0;
		u32 m_instanceCount = 0;
		u32 m_firstIndex = 0;
		u32 m_vertexOffset = 0;
		u32 m_firstInstance = 0;
	};

	struct DrawMeshTasksIndirectParams
	{
		u32 m_workGroupX = 0;
		u32 m_workGroupY = 0;
		u32 m_workGroupZ = 0;
	};

	struct DeviceLimitations
	{
		bool m_supportMeshShader = false;
	};
}