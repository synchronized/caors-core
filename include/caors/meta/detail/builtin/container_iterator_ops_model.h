// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_CONTAINER_ITERATOR_OPS_MODEL_H_
#define CAORS_META_DETAIL_BUILTIN_CONTAINER_ITERATOR_OPS_MODEL_H_

#include "caors/config.h"
#include "caors/memory/resource.h"
#include "caors/meta/type_ops.h"
#include "caors/traits/container.h"

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

template <typename Traits, typename Container, typename = void>
struct has_traits_const_iterator : std::false_type {};

template <typename Traits, typename Container>
struct has_traits_const_iterator<
    Traits, Container,
    std::void_t<typename Traits::const_iterator,
                decltype(Traits::begin(std::declval<const Container&>())),
                decltype(Traits::end(std::declval<const Container&>()))>> : std::true_type {};

template <typename Traits, typename Container>
inline constexpr bool has_traits_const_iterator_v =
    has_traits_const_iterator<Traits, Container>::value;

using meta::container_iterator_ops;

template <typename Iterator> struct container_iterator_holder {
  template <typename... Args>
  explicit container_iterator_holder(memory::memory_resource* iterator_resource, Args&&... args)
      : resource(iterator_resource), iterator(std::forward<Args>(args)...) {}

  memory::memory_resource* resource = nullptr;
  Iterator iterator;
};

template <typename Iterator, typename... Args>
[[nodiscard]] void* make_container_iterator(Args&&... args) noexcept {
  using holder_type = container_iterator_holder<Iterator>;
  memory::memory_resource& resource = memory::process_resource();
  void* storage = resource.try_allocate(sizeof(holder_type), alignof(holder_type));
  if (storage == nullptr) {
    return nullptr;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
    return ::new (storage) holder_type(&resource, std::forward<Args>(args)...);
  } catch (...) {
    resource.deallocate(storage, sizeof(holder_type), alignof(holder_type));
    return nullptr;
  }
#else
  if constexpr (std::is_nothrow_constructible_v<holder_type, memory::memory_resource*, Args...>) {
    return ::new (storage) holder_type(&resource, std::forward<Args>(args)...);
  } else {
    resource.deallocate(storage, sizeof(holder_type), alignof(holder_type));
    return nullptr;
  }
#endif
}

template <typename Iterator>
[[nodiscard]] void* clone_container_iterator(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  return make_container_iterator<Iterator>(holder->iterator);
}

template <typename Iterator> void destroy_container_iterator(void* iterator) noexcept {
  if (iterator == nullptr) {
    return;
  }

  using holder_type = container_iterator_holder<Iterator>;
  auto* holder = static_cast<holder_type*>(iterator);
  memory::memory_resource* resource = holder->resource;
  holder->~holder_type();
  if (resource != nullptr) {
    resource->deallocate(holder, sizeof(holder_type), alignof(holder_type));
  }
}

template <typename Iterator> void advance_container_iterator(void* iterator) noexcept {
  if (iterator != nullptr) {
    ++(static_cast<container_iterator_holder<Iterator>*>(iterator)->iterator);
  }
}

template <typename Iterator>
[[nodiscard]] bool equal_container_iterator(const void* lhs, const void* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return lhs == rhs;
  }

  const auto* left = static_cast<const container_iterator_holder<Iterator>*>(lhs);
  const auto* right = static_cast<const container_iterator_holder<Iterator>*>(rhs);
  return left->iterator == right->iterator;
}

template <typename Container>
[[nodiscard]] void* begin_container_iterator(const void* storage) noexcept;

template <typename Container>
[[nodiscard]] void* end_container_iterator(const void* storage) noexcept;

template <typename Container> struct container_iterator_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Container>>;
  using iterator_type = decltype(std::declval<const type&>().begin());

  inline static const container_iterator_ops value{
      &begin_container_iterator<type>,
      &end_container_iterator<type>,
      &clone_container_iterator<iterator_type>,
      &destroy_container_iterator<iterator_type>,
      &advance_container_iterator<iterator_type>,
      &equal_container_iterator<iterator_type>,
  };
};

template <typename Container, typename Traits>
[[nodiscard]] void* begin_traits_container_iterator(const void* storage) noexcept;

template <typename Container, typename Traits>
[[nodiscard]] void* end_traits_container_iterator(const void* storage) noexcept;

template <typename Container, typename Traits> struct traits_container_iterator_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Container>>;
  using iterator_type = typename Traits::const_iterator;

  inline static const container_iterator_ops value{
      &begin_traits_container_iterator<type, Traits>, &end_traits_container_iterator<type, Traits>,
      &clone_container_iterator<iterator_type>,       &destroy_container_iterator<iterator_type>,
      &advance_container_iterator<iterator_type>,     &equal_container_iterator<iterator_type>,
  };
};

template <typename Container>
[[nodiscard]] const container_iterator_ops* container_iterator_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<Container>>;
  if constexpr (has_const_container_iterator_v<type>) {
    return container_iterator_ops_model<type>::value.valid()
               ? &container_iterator_ops_model<type>::value
               : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Container, typename Traits>
[[nodiscard]] const container_iterator_ops* traits_container_iterator_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<Container>>;
  if constexpr (has_traits_const_iterator_v<Traits, type>) {
    return traits_container_iterator_ops_model<type, Traits>::value.valid()
               ? &traits_container_iterator_ops_model<type, Traits>::value
               : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Container>
[[nodiscard]] void* begin_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().begin());
  return make_container_iterator<iterator_type>(static_cast<const Container*>(storage)->begin());
}

template <typename Container>
[[nodiscard]] void* end_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().end());
  return make_container_iterator<iterator_type>(static_cast<const Container*>(storage)->end());
}

template <typename Container, typename Traits>
[[nodiscard]] void* begin_traits_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = typename Traits::const_iterator;
  return make_container_iterator<iterator_type>(
      Traits::begin(*static_cast<const Container*>(storage)));
}

template <typename Container, typename Traits>
[[nodiscard]] void* end_traits_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = typename Traits::const_iterator;
  return make_container_iterator<iterator_type>(
      Traits::end(*static_cast<const Container*>(storage)));
}

template <typename Iterator>
[[nodiscard]] const void* set_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  return &*holder->iterator;
}

template <typename Container, typename Iterator>
[[nodiscard]] const void* set_traits_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  using traits_type = traits::set_traits<Container>;
  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  if constexpr (requires(const Iterator& current) { traits_type::value(current); }) {
    return traits_type::value(holder->iterator);
  } else {
    return &*holder->iterator;
  }
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_key(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  return &holder->iterator->first;
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  return &holder->iterator->second;
}

template <typename Container, typename Iterator>
[[nodiscard]] const void* associative_traits_iterator_key(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  using traits_type = traits::associative_traits<Container>;
  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  if constexpr (requires(const Iterator& current) { traits_type::key(current); }) {
    return traits_type::key(holder->iterator);
  } else {
    return &holder->iterator->first;
  }
}

template <typename Container, typename Iterator>
[[nodiscard]] const void* associative_traits_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  using traits_type = traits::associative_traits<Container>;
  const auto* holder = static_cast<const container_iterator_holder<Iterator>*>(iterator);
  if constexpr (requires(const Iterator& current) { traits_type::mapped(current); }) {
    return traits_type::mapped(holder->iterator);
  } else {
    return &holder->iterator->second;
  }
}

} // namespace caors::meta::detail

#endif
