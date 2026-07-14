// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/type.h"

#include "caors/object/detail/factory_ops.h"

namespace caors::objects {

type::type(meta::type type, const object_factory_ops* factory) noexcept
    : type_(type), factory_(factory) {}

bool type::valid() const noexcept { return type_.valid() && factory_ != nullptr; }

meta::type type::info() const noexcept { return valid() ? type_ : meta::type{}; }

object_type_id type::id() const noexcept { return type_.id(); }

core::string_name type::name() const noexcept { return type_.name(); }

core::type_guid type::guid() const noexcept { return type_.guid(); }

std::size_t type::size() const noexcept {
  const meta::value_storage_ops* value_ops = type_.get_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.size : 0;
}

std::size_t type::alignment() const noexcept {
  const meta::value_storage_ops* value_ops = type_.get_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.alignment : 0;
}

object_type_id type::parent_id() const noexcept { return type_.parent_id(); }

bool type::constructible() const noexcept {
  return factory_ != nullptr && factory_->constructible();
}

const object_factory_ops* type::factory() const noexcept { return valid() ? factory_ : nullptr; }

} // namespace caors::objects
