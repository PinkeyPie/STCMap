#pragma once

#if defined(SHARED_LIBRARY)
#ifdef _MSC_VER
#define SHARED_EXPORT __declspec(dllexport)
#else
#define SHARED_EXPORT
#endif
#else
#ifdef _MSC_VER
#define SHARED_EXPORT __declspec(dllimport)
#else
#define SHARED_EXPORT
#endif
#endif