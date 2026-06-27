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

namespace caors::config {

inline constexpr bool has_exceptions = CAORS_HAS_EXCEPTIONS != 0;

#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
inline constexpr std::size_t default_new_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
inline constexpr std::size_t default_new_alignment = alignof(std::max_align_t);
#endif

}

#endif
