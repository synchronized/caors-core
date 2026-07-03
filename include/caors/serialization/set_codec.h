// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_SET_CODEC_H_
#define CAORS_SERIALIZATION_SET_CODEC_H_

#include "caors/serialization/vector_codec.h"

#include <set>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace caors::serialization {

/** @brief 基于 `adapters::set_traits<T>` 的通用集合容器序列化 codec。 */
template <typename Set> struct set_codec {
  using traits = adapters::set_traits<Set>;
  using element_type = typename traits::element_type;

  static constexpr bool can_write = traits::is_set;
  static constexpr bool can_read = traits::is_set && std::is_default_constructible_v<Set> &&
                                   std::is_default_constructible_v<element_type>;
  static constexpr bool can_make_value = traits::is_set;

  [[nodiscard]] static error write(serialization_writer& writer, const Set& values) {
    error status = writer.begin_array(traits::size(values));
    if (status) {
      return status;
    }

    std::size_t index = 0;
    error visitor_status{};
    traits::for_each(values, [&](const element_type& value) {
      if (visitor_status) {
        return;
      }

      status = writer.begin_element(index);
      if (status) {
        visitor_status = status;
        return;
      }

      status = detail::write_vector_element(writer, value);
      if (status) {
        visitor_status = status;
        return;
      }

      status = writer.end_element();
      if (status) {
        visitor_status = status;
        return;
      }

      ++index;
    });

    return visitor_status ? visitor_status : writer.end_array();
  }

  [[nodiscard]] static error read(serialization_reader& reader, Set& values) {
    std::size_t size = 0;
    error status = reader.begin_array(size);
    if (status) {
      return status;
    }

    Set decoded{};
    if constexpr (requires(Set& container, std::size_t count) { container.reserve(count); }) {
      decoded.reserve(size);
    }

    for (std::size_t index = 0; index < size; ++index) {
      status = reader.begin_element(index);
      if (status) {
        return status;
      }

      element_type element{};
      status = detail::read_vector_element(reader, element);
      if (status) {
        return status;
      }

      if (!traits::insert(decoded, std::move(element))) {
        return {error_domain::serialization, errc::already_exists};
      }

      status = reader.end_element();
      if (status) {
        return status;
      }
    }

    status = reader.end_array();
    if (status) {
      return status;
    }

    values = std::move(decoded);
    return {};
  }

  [[nodiscard]] static variant_result make_value(const Set& values) {
    variant::array_type elements;
    elements.reserve(traits::size(values));
    variant_result visitor_result{};

    traits::for_each(values, [&](const element_type& value) {
      if (visitor_result.failed()) {
        return;
      }

      variant_result element = detail::make_vector_element_value(value);
      if (element.failed()) {
        visitor_result = element;
        return;
      }

      elements.push_back(std::move(element.value));
    });

    if (visitor_result.failed()) {
      return visitor_result;
    }

    return {{}, variant::array(std::move(elements))};
  }
};

/** @brief `std::unordered_set` / `caors::hash_set` 的数组序列化 codec。 */
template <typename T, typename Hash, typename Equal, typename Allocator>
struct codec<std::unordered_set<T, Hash, Equal, Allocator>>
    : set_codec<std::unordered_set<T, Hash, Equal, Allocator>> {};

/** @brief `std::set` 的数组序列化 codec。 */
template <typename T, typename Compare, typename Allocator>
struct codec<std::set<T, Compare, Allocator>> : set_codec<std::set<T, Compare, Allocator>> {};

} // namespace caors::serialization

#endif
