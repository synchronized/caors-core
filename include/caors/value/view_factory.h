// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_VIEW_FACTORY_H_
#define CAORS_VALUE_VIEW_FACTORY_H_

#include "caors/value/associative_view.h"
#include "caors/value/indirect_view.h"
#include "caors/value/pair_view.h"
#include "caors/value/sequence_view.h"
#include "caors/value/set_view.h"
#include "caors/value/tuple_view.h"
#include "caors/value/variant_view.h"

namespace caors::value {

inline associative_view any::create_associative_view() {
  if (!valid()) {
    return {};
  }

  const meta::detail::associative_data* data =
      meta::get_registry().find_ptr<meta::detail::associative_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, mutable_data(), true};
}

inline associative_view any::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::associative_data* data =
      meta::get_registry().find_ptr<meta::detail::associative_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, data_, false};
}

inline associative_view any_ref::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::associative_data* data =
      meta::get_registry().find_ptr<meta::detail::associative_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, data_, writable_};
}

inline pair_view any::create_pair_view() {
  if (!valid()) {
    return {};
  }

  const meta::detail::pair_data* data =
      meta::get_registry().find_ptr<meta::detail::pair_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return pair_view{data, mutable_data(), true};
}

inline pair_view any::create_pair_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::pair_data* data =
      meta::get_registry().find_ptr<meta::detail::pair_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return pair_view{data, data_, false};
}

inline pair_view any_ref::create_pair_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::pair_data* data =
      meta::get_registry().find_ptr<meta::detail::pair_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return pair_view{data, data_, writable_};
}

inline tuple_view any::create_tuple_view() {
  if (!valid()) {
    return {};
  }

  const meta::detail::tuple_data* data =
      meta::get_registry().find_ptr<meta::detail::tuple_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return tuple_view{data, mutable_data(), true};
}

inline tuple_view any::create_tuple_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::tuple_data* data =
      meta::get_registry().find_ptr<meta::detail::tuple_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return tuple_view{data, data_, false};
}

inline tuple_view any_ref::create_tuple_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::tuple_data* data =
      meta::get_registry().find_ptr<meta::detail::tuple_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return tuple_view{data, data_, writable_};
}

inline variant_view any::create_variant_view() {
  if (!valid()) {
    return {};
  }

  const meta::detail::variant_data* data =
      meta::get_registry().find_ptr<meta::detail::variant_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return variant_view{data, mutable_data(), true};
}

inline variant_view any::create_variant_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::variant_data* data =
      meta::get_registry().find_ptr<meta::detail::variant_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return variant_view{data, data_, false};
}

inline variant_view any_ref::create_variant_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::variant_data* data =
      meta::get_registry().find_ptr<meta::detail::variant_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return variant_view{data, data_, writable_};
}

inline indirect_view any::create_indirect_view() {
  if (!valid()) {
    return {};
  }

  const meta::type_semantics_data* data =
      meta::get_registry().find_ptr<meta::type_semantics_data>(type_);
  if (data == nullptr || (data->kind != meta::type_kind::raw_pointer &&
                          (data->kind != meta::type_kind::wrapper ||
                           data->wrapper_semantics == meta::wrapper_semantics::unsupported))) {
    return {};
  }

  return indirect_view{data, mutable_data(), true};
}

inline indirect_view any::create_indirect_view() const {
  if (!valid()) {
    return {};
  }

  const meta::type_semantics_data* data =
      meta::get_registry().find_ptr<meta::type_semantics_data>(type_);
  if (data == nullptr || (data->kind != meta::type_kind::raw_pointer &&
                          (data->kind != meta::type_kind::wrapper ||
                           data->wrapper_semantics == meta::wrapper_semantics::unsupported))) {
    return {};
  }

  return indirect_view{data, data_, false};
}

inline indirect_view any_ref::create_indirect_view() const {
  if (!valid()) {
    return {};
  }

  const meta::type_semantics_data* data =
      meta::get_registry().find_ptr<meta::type_semantics_data>(type_);
  if (data == nullptr || (data->kind != meta::type_kind::raw_pointer &&
                          (data->kind != meta::type_kind::wrapper ||
                           data->wrapper_semantics == meta::wrapper_semantics::unsupported))) {
    return {};
  }

  return indirect_view{data, data_, writable_};
}

inline sequence_view any::create_sequence_view() {
  if (!valid()) {
    return {};
  }

  const meta::detail::sequence_data* data =
      meta::get_registry().find_ptr<meta::detail::sequence_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, mutable_data(), true};
}

inline sequence_view any::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::sequence_data* data =
      meta::get_registry().find_ptr<meta::detail::sequence_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, data_, false};
}

inline sequence_view any_ref::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::sequence_data* data =
      meta::get_registry().find_ptr<meta::detail::sequence_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, data_, writable_};
}

inline set_view any::create_set_view() {
  if (!valid()) {
    return {};
  }

  const meta::detail::set_data* data = meta::get_registry().find_ptr<meta::detail::set_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, mutable_data(), true};
}

inline set_view any::create_set_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::set_data* data = meta::get_registry().find_ptr<meta::detail::set_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, data_, false};
}

inline set_view any_ref::create_set_view() const {
  if (!valid()) {
    return {};
  }

  const meta::detail::set_data* data = meta::get_registry().find_ptr<meta::detail::set_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, data_, writable_};
}

} // namespace caors::value

#endif
