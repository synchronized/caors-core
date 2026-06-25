// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CONFIG_H_
#define CAORS_CONFIG_H_

#include <cstddef>

namespace caors::config {

#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
inline constexpr std::size_t default_new_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
inline constexpr std::size_t default_new_alignment = alignof(std::max_align_t);
#endif

}

#endif
