// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_FWD_H_
#define CAORS_VALUE_FWD_H_

#include "caors/core/string_name.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <iterator>
#include <optional>

namespace caors::meta::detail {

struct type_descriptor;
struct indirect_data;
struct sequence_data;
struct set_data;
struct associative_data;
struct pair_data;
struct tuple_data;
struct variant_data;

} // namespace caors::meta::detail

namespace caors::memory {

class memory_resource;

} // namespace caors::memory

namespace caors::value {

class any;
class any_ref;
class indirect_view;
class sequence_view;
class set_view;
class associative_view;
class associative_entry_ref;
class pair_view;
class tuple_view;
class variant_view;

/** @brief 集合元素访问回调。 */
using set_element_visitor = bool (*)(any_ref element, void* user_data);

/** @brief 关联容器 key/value 访问回调。 */
using associative_entry_visitor = bool (*)(any_ref key, any_ref mapped, void* user_data);

} // namespace caors::value

#endif
