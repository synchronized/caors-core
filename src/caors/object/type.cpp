// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/type.h"

#include "caors/object/detail/type_data.h"

namespace caors::objects {

type::type(meta::type type, const detail::type_data* data) noexcept : type_(type), data_(data) {}

bool type::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type type::info() const noexcept { return valid() ? type_ : meta::type{}; }

object_type_id type::id() const noexcept { return type_.id(); }

core::string_name type::name() const noexcept { return type_.name(); }

core::type_guid type::guid() const noexcept { return type_.guid(); }

std::size_t type::size() const noexcept { return type_.size(); }

std::size_t type::alignment() const noexcept { return type_.alignment(); }

object_type_id type::parent_id() const noexcept { return type_.parent_id(); }

bool type::constructible() const noexcept { return data_ != nullptr && data_->constructible(); }

const detail::type_data* type::data() const noexcept { return valid() ? data_ : nullptr; }

} // namespace caors::objects
