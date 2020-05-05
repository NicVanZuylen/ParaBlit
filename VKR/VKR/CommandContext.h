#pragma once
#include "ParaBlitApi.h"
#include "vulkan/vulkan.h"

#define PB_LOG_COMMAND_CONTEXT_STATE 1

#if PB_LOG_COMMAND_CONTEXT_STATE
#define PB_COMMAND_CONTEXT_LOG PB_LOG_FORMAT
#else
#define PB_COMMAND_CONTEXT_LOG
#endif

namespace PB 
{
	class IRenderer;
	class Renderer;

	struct Float4
	{
		float r, g, b, a;
	};

	struct Rect
	{
		u32 x, y, w, h;
	};

	struct ClearDesc
	{
		Float4 m_color;
		Rect m_region;
		u32 m_attachmentIndex;
	};

	enum ECmdContextState : u8
	{
		PB_COMMAND_CONTEXT_STATE_OPEN,
		PB_COMMAND_CONTEXT_STATE_RECORDING,
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

	struct CommandContextDesc
	{
		IRenderer* m_renderer = nullptr;
		ECommandContextUsage m_usage = PB_COMMAND_CONTEXT_USAGE_GRAPHICS;
		ECommandContextFlags m_flags = PB_COMMAND_CONTEXT_NONE;
	};

	class CommandContext
	{
	public:
		PARABLIT_API CommandContext();

		PARABLIT_API ~CommandContext();

		PARABLIT_API void Init(CommandContextDesc& desc);

		PARABLIT_API void Begin();

		PARABLIT_API void End();

		PARABLIT_API ECmdContextState GetState();

		PARABLIT_API VkCommandBuffer GetCmdBuffer();

		PARABLIT_API void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount);

	private:

		PARABLIT_API inline void ValidateRecordingState();

		Renderer* m_renderer = nullptr;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		ECmdContextState m_state = PB_COMMAND_CONTEXT_STATE_OPEN;
		ECommandContextUsage m_usage = PB_COMMAND_CONTEXT_USAGE_COPY; // Copy by default since any device queue is capable of copy operations.
		bool m_priority;
	};
}

