// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_SEQUENCE_TRAITS_H_
#define CAORS_ADAPTERS_STL_SEQUENCE_TRAITS_H_

#include "caors/core/containers.h"
#include "caors/traits/container.h"

#include <array>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <iterator>
#include <list>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace caors::traits {
namespace detail {

template <typename Container>
inline constexpr bool uses_caors_container_allocator_v =
    std::is_same_v<typename Container::allocator_type,
                   memory::allocator<typename Container::value_type>>;

template <typename T, typename = void> inline constexpr bool has_resource_allocator_v = false;

template <typename T>
inline constexpr bool has_resource_allocator_v<T, std::void_t<typename T::allocator_type>> =
    std::is_constructible_v<typename T::allocator_type, memory::memory_resource*>;

template <typename Element, typename Emplace>
[[nodiscard]] bool emplace_default_with_resource(memory::memory_resource* resource,
                                                 Emplace&& emplace) {
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    if constexpr (!std::is_default_constructible_v<Element>) {
      (void)resource;
      (void)emplace;
      return false;
    } else {
      if constexpr (has_resource_allocator_v<Element> && std::is_move_constructible_v<Element>) {
        if (resource != nullptr) {
          Element value{typename Element::allocator_type{resource}};
          emplace(std::move(value));
        } else {
          emplace();
        }
      } else {
        (void)resource;
        emplace();
      }

      return true;
    }
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    return false;
  }
#endif
}

template <typename Container>
[[nodiscard]] bool resize_back_sequence(Container& values, std::size_t size,
                                        memory::memory_resource* resource) {
  using element_type = typename Container::value_type;

  if (size <= values.size()) {
    while (values.size() > size) {
      values.pop_back();
    }
    return true;
  }

  if constexpr (!std::is_default_constructible_v<element_type>) {
    (void)resource;
    return false;
  } else {
    if constexpr (!has_resource_allocator_v<element_type>) {
      (void)resource;
      values.resize(size);
      return true;
    }

    if (resource == nullptr) {
      values.resize(size);
      return true;
    }

    const std::size_t original_size = values.size();
    while (values.size() < size) {
      if (!emplace_default_with_resource<element_type>(resource, [&values](auto&&... args) {
            values.emplace_back(std::forward<decltype(args)>(args)...);
          })) {
        values.resize(original_size);
        return false;
      }
    }

    return true;
  }
}

template <typename ForwardList> void trim_forward_list(ForwardList& values, std::size_t size) {
  auto before = values.before_begin();
  auto current = values.begin();
  std::size_t index = 0;
  while (current != values.end() && index < size) {
    ++before;
    ++current;
    ++index;
  }

  values.erase_after(before, values.end());
}

} // namespace detail

/** @brief `std::vector` / `caors::vector` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::vector<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<T>>) {
      return "caors.vector";
    } else {
      return "std.vector";
    }
  }

  [[nodiscard]] static std::size_t size(const std::vector<T, Allocator>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type* element(const std::vector<T, Allocator>& values,
                                                   std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::vector<T, Allocator>& values,
                                                     std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  static void clear(std::vector<T, Allocator>& values) { values.clear(); }

  [[nodiscard]] static bool resize(std::vector<T, Allocator>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool resize(std::vector<T, Allocator>& values, std::size_t size,
                                   memory::memory_resource* resource) {
    return detail::resize_back_sequence(values, size, resource);
  }

  [[nodiscard]] static bool append(std::vector<T, Allocator>& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

/** @brief `std::array` 的固定长度顺序容器反射适配。 */
template <typename T, std::size_t Size> struct sequence_traits<std::array<T, Size>> {
  static constexpr bool is_sequence = true;
  static constexpr std::size_t fixed_size = Size;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.array";
  }

  [[nodiscard]] static constexpr std::size_t size(const std::array<T, Size>&) noexcept {
    return Size;
  }

  [[nodiscard]] static const element_type* element(const std::array<T, Size>& values,
                                                   std::size_t index) noexcept {
    return index < Size ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::array<T, Size>& values,
                                                     std::size_t index) noexcept {
    return index < Size ? &values[index] : nullptr;
  }

  static void clear(std::array<T, Size>&) noexcept {}

  [[nodiscard]] static constexpr bool resize(std::array<T, Size>&, std::size_t size) noexcept {
    return size == Size;
  }

  [[nodiscard]] static constexpr bool append(std::array<T, Size>&, const element_type&) noexcept {
    return false;
  }
};

