#pragma once
#include "ParaBlitApi.h"
#include "CLib/Vector.h"
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

		CLib::Vector<CommandContext*> m_contextPages;
		CLib::Vector<ICommandContext*, 16> m_gaps;
		uint32_t m_contextCount = 0;
		std::mutex m_allocatorLock;
	};
}

