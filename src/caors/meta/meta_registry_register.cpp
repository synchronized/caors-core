// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

#include <mutex>

namespace caors::meta {
namespace {

[[nodiscard]] error register_type_impl(registry_state& state, detail::type_data type,
                                       type_id* stored_id = nullptr) {
  if (!type.valid() || !type.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state.mutex);
  if (state.frozen) {
    return {error_domain::meta, errc::frozen};
  }

  error conflict{};
  const std::optional<type_id> existing_id = detail::find_existing_type_id(state, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    detail::type_data* existing = detail::find_type_data(state, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    const error status = detail::merge_type_data(state, *existing, type);
    if (status.value() == errc::invalid_type) {
      return detail::duplicate_identity_error(state, type, *existing_id);
    }

    if (stored_id != nullptr) {
      *stored_id = existing->id();
    }

    return status;
  }

  return detail::store_new_type(state, type, stored_id);
}

} // namespace

error registry::register_type(const detail::type_data& type) {
  return register_type_impl(*state_, type);
}

} // namespace caors::meta
