// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

namespace caors {

bool meta_type::valid() const noexcept { return data_ != nullptr && data_->registered(); }

type_id meta_type::id() const noexcept {
  return data_ != nullptr ? data_->id() : type_id::invalid();
}

string_name meta_type::name() const noexcept {
  return data_ != nullptr ? data_->name() : string_name{};
}

type_guid meta_type::guid() const noexcept {
  return data_ != nullptr ? data_->guid() : type_guid::nil();
}

std::size_t meta_type::size() const noexcept { return data_ != nullptr ? data_->size() : 0; }

std::size_t meta_type::alignment() const noexcept {
  return data_ != nullptr ? data_->alignment() : 0;
}

type_id meta_type::parent_id() const noexcept {
  return data_ != nullptr ? data_->parent_id() : type_id::invalid();
}

meta_type meta_type::parent() const noexcept {
  if (registry_ == nullptr || !parent_id()) {
    return {};
  }

  const std::optional<meta_type> parent_type = registry_->find_type(parent_id());
  return parent_type.value_or(meta_type{});
}

} // namespace caors
