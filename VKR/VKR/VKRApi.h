#pragma once

namespace VKR 
{
	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef unsigned long long u64;
}

#ifdef VKR_EXPORT
#define VKR_API __declspec(dllexport)
#else
#define VKR_API __declspec(dllimport)
#endif