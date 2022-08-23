#include "PipelineApplication.h"

#include <crtdbg.h>

int main(int argc, char** argv)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	AssetPipeline::PipelineApplication* application = new AssetPipeline::PipelineApplication(argc, argv);
	application->Run(argc, argv);

	delete application;

	return 0;
}