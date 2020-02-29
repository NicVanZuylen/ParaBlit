#pragma once

#ifdef VKR_EXPORT
#define VKR_API __declspec(dllexport)
#else
#define VKR_API __declspec(dllimport)
#endif