#pragma once
#include "ParaBlitApi.h"
#include "ParaBlitInterface.h"
#include "ITexture.h"

namespace PB
{
	class IRenderer;

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

	struct SubresourceRange
	{
		u16 m_baseMip = 0;
		u16 m_mipCount = 1;
		u16 m_firstArrayElement = 0;
		u16 m_arrayCount = 1;
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

	struct CommandContextDesc
	{
		IRenderer* m_renderer = nullptr;
		ECommandContextUsage m_usage = PB_COMMAND_CONTEXT_USAGE_GRAPHICS;
		ECommandContextFlags m_flags = PB_COMMAND_CONTEXT_NONE;
	};

	class ICommandContext
	{
	public:

		PARABLIT_API ICommandContext();

		PARABLIT_API ~ICommandContext();

		PARABLIT_INTERFACE void Init(CommandContextDesc& desc) = 0;

		PARABLIT_INTERFACE void Begin() = 0;

		PARABLIT_INTERFACE void End() = 0;

		PARABLIT_INTERFACE void Return() = 0;

		PARABLIT_INTERFACE void CmdClearColorTargets(ClearDesc* clearColors, u32 targetCount) = 0;

		PARABLIT_INTERFACE void CmdTransitionTexture(ITexture* texture, ETextureState newState, SubresourceRange subResourceRange = {}) = 0;
	};

	ICommandContext* CreateCommandContext(IRenderer* renderer);

	void DestroyCommandContext(ICommandContext*& cmdContext);

	class SCommandContext
	{
	public:

		PARABLIT_API SCommandContext(IRenderer* renderer);

		PARABLIT_API ~SCommandContext();

		PARABLIT_API inline ICommandContext* operator -> ();

	private:

		ICommandContext* m_ptr;
	};
}