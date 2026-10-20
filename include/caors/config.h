// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CONFIG_H_
#define CAORS_CONFIG_H_

#include <cstddef>

#if defined(CAORS_FORCE_DISABLE_EXCEPTIONS)
#define CAORS_HAS_EXCEPTIONS 0
#elif defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define CAORS_HAS_EXCEPTIONS 1
#else
#define CAORS_HAS_EXCEPTIONS 0
#endif

#if !defined(CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE)
#define CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE 0
#endif

// Web 单线程基线不提供后台线程或原生动态模块。
#if defined(__EMSCRIPTEN__)
#define CAORS_HAS_BACKGROUND_THREADS 0
#define CAORS_HAS_DYNAMIC_LIBRARIES 0
#else
#define CAORS_HAS_BACKGROUND_THREADS 1
#define CAORS_HAS_DYNAMIC_LIBRARIES 1
#endif

namespace caors::config {

/** @brief 当前目标是否支持库提供的后台线程。 */
inline constexpr bool has_background_threads = CAORS_HAS_BACKGROUND_THREADS != 0;
/** @brief 当前目标是否支持原生动态库加载。 */
inline constexpr bool has_dynamic_libraries = CAORS_HAS_DYNAMIC_LIBRARIES != 0;

inline constexpr bool has_exceptions = CAORS_HAS_EXCEPTIONS != 0;
inline constexpr bool internal_allocations_use_default_resource =
    CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE != 0;

#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
inline constexpr std::size_t default_new_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
inline constexpr std::size_t default_new_alignment = alignof(std::max_align_t);
#endif

}

#endif
