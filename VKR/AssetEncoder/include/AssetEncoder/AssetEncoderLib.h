#pragma once

#ifdef ASSET_ENCODER_EXPORT
#define ASSET_ENCODER_API __declspec(dllexport)
#else
#define ASSET_ENCODER_API __declspec(dllimport)
#endif