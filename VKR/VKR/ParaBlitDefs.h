#pragma once

namespace PB
{
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef unsigned long long u64;

	// Bitmask helper class for strongly typed 'enum class' enums.
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

	private:
		Val_T m_values;
	};

// Used to define bit field enum class types with OR operators for the enum class type.
#define PB_DEFINE_ENUM_FIELD(name, enumType, numType) using name = EnumField<enumType, numType>; inline name operator | (const enumType& lhs, const enumType& rhs) { return static_cast<numType>(lhs) | static_cast<numType>(rhs); }

	enum class EMemoryType : u16
	{
		HOST_VISIBLE,
		DEVICE_LOCAL,
		END_RANGE
	};

	enum class ETextureState : u16
	{
		NONE			=	0,
		RAW				=	1,
		COLORTARGET		=	1 << 1,
		DEPTHTARGET		=	1 << 2,
		SAMPLED			=	1 << 3,
		COPY_SRC		=	1 << 4,
		COPY_DST		=	1 << 5,
		PRESENT			=	1 << 6,
		MAX				=	1 << 7
	};
	PB_DEFINE_ENUM_FIELD(TextureStateFlags, ETextureState, u16)

	enum class ETextureFormat : u16
	{
		UNKNOWN = 0,
		R8_UNORM,
		R8G8_UNORM,
		R8G8B8_UNORM,
		R8G8B8A8_UNORM,
		B8G8R8A8_UNORM,
		R32_FLOAT,
		R32G32_FLOAT,
		R32G32B32_FLOAT,
		R32G32B32A32_FLOAT,
		D16_UNORM,
		D16_UNORM_S8_UINT,
		D24_UNORM_S8_UINT,
		D32_FLOAT,
		D32_FLOAT_S8_UINT,
	};

	enum class EAttachmentUsage : u8
	{
		NONE			=	0,
		COLOR			=	1,
		DEPTHSTENCIL	=	1 << 1,
		READ			=	1 << 2
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
	};
	PB_DEFINE_ENUM_FIELD(BufferOptionFlags, EBufferOptions, u32)

	enum class EBufferUsage : u32
	{
		UNIFORM		=	1,
		STORAGE		=	1 << 1,
		COPY_SRC	=	1 << 2,
		COPY_DST	=	1 << 3,
		VERTEX		=	1 << 4,
		INDEX		=	1 << 5
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

	struct Float4
	{
		union
		{
			struct { float r, g, b, a; };
			struct { float x, y, z, w; };
			struct { float u, v; };
			float m_data[4];
		};

		inline float& operator[](const u32& index) { return m_data[index]; }
	};

	struct Float3
	{
		union
		{
			struct { float r, g, b; };
			struct { float x, y, z; };
			struct { float u, v; };
			float m_data[3];
		};

		inline float& operator[](const u32& index) { return m_data[index]; }
	};

	struct Float2
	{
		union
		{
			struct { float r, g; };
			struct { float x, y; };
			struct { float u, v; };
			float m_data[2];
		};

		inline float& operator[](const u32& index) { return m_data[index]; }
	};

	struct Rect
	{
		u32 x, y, w, h;
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
		END_RANGE
	};
	PB_DEFINE_ENUM_FIELD(CommandContextFlags, ECommandContextFlags, u8)

	enum EShaderStage : u32
	{
		VERTEX,
		FRAGMENT,
		PB_SHADER_STAGE_COUNT
	};

	using BufferView = void*;
	using TextureView = u64;
	using Sampler = u64;
	using RenderPass = void*;
	using Framebuffer = void*;
	using ShaderModule = u64;
	using Pipeline = u64;

	using BufferOptions = u32;

	struct SubresourceRange
	{
		u16 m_baseMip = 0;
		u16 m_mipCount = 1;
		u16 m_firstArrayElement = 0;
		u16 m_arrayCount = 1;

		bool operator == (const SubresourceRange& other) const;
	};

	enum class EBindingLayoutLocation : u16
	{
		DEFAULT,			// Binding indices will be recorded into a DRI buffer, a UBO bound at location 0 in the shader.
							// ParaBlit will automatically allocate create, fill and bind the buffer containing the binding indices.

		INSTANCE			// Binding indices will be read from an instance buffer. This approach is preferable when drawing instanced geometry, to avoid a uniform buffer binding for each draw call using the layout.
	};

	struct BindingLayout
	{
		EBindingLayoutLocation m_bindingLocation = EBindingLayoutLocation::DEFAULT;

		u16 m_bufferCount = 0;
		BufferView* m_buffers = nullptr;

		u16 m_textureCount = 0;
		TextureView* m_textures = nullptr;

		u16 m_samplerCount = 0;
		Sampler* m_samplers = nullptr;
	};
}