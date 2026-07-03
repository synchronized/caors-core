// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

#include <mutex>

namespace caors::meta {
namespace {

[[nodiscard]] error register_type_impl(registry_state& state, type_data type,
                                       caors::detail::cpp_type_key explicit_key = nullptr,
                                       type_id* stored_id = nullptr) {
  if (explicit_key != nullptr) {
    if (type.cpp_type() != nullptr && type.cpp_type() != explicit_key) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (type.cpp_type() == nullptr) {
      detail::set_cpp_type(type, explicit_key);
    }
  }

  if (!type.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state.mutex);
  if (state.frozen) {
    return {error_domain::meta, errc::frozen};
  }

  if (type.cpp_type() != nullptr) {
    const auto key_position = state.cpp_types.find(type.cpp_type());
    if (key_position != state.cpp_types.end()) {
      type_data* existing = detail::find_type_data(state, key_position->second);
      if (existing == nullptr) {
        return {error_domain::meta, errc::invalid_type};
      }

      const error status = detail::merge_type_data(state, *existing, type);
      if (status.value() == errc::invalid_type) {
        return {error_domain::meta, errc::invalid_type};
      }

      if (stored_id != nullptr) {
        *stored_id = existing->id();
      }

      return status;
    }
  }

  error conflict{};
  const std::optional<type_id> existing_id = detail::find_existing_type_id(state, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    type_data* existing = detail::find_type_data(state, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (type.cpp_type() == nullptr && detail::has_cpp_type(state, *existing_id)) {
      return detail::duplicate_identity_error(state, type, *existing_id);
    }

    if (type.cpp_type() != nullptr && detail::has_cpp_type(state, *existing_id)) {
      return detail::duplicate_identity_error(state, type, *existing_id);
    }

    const error status = detail::merge_type_data(state, *existing, type);
    if (status.value() == errc::invalid_type) {
      return detail::duplicate_identity_error(state, type, *existing_id);
    }

    if (stored_id != nullptr) {
      *stored_id = existing->id();
    }

    return type.cpp_type() != nullptr && status.value() == errc::duplicate_id ? error{} : status;
  }

  return detail::store_new_type(state, type, stored_id);
}

} // namespace

error registry::register_type(const type_data& type) { return register_type_impl(*state_, type); }

error registry::register_cpp_type(caors::detail::cpp_type_key key, const type_data& type) {
  if (key == nullptr) {
    return register_type(type);
  }

  return register_type_impl(*state_, type, key);
}

} // namespace caors::meta
