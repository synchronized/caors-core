// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

namespace caors::meta {

bool type::valid() const noexcept { return data_ != nullptr && data_->registered(); }

type_id type::id() const noexcept { return data_ != nullptr ? data_->id() : type_id::invalid(); }

core::string_name type::name() const noexcept {
  return data_ != nullptr ? data_->name() : core::string_name{};
}

core::type_guid type::guid() const noexcept {
  return data_ != nullptr ? data_->guid() : core::type_guid::nil();
}

std::size_t type::size() const noexcept { return data_ != nullptr ? data_->size() : 0; }

std::size_t type::alignment() const noexcept { return data_ != nullptr ? data_->alignment() : 0; }

type_id type::parent_id() const noexcept {
  return data_ != nullptr ? data_->parent_id() : type_id::invalid();
}

type type::parent() const noexcept {
  if (registry_ == nullptr || !parent_id()) {
    return {};
  }

  return registry_->find_type(parent_id());
}

namespace detail {

value_default_construct_fn get_default_construct_value_fn(const meta::type& type) noexcept {
  return type.data_ != nullptr ? type.data_->default_construct_value() : nullptr;
}

value_clone_fn get_clone_value_fn(const meta::type& type) noexcept {
  return type.data_ != nullptr ? type.data_->clone_value() : nullptr;
}

value_destroy_fn get_destroy_value_fn(const meta::type& type) noexcept {
  return type.data_ != nullptr ? type.data_->destroy_value() : nullptr;
}

value_assign_fn get_assign_value_fn(const meta::type& type) noexcept {
  return type.data_ != nullptr ? type.data_->assign_value() : nullptr;
}

} // namespace detail

} // namespace caors::meta
