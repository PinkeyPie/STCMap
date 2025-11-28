#pragma once

#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>

#include <limits>
#include <array>
#include <string>
#include <vector>
#include <iostream>
#include <memory>

#include <cassert>

#include <filesystem>
namespace fs = std::filesystem;

#include <mutex>

#include <wrl.h>

using int8 = int8_t;
using uint8 = uint8_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using wchar = wchar_t;

template<class T> using Ptr = std::shared_ptr<T>;
template<class T> using WeakPtr = std::weak_ptr<T>;

template<class T, class... Args>
Ptr<T> MakePtr(Args&&... args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> struct is_ref : std::false_type {};
template <typename T> struct is_ref<Ptr<T>> : std::true_type {};

template <typename T> inline constexpr bool is_ref_v = is_ref<T>::value;

template <typename T>
using Com = Microsoft::WRL::ComPtr<T>;

#define arraysize(arr) (sizeof(arr) / sizeof((arr)[0]))

template <typename T>
constexpr inline auto Min(T a, T b) {
	return (a < b) ? a : b;
}

template <typename T>
constexpr inline auto Max(T a, T b) {
	return (a < b) ? b : a;
}

template <auto V> static constexpr auto force_consteval = V;

#define SetBit(mask, bit) (mask) |= (1 << (bit))
#define UnsetBit(mask, bit) (mask) ^= (1 << (bit))
