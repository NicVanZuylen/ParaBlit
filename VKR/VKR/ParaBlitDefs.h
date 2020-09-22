#pragma once

namespace PB
{
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef unsigned long long u64;

	// Bitmask helper class for strongly typed 'enum class' enums.
	template<typename T, typename Val_T = u32>
	class BitField
	{
	public:

		inline BitField() { m_values = Val_T(); }
		inline BitField(const Val_T& values) { m_values = values; }
		inline BitField(const T& values) { m_values = static_cast<Val_T>(values); }
		inline BitField(const BitField<T, Val_T>& values) { m_values = values.m_values; }
		inline ~BitField() = default;

		inline Val_T operator | (const T& rhs) const { return m_values | static_cast<Val_T>(rhs); }
		inline Val_T operator & (const T& rhs) const { return m_values & static_cast<Val_T>(rhs); }
		inline Val_T operator |= (const T& rhs) { return m_values |= static_cast<Val_T>(rhs); }
		inline Val_T operator &= (const T& rhs) { return m_values &= static_cast<Val_T>(rhs); }
		inline Val_T operator = (const T& rhs) { return m_values = static_cast<Val_T>(rhs); }

		inline Val_T operator | (const BitField<T, Val_T>& rhs) const { return m_values | rhs.m_values; }
		inline Val_T operator & (const BitField<T, Val_T>& rhs) const { return m_values & rhs.m_values; }
		inline Val_T operator |= (const BitField<T, Val_T>& rhs) { return m_values |= rhs.m_values; }
		inline Val_T operator &= (const BitField<T, Val_T>& rhs) { return m_values &= rhs.m_values; };
		inline Val_T operator = (const BitField<T, Val_T>& rhs) { return m_values = rhs.m_values; }

		inline Val_T operator = (const Val_T& rhs) { return m_values = rhs; }

		inline operator Val_T() const { return m_values; }

	private:
		Val_T m_values;
	};

	enum EMemoryType : u16
	{
		PB_MEMORY_TYPE_HOST_VISIBLE,
		PB_MEMORY_TYPE_DEVICE_LOCAL,
		PB_MEMORY_TYPE_END_RANGE
	};

	typedef u16 ETextureStateFlags;
	enum ETextureState : ETextureStateFlags
	{
		PB_TEXTURE_STATE_NONE = 0,
		PB_TEXTURE_STATE_RAW = 1,
		PB_TEXTURE_STATE_COLORTARGET = 1 << 1,
		PB_TEXTURE_STATE_DEPTHTARGET = 1 << 2,
		PB_TEXTURE_STATE_SAMPLED = 1 << 3,
		PB_TEXTURE_STATE_COPY_SRC = 1 << 4,
		PB_TEXTURE_STATE_COPY_DST = 1 << 5,
		PB_TEXTURE_STATE_PRESENT = 1 << 6,
		PB_TEXTURE_STATE_MAX = 1 << 7
	};

	enum ETextureFormat : u16
	{
		PB_TEXTURE_FORMAT_UNKNOWN = 0,
		PB_TEXTURE_FORMAT_R8_UNORM,
		PB_TEXTURE_FORMAT_R8G8_UNORM,
		PB_TEXTURE_FORMAT_R8G8B8_UNORM,
		PB_TEXTURE_FORMAT_R8G8B8A8_UNORM,
		PB_TEXTURE_FORMAT_B8G8R8A8_UNORM,
		PB_TEXTURE_FORMAT_D16_UNORM,
		PB_TEXTURE_FORMAT_D16_UNORM_S8_UINT,
		PB_TEXTURE_FORMAT_D24_UNORM_S8_UINT,
		PB_TEXTURE_FORMAT_D32_FLOAT,
		PB_TEXTURE_FORMAT_D32_FLOAT_S8_UINT,
	};

	enum EAttachmentUsage : u8
	{
		PB_ATTACHMENT_USAGE_NONE = 0,
		PB_ATTACHMENT_USAGE_COLOR = 1,
		PB_ATTACHMENT_USAGE_DEPTHSTENCIL = 1 << 1,
		PB_ATTACHMENT_USAGE_READ = 1 << 2
	};

	enum EAttachmentAction : u8
	{
		PB_ATTACHMENT_START_ACTION_NONE,
		PB_ATTACHMENT_START_ACTION_CLEAR,
		PB_ATTACHMENT_START_ACTION_LOAD
	};

	enum EBufferOptions
	{
		PB_BUFFER_OPTION_ZERO_INITIALIZE = 1,
		PB_BUFFER_OPTION_CPU_ACCESSIBLE = 1 << 1,
	};

	enum EBufferUsage
	{
		PB_BUFFER_USAGE_UNIFORM = 1,
		PB_BUFFER_USAGE_STORAGE = 1 << 1,
		PB_BUFFER_USAGE_COPY_SRC = 1 << 2,
		PB_BUFFER_USAGE_COPY_DST = 1 << 3,
		PB_BUFFER_USAGE_VERTEX = 1 << 4,
		PB_BUFFER_USAGE_INDEX = 1 << 5
	};

	enum EVertexAttributeType
	{
		PB_VERTEX_ATTRIBUTE_NONE,
		PB_VERTEX_ATTRIBUTE_FLOAT = sizeof(float),
		PB_VERTEX_ATTRIBUTE_FLOAT2 = sizeof(float) * 2,
		PB_VERTEX_ATTRIBUTE_FLOAT3 = sizeof(float) * 3,
		PB_VERTEX_ATTRIBUTE_FLOAT4 = sizeof(float) * 4,
	};

	enum ECompareOP : u8
	{
		PB_COMPARE_OP_ALWAYS,
		PB_COMPARE_OP_NEVER,
		PB_COMPARE_OP_LEQUAL,
		PB_COMPARE_OP_LESS,
		PB_COMPARE_OP_EQUAL,
		PB_COMPARE_OP_GREQUAL,
		PB_COMPARE_OP_GREATER
	};

	struct Float4
	{
		union
		{
			struct
			{	
				float r, g, b, a;
			};
			struct
			{
				float x, y, z, w;
			};
			float m_data[4];
		};

		inline float& operator[](const u32& index)
		{
			return m_data[index];
		}
	};

	struct Rect
	{
		u32 x, y, w, h;
	};

	enum ECmdContextState : u8
	{
		PB_COMMAND_CONTEXT_STATE_OPEN,
		PB_COMMAND_CONTEXT_STATE_RECORDING,
		PB_COMMAND_CONTEXT_STATE_PENDING_SUBMISSION,
		PB_COMMAND_CONTEXT_STATE_MAX
	};

	enum ECommandContextUsage : u8
	{
		PB_COMMAND_CONTEXT_USAGE_GRAPHICS,
		PB_COMMAND_CONTEXT_USAGE_COMPUTE,
		PB_COMMAND_CONTEXT_USAGE_COPY,
		PB_COMMAND_CONTEXT_USAGE_MAX
	};

	enum ECommandContextFlags : u8
	{
		PB_COMMAND_CONTEXT_NONE,
		PB_COMMAND_CONTEXT_PRIORITY,
		PB_COMMAND_CONTEXT_MAX
	};

	enum EShaderStage : u32
	{
		PB_SHADER_STAGE_VERTEX,
		PB_SHADER_STAGE_FRAGMENT,
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
	using BufferUsage = u32;

	struct SubresourceRange
	{
		u16 m_baseMip = 0;
		u16 m_mipCount = 1;
		u16 m_firstArrayElement = 0;
		u16 m_arrayCount = 1;

		bool operator == (const SubresourceRange& other) const;
	};

	enum EBindingLayoutLocation : u16
	{
		PB_BINDING_LAYOUT_LOCATION_DEFAULT,			// Binding indices will be recorded into a DRI buffer, a UBO bound at location 0 in the shader.
													// ParaBlit will automatically allocate create, fill and bind the buffer containing the binding indices.
		PB_BINDING_LAYOUT_LOCATION_INSTANCE			// Binding indices will be read from an instance buffer. This approach is preferable when drawing instanced geometry, to avoid a uniform buffer binding for each draw call using the layout.
	};

	struct BindingLayout
	{
		EBindingLayoutLocation m_bindingLocation = PB_BINDING_LAYOUT_LOCATION_DEFAULT;
		u16 m_bufferCount = 0;
		u16 m_textureCount = 0;
		u16 m_samplerCount = 0;
		BufferView* m_buffers = nullptr;
		TextureView* m_textures = nullptr;
		Sampler* m_samplers = nullptr;
	};
}