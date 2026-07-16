// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_FWD_H_
#define CAORS_META_VALUE_FWD_H_

#include "caors/core/string_name.h"
#include "caors/meta/type.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <iterator>
#include <optional>

namespace caors::meta {

struct indirect_ops;
struct sequence_ops;
struct set_ops;
struct associative_ops;
struct pair_ops;
struct tuple_ops;
struct variant_ops;

} // namespace caors::meta

namespace caors::memory {

class memory_resource;

} // namespace caors::memory

namespace caors::meta {

class any;
class any_ref;
class owned_value;
class indirect_view;
class sequence_view;
class set_view;
class associative_view;
class associative_entry_ref;
class pair_view;
class tuple_view;
class variant_view;

} // namespace caors::meta

#endif
