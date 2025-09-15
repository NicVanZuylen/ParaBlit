#include "PipelineApplication.h"

#include <cassert>
#include <iostream>
#include <filesystem>

#include "Engine.Control/ISettingsParsers.h"
#include "Engine.AssetEncoder/EncoderBase.h"

#include "ReflectionGenerator.h"

namespace Reflectron
{
	PipelineApplication::PipelineApplication(int argc, char** argv)
	{
		Ctrl::ICommandLine* parser = Ctrl::ICommandLine::Create(argc, argv);
		const Ctrl::IDataContainer* commandLineData = parser->GetData();
		Ctrl::ISettingsHub* settings = Ctrl::ISettingsHub::GetOrCreate();
		settings->AddSettings(commandLineData);

		Ctrl::ICommandLine::Destroy(parser);
	}

	PipelineApplication::~PipelineApplication()
	{
		Ctrl::ISettingsHub::Destroy();

		REFLECTRON_LOG("Shutting down...");
	}

	void PipelineApplication::Run(int argc, char** argv)
	{
		Ctrl::ISettingsHub* settings = Ctrl::ISettingsHub::GetOrCreate();

		REFLECTRON_LOG("Starting Reflectron...\n");

		std::string rootDir = settings->GetStringValue("Path.SourceDir");
		std::string workingDir = std::filesystem::current_path().string();
		std::string dbDir = workingDir + "\\" + rootDir;

		REFLECTRON_LOG("Working Directory: %s", workingDir.c_str());
		REFLECTRON_LOG("Source Directory: %s\n", dbDir.c_str());

		ReflectionGenerator generator("Reflectron Worker", nullptr, rootDir.c_str());
	}
}
