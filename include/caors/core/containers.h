// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_CONTAINERS_H_
#define CAORS_CORE_CONTAINERS_H_

#include "caors/memory.h"

#include <string>
#include <vector>

namespace caors {

/** @brief 使用 CAORS allocator 的动态数组别名。 */
template <typename T> using vector = std::vector<T, allocator<T>>;

/** @brief 使用 CAORS allocator 的字符串别名。 */
using string = std::basic_string<char, std::char_traits<char>, allocator<char>>;

} // namespace caors

#endif
