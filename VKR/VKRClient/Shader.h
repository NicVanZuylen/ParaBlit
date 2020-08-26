#pragma once
#include "IRenderer.h"

namespace CLib
{
	class Allocator;
}

namespace PBClient
{
	class Shader
	{
	public:

		Shader(PB::IRenderer* renderer, const char* path, CLib::Allocator* allocator = nullptr);

		~Shader();

		PB::ShaderModule GetModule();

		operator PB::ShaderModule();

	private:

		PB::ShaderModule m_module = 0;
	};
}

