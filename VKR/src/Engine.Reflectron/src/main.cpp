#include "PipelineApplication.h"

#if REFLECTRON_WINDOWS
#include <crtdbg.h>
#endif

int main(int argc, char** argv)
{
#if REFLECTRON_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Reflectron::PipelineApplication* application = new Reflectron::PipelineApplication(argc, argv);
	application->Run(argc, argv);

	delete application;

	return 0;
}