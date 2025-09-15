#pragma once
#include "Reflectron.h"

namespace Reflectron
{
	class PipelineApplication
	{
	public:

		PipelineApplication(int argc, char** argv);

		~PipelineApplication();

		void Run(int argc, char** argv);

	private:

	};
};