// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

#include <mutex>

namespace caors {

error meta_registry::register_type(const meta_type_data& type) {
  if (!type.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);

  error conflict{};
  const std::optional<type_id> existing_id = detail::find_existing_type_id(*state_, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    meta_type_data* existing = detail::find_type_data(*state_, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (detail::has_cpp_type(*state_, *existing_id)) {
      return detail::duplicate_identity_error(*state_, type, *existing_id);
    }

    const error status = detail::merge_type_data(*state_, *existing, type);
    if (status.value() == errc::invalid_type) {
      return detail::duplicate_identity_error(*state_, type, *existing_id);
    }

    return status;
  }

  return detail::store_new_type(*state_, type);
}

error meta_registry::register_cpp_type(detail::cpp_type_key key, const meta_type_data& type) {
  if (key == nullptr) {
    return register_type(type);
  }

  if (!type.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);

  const auto key_position = state_->cpp_types.find(key);
  if (key_position != state_->cpp_types.end()) {
    meta_type_data* existing = detail::find_type_data(*state_, key_position->second);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    const error status = detail::merge_type_data(*state_, *existing, type);
    if (status.value() == errc::invalid_type) {
      return {error_domain::meta, errc::invalid_type};
    }

    return status;
  }

  error conflict{};
  const std::optional<type_id> existing_id = detail::find_existing_type_id(*state_, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    meta_type_data* existing = detail::find_type_data(*state_, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (detail::has_cpp_type(*state_, *existing_id)) {
      return detail::duplicate_identity_error(*state_, type, *existing_id);
    }

    const error status = detail::merge_type_data(*state_, *existing, type);
    if (status.value() == errc::invalid_type) {
      return detail::duplicate_identity_error(*state_, type, *existing_id);
    }

    state_->cpp_types.emplace(key, *existing_id);
    return status.value() == errc::duplicate_id ? error{} : status;
  }

  type_id stored_id{};
  const error status = detail::store_new_type(*state_, type, &stored_id);
  if (status.succeeded()) {
    state_->cpp_types.emplace(key, stored_id);
  }

  return status;
}

meta_type meta_registry::ensure_cpp_type(detail::cpp_type_key key, std::size_t size,
                                         std::size_t alignment) {
  if (key == nullptr || size == 0 || alignment == 0) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto key_position = state_->cpp_types.find(key);
  if (key_position != state_->cpp_types.end()) {
    const meta_type_data* existing = detail::find_type_data(*state_, key_position->second);
    return meta_type{this, existing};
  }

  meta_type_data data{
      detail::allocate_type_id(*state_),
      string_name{},
      type_guid::nil(),
      size,
      alignment,
      type_id::invalid(),
  };
  const type_id id = data.id();

  auto [position, inserted] =
      state_->types.emplace(id.value(), std::make_unique<meta_type_data>(data));
  if (!inserted) {
    return {};
  }

  state_->cpp_types.emplace(key, id);
  return meta_type{this, position->second.get()};
}

} // namespace caors
