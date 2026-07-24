// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/property_accessor.h"

#include "caors/meta/detail/property_record.h"

namespace caors::meta {

bool property_accessor::valid() const noexcept { return state_ != nullptr && value_type_.valid(); }

any_ref property_accessor::get_ref(const void* object) const {
  if (valid() && member_ && object != nullptr) {
    const auto* bytes = static_cast<const unsigned char*>(object);
    return detail::any_ref_access::from_erased(value_type_, bytes + offset_, false);
  }
  const auto* data = static_cast<const detail::property_record*>(state_);
  return valid() ? data->get_ref_unchecked(value_type_, object) : any_ref{};
}

any_ref property_accessor::get_ref(void* object) const {
  if (valid() && member_ && object != nullptr) {
    auto* bytes = static_cast<unsigned char*>(object);
    return detail::any_ref_access::from_erased(value_type_, bytes + offset_, writable_);
  }
  const auto* data = static_cast<const detail::property_record*>(state_);
  return valid() ? data->get_ref_unchecked(value_type_, object) : any_ref{};
}

property_accessor compile_property_accessor(const property& source) noexcept {
  if (source.property_ == nullptr) {
    return {};
  }

  const detail::property_storage_info info = source.property_->storage_info();
  return property_accessor{source.property_, info.value_type, info.offset,
                           info.kind == detail::property_storage_kind::member, info.writable};
}

} // namespace caors::meta
