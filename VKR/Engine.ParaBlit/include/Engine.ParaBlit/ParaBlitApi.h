#pragma once

#include "ParaBlitDefs.h"

#ifdef PARABLIT_EXPORT
#define PARABLIT_API __declspec(dllexport)
#else
#define PARABLIT_API __declspec(dllimport)
#endif