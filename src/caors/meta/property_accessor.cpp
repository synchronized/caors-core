// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/property_accessor.h"

#include "caors/meta/detail/property_record.h"

namespace caors::meta {

bool property_accessor::valid() const noexcept {
  return state_ != nullptr && value_type_.valid();
}

any_ref property_accessor::get_ref(const void* object) const {
  const auto* data = static_cast<const detail::property_record*>(state_);
  return valid() ? data->get_ref_unchecked(value_type_, object) : any_ref{};
}

any_ref property_accessor::get_ref(void* object) const {
  const auto* data = static_cast<const detail::property_record*>(state_);
  return valid() ? data->get_ref_unchecked(value_type_, object) : any_ref{};
}

property_accessor compile_property_accessor(const property& source) noexcept {
  return source.property_ != nullptr
             ? property_accessor{source.property_, source.property_->get_type()}
             : property_accessor{};
}

} // namespace caors::meta