/** @brief `std::span` 的借用顺序容器反射适配。 */
template <typename T> struct sequence_traits<std::span<T, std::dynamic_extent>> {
  static constexpr bool is_sequence = true;
  static constexpr bool can_read = !std::is_const_v<T>;
  static constexpr bool read_in_place = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept { return "std.span"; }

  [[nodiscard]] static std::size_t size(const std::span<T, std::dynamic_extent>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type* element(const std::span<T, std::dynamic_extent>& values,
                                                   std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::span<T, std::dynamic_extent>& values,
                                                     std::size_t index) noexcept {
    if constexpr (std::is_const_v<T>) {
      (void)values;
      (void)index;
      return nullptr;
    } else {
      return index < values.size() ? &values[index] : nullptr;
    }
  }

  static void clear(std::span<T, std::dynamic_extent>&) noexcept {}

  [[nodiscard]] static bool resize(std::span<T, std::dynamic_extent>& values,
                                   std::size_t size) noexcept {
    return values.size() == size;
  }

  [[nodiscard]] static constexpr bool append(std::span<T, std::dynamic_extent>&,
                                             const element_type&) noexcept {
    return false;
  }
};

/** @brief 固定长度 `std::span` 的借用顺序容器反射适配。 */
template <typename T, std::size_t Extent> struct sequence_traits<std::span<T, Extent>> {
  static constexpr bool is_sequence = true;
  static constexpr bool can_read = !std::is_const_v<T>;
  static constexpr bool read_in_place = true;
  static constexpr std::size_t fixed_size = Extent;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept { return "std.span"; }

  [[nodiscard]] static constexpr std::size_t size(const std::span<T, Extent>&) noexcept {
    return Extent;
  }

  [[nodiscard]] static const element_type* element(const std::span<T, Extent>& values,
                                                   std::size_t index) noexcept {
    return index < Extent ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::span<T, Extent>& values,
                                                     std::size_t index) noexcept {
    if constexpr (std::is_const_v<T>) {
      (void)values;
      (void)index;
      return nullptr;
    } else {
      return index < Extent ? &values[index] : nullptr;
    }
  }

  static void clear(std::span<T, Extent>&) noexcept {}

  [[nodiscard]] static constexpr bool resize(std::span<T, Extent>&, std::size_t size) noexcept {
    return size == Extent;
  }

  [[nodiscard]] static constexpr bool append(std::span<T, Extent>&, const element_type&) noexcept {
    return false;
  }
};

/** @brief `std::deque` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::deque<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.deque";
  }

  [[nodiscard]] static std::size_t size(const std::deque<T, Allocator>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type* element(const std::deque<T, Allocator>& values,
                                                   std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::deque<T, Allocator>& values,
                                                     std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  static void clear(std::deque<T, Allocator>& values) { values.clear(); }

  [[nodiscard]] static bool resize(std::deque<T, Allocator>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool resize(std::deque<T, Allocator>& values, std::size_t size,
                                   memory::memory_resource* resource) {
    return detail::resize_back_sequence(values, size, resource);
  }

  [[nodiscard]] static bool append(std::deque<T, Allocator>& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

/** @brief `std::list` / `caors::list` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::list<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;
  using list_type = std::list<T, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<T>>) {
      return "caors.list";
    } else {
      return "std.list";
    }
  }

  [[nodiscard]] static std::size_t size(const list_type& values) noexcept { return values.size(); }

  [[nodiscard]] static const element_type* element(const list_type& values,
                                                   std::size_t index) noexcept {
    if (index >= values.size()) {
      return nullptr;
    }

    auto iterator = values.begin();
    std::advance(iterator, static_cast<typename list_type::difference_type>(index));
    return &*iterator;
  }

  [[nodiscard]] static element_type* mutable_element(list_type& values,
                                                     std::size_t index) noexcept {
    if (index >= values.size()) {
      return nullptr;
    }

    auto iterator = values.begin();
    std::advance(iterator, static_cast<typename list_type::difference_type>(index));
    return &*iterator;
  }

  static void clear(list_type& values) { values.clear(); }

  [[nodiscard]] static bool resize(list_type& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool resize(list_type& values, std::size_t size,
                                   memory::memory_resource* resource) {
    return detail::resize_back_sequence(values, size, resource);
  }

  [[nodiscard]] static bool append(list_type& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

/** @brief `std::forward_list` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::forward_list<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;
  using list_type = std::forward_list<T, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.forward_list";
  }

  [[nodiscard]] static std::size_t size(const list_type& values) noexcept {
    return static_cast<std::size_t>(std::distance(values.begin(), values.end()));
  }

  [[nodiscard]] static const element_type* element(const list_type& values,
                                                   std::size_t index) noexcept {
    auto iterator = values.begin();
    for (std::size_t current = 0; current < index && iterator != values.end(); ++current) {
      ++iterator;
    }

    return iterator != values.end() ? &*iterator : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(list_type& values,
                                                     std::size_t index) noexcept {
    auto iterator = values.begin();
    for (std::size_t current = 0; current < index && iterator != values.end(); ++current) {
      ++iterator;
    }

    return iterator != values.end() ? &*iterator : nullptr;
  }

  static void clear(list_type& values) { values.clear(); }

  [[nodiscard]] static bool resize(list_type& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool resize(list_type& values, std::size_t size,
                                   memory::memory_resource* resource) {
    const std::size_t current_size = sequence_traits<list_type>::size(values);
    if (size <= current_size) {
      detail::trim_forward_list(values, size);
      return true;
    }

    if constexpr (!std::is_default_constructible_v<T>) {
      (void)resource;
      return false;
    } else {
      if constexpr (!detail::has_resource_allocator_v<element_type>) {
        (void)resource;
        values.resize(size);
        return true;
      }

      if (resource == nullptr) {
        values.resize(size);
        return true;
      }

      auto before_end = values.before_begin();
      for (auto current = values.begin(); current != values.end(); ++current) {
        ++before_end;
      }

      std::size_t appended = current_size;
      while (appended < size) {
        if (!detail::emplace_default_with_resource<element_type>(resource, [&values, &before_end](
                                                                               auto&&... args) {
              before_end = values.emplace_after(before_end, std::forward<decltype(args)>(args)...);
            })) {
          detail::trim_forward_list(values, current_size);
          return false;
        }
        ++appended;
      }

      return true;
    }
  }

  [[nodiscard]] static bool append(list_type& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      auto before_end = values.before_begin();
      for (auto current = values.begin(); current != values.end(); ++current) {
        ++before_end;
      }

      values.insert_after(before_end, value);
      return true;
    } else {
      return false;
    }
  }
};

} // namespace caors::traits

#endif
