#pragma once
#include "ParaBlitApi.h"
#include "DynamicArray.h"
#include "ICommandContext.h"
#include "CommandContext.h"

#include <mutex>

namespace PB
{
	class CmdContextPool
	{
	public:

		CmdContextPool();

		~CmdContextPool();

		PARABLIT_API ICommandContext* Allocate();

		PARABLIT_API void Free(ICommandContext* contextAddress);

	private:

		DynamicArray<CommandContext*> m_contextPages;
		DynamicArray<ICommandContext*, 16> m_gaps;
		uint32_t m_contextCount = 0;
		std::mutex m_allocatorLock;
	};
}

