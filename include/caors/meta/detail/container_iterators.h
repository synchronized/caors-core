// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_ITERATORS_H_
#define CAORS_META_DETAIL_CONTAINER_ITERATORS_H_

#include <new>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {
template <typename T, typename = void> struct has_const_container_iterator : std::false_type {};

template <typename T>
struct has_const_container_iterator<T, std::void_t<decltype(std::declval<const T&>().begin()),
                                                   decltype(std::declval<const T&>().end())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_const_container_iterator_v = has_const_container_iterator<T>::value;

template <typename Iterator>
[[nodiscard]] void* clone_container_iterator(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return new (std::nothrow) Iterator(*static_cast<const Iterator*>(iterator));
}

template <typename Iterator> void destroy_container_iterator(void* iterator) noexcept {
  delete static_cast<Iterator*>(iterator);
}

template <typename Iterator> void advance_container_iterator(void* iterator) noexcept {
  if (iterator != nullptr) {
    ++(*static_cast<Iterator*>(iterator));
  }
}

template <typename Iterator>
[[nodiscard]] bool equal_container_iterator(const void* lhs, const void* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return lhs == rhs;
  }

  return *static_cast<const Iterator*>(lhs) == *static_cast<const Iterator*>(rhs);
}

template <typename Container>
[[nodiscard]] void* begin_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().begin());
  return new (std::nothrow) iterator_type(static_cast<const Container*>(storage)->begin());
}

template <typename Container>
[[nodiscard]] void* end_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().end());
  return new (std::nothrow) iterator_type(static_cast<const Container*>(storage)->end());
}

template <typename Iterator>
[[nodiscard]] const void* set_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &**static_cast<const Iterator*>(iterator);
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_key(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &(*static_cast<const Iterator*>(iterator))->first;
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &(*static_cast<const Iterator*>(iterator))->second;
}

} // namespace caors::meta::detail

#endif
