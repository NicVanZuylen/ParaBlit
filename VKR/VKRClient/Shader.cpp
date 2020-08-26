#include "Shader.h"
#include "QuickIO.h"
#include "CLib/Allocator.h"
#include <cstring>

namespace PBClient
{
	Shader::Shader(PB::IRenderer* renderer, const char* path, CLib::Allocator* allocator)
	{
		PB::ShaderModuleDesc moduleDesc;

		char* data = nullptr;
		if(allocator)
			QIO::LoadAlloc(path, &data, &moduleDesc.m_size, allocator);
		else
			QIO::Load(path, &data, &moduleDesc.m_size);

		moduleDesc.m_byteCode = data;
		moduleDesc.m_key = path;
		moduleDesc.m_keySize = strlen(path);
		
		m_module = renderer->GetShaderModuleCache()->GetModule(moduleDesc);
		if (allocator)
			allocator->Free((void*)data);
		else
			delete[] data;
	}

	Shader::~Shader()
	{

	}

	PB::ShaderModule Shader::GetModule()
	{
		return m_module;
	}

	Shader::operator PB::ShaderModule()
	{
		return m_module;
	}
}