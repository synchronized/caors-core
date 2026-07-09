// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_FWD_H_
#define CAORS_VALUE_FWD_H_

#include "caors/core/string_name.h"
#include "caors/meta/type.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <iterator>
#include <optional>

namespace caors::meta {

struct type_descriptor;

namespace detail {

struct indirect_ops;
struct sequence_ops;
struct set_ops;
struct associative_ops;
struct pair_ops;
struct tuple_ops;
struct variant_ops;

} // namespace detail

} // namespace caors::meta

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
