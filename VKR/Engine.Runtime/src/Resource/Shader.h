#pragma once
#include "Engine.ParaBlit/IRenderer.h"
#include "Engine.AssetEncoder/AssetDatabaseReader.h"

namespace CLib
{
	class Allocator;
}

namespace Eng
{
	class Shader
	{
	public:

		Shader(PB::IRenderer* renderer, const char* path, CLib::Allocator* allocator = nullptr, bool loadFromDatabase = false);

		~Shader();

		PB::ShaderModule GetModule();

		operator PB::ShaderModule();

	private:

		static AssetEncoder::AssetBinaryDatabaseReader s_shaderDatabaseLoader;
		PB::ShaderModule m_module = 0;
	};
}

