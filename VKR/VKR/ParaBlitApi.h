#pragma once

namespace PB 
{
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef unsigned long long u64;
}


#ifdef PARABLIT_EXPORT
#define PARABLIT_API __declspec(dllexport)
#else
#define PARABLIT_API __declspec(dllimport)
#endif