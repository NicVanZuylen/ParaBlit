#pragma once

#if CONTROL_EXPORT
#define CONTROL_API __declspec(dllexport)
#else
#define CONTROL_API __declspec(dllimport)
#endif

#define CONTROL_INTERFACE virtual