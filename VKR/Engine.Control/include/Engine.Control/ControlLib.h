#pragma once

#ifdef CONTROL_EXPORT
#define CONTROL_API __declspec(dllexport)
#else
#define CONTROL_API __declspec(dllimport)
#endif