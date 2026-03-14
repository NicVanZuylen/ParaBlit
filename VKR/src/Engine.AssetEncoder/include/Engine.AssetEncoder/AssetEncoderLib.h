#pragma once

#if defined(_MSC_VER)

#ifdef ASSET_ENCODER_EXPORT
#define ASSET_ENCODER_API __declspec(dllexport)
#else
#define ASSET_ENCODER_API __declspec(dllimport)
#endif

#elif defined(__GNUC__)

#ifdef ASSET_ENCODER_EXPORT
#define ASSET_ENCODER_API __attribute__((visibility("default")))
#else
#define ASSET_ENCODER_API
#endif

#endif