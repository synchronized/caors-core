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
namespace detail {

template <typename Capability>
[[nodiscard]] inline bool has_capability(const meta::detail::type_descriptor* descriptor) noexcept {
  return meta::detail::descriptor_capability<Capability>(descriptor) != nullptr;
}

[[nodiscard]] inline bool
has_indirect_capability(const meta::detail::type_descriptor* descriptor) noexcept {
  const meta::detail::indirect_data* indirect =
      meta::detail::descriptor_capability<meta::detail::indirect_data>(descriptor);
  return descriptor != nullptr && meta::detail::descriptor_indirect(*descriptor) &&
         indirect != nullptr && indirect->valid();
}

} // namespace detail

inline associative_view any::create_associative_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::associative_data>(descriptor_)) {
    return {};
  }

  return associative_view{descriptor_, mutable_data(), true};
}

inline associative_view any::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::associative_data>(descriptor_)) {
    return {};
  }

  return associative_view{descriptor_, data_, false};
}

inline associative_view any_ref::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::associative_data>(descriptor_)) {
    return {};
  }

  return associative_view{descriptor_, data_, writable_};
}

inline pair_view any::create_pair_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::pair_data>(descriptor_)) {
    return {};
  }

  return pair_view{descriptor_, mutable_data(), true};
}

inline pair_view any::create_pair_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::pair_data>(descriptor_)) {
    return {};
  }

  return pair_view{descriptor_, data_, false};
}

inline pair_view any_ref::create_pair_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::pair_data>(descriptor_)) {
    return {};
  }

  return pair_view{descriptor_, data_, writable_};
}

inline tuple_view any::create_tuple_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::tuple_data>(descriptor_)) {
    return {};
  }

  return tuple_view{descriptor_, mutable_data(), true};
}

inline tuple_view any::create_tuple_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::tuple_data>(descriptor_)) {
    return {};
  }

  return tuple_view{descriptor_, data_, false};
}

inline tuple_view any_ref::create_tuple_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::tuple_data>(descriptor_)) {
    return {};
  }

  return tuple_view{descriptor_, data_, writable_};
}

inline variant_view any::create_variant_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::variant_data>(descriptor_)) {
    return {};
  }

  return variant_view{descriptor_, mutable_data(), true};
}

inline variant_view any::create_variant_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::variant_data>(descriptor_)) {
    return {};
  }

  return variant_view{descriptor_, data_, false};
}

inline variant_view any_ref::create_variant_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::variant_data>(descriptor_)) {
    return {};
  }

  return variant_view{descriptor_, data_, writable_};
}

inline indirect_view any::create_indirect_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_indirect_capability(descriptor_)) {
    return {};
  }

  return indirect_view{descriptor_, mutable_data(), true};
}

inline indirect_view any::create_indirect_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_indirect_capability(descriptor_)) {
    return {};
  }

  return indirect_view{descriptor_, data_, false};
}

inline indirect_view any_ref::create_indirect_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_indirect_capability(descriptor_)) {
    return {};
  }

  return indirect_view{descriptor_, data_, writable_};
}

inline sequence_view any::create_sequence_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::sequence_data>(descriptor_)) {
    return {};
  }

  return sequence_view{descriptor_, mutable_data(), true};
}

inline sequence_view any::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::sequence_data>(descriptor_)) {
    return {};
  }

  return sequence_view{descriptor_, data_, false};
}

inline sequence_view any_ref::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::sequence_data>(descriptor_)) {
    return {};
  }

  return sequence_view{descriptor_, data_, writable_};
}

inline set_view any::create_set_view() {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::set_data>(descriptor_)) {
    return {};
  }

  return set_view{descriptor_, mutable_data(), true};
}

inline set_view any::create_set_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::set_data>(descriptor_)) {
    return {};
  }

  return set_view{descriptor_, data_, false};
}

inline set_view any_ref::create_set_view() const {
  if (!valid()) {
    return {};
  }

  if (!detail::has_capability<meta::detail::set_data>(descriptor_)) {
    return {};
  }

  return set_view{descriptor_, data_, writable_};
}

} // namespace caors::value

#endif
