#include <iostream>
#include <cassert>
#include <filesystem>

#include <crtdbg.h>

#include "SpirvShaderEncoder.h"
#include "MeshEncoder.h"

int main(int argc, char** argv)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	printf("Running Asset Pipeline...\n\n");

	if (argc <= 1)
	{
		printf("Arguments required:\n 1. %s\n\n",
			"Asset Root Directory");
	}
	assert(argc > 1);

	std::string rootDir = argv[1];
	assert(rootDir[0] == '-');
	rootDir.erase(rootDir.begin());

	std::string workingDir = std::filesystem::current_path().parent_path().string();
	std::string dbDir = workingDir + "\\" + rootDir;

	printf("Working Directory: %s\n", workingDir.c_str());
	printf("Assets Directory: %s\n\n", dbDir.c_str());

	printf("Commence Encoding...\n\n");

	{
		AssetPipeline::SpirvShaderEncoder encoder("[SPIR-V Encoder]", "shaders.adb", dbDir.c_str());
	}

	{
		AssetPipeline::MeshEncoder encoder("[Mesh Encoder]", "meshes.adb", dbDir.c_str());
	}

	printf("\nBuild Complete.\n");

	return 0;
}