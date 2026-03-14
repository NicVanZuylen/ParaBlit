#include "PipelineApplication.h"

#if ASSETPIPELINE_WINDOWS
#include <crtdbg.h>
#endif

int main(int argc, char** argv)
{
#if ASSETPIPELINE_WINDOWS
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	AssetPipeline::PipelineApplication* application = new AssetPipeline::PipelineApplication(argc, argv);
	application->Run(argc, argv);

	delete application;

	return 0;
}