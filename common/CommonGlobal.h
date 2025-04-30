#pragma once

#if defined(COMMON_LIBRARY)
#ifdef _MSC_VER
#define COMMON_EXPORT __declspec(dllexport)
#else
#define COMMON_EXPORT
#endif
#else
#ifdef _MSC_VER
#define COMMON_EXPORT __declspec(dllimport)
#else
#define COMMON_EXPORT
#endif
#endif