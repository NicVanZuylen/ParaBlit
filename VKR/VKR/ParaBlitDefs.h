#pragma once

namespace PB
{
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef unsigned long long u64;

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
		PB_TEXTURE_FORMAT_B8G8R8A8_UNORM
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
		PB_BUFFER_USAGE_COPY_DST = 1 << 3
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

	using TextureView = void*;
	using BufferView = void*;
	using RenderPass = void*;
	using Framebuffer = void*;
	using Sampler = void*;
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
		PB_BINDING_LAYOUT_LOCATION_UNIFORM_BUFFER,	// Binding indices will be read from a bound uniform buffer. This approach is more useful when not drawing instanced geometry.
													// ParaBlit will automatically allocate create, fill and bind the buffer containing the binding indices.
		PB_BINDING_LAYOUT_LOCATION_INSTANCE			// Binding indices will be read from an instance buffer. This approach is preferable when drawing instanced geometry, to avoid a uniform buffer binding for each draw call using the layout.
	};

	struct BindingLayout
	{
		EBindingLayoutLocation m_bindingLocation = PB_BINDING_LAYOUT_LOCATION_UNIFORM_BUFFER;
		u16 bufferCount = 0;
		u16 textureCount = 0;
		u16 samplerCount = 0;
		BufferView* m_buffers = nullptr;
		TextureView* m_textures = nullptr;
		Sampler* m_samplers = nullptr;
	};
}