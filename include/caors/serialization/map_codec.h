// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_MAP_CODEC_H_
#define CAORS_SERIALIZATION_MAP_CODEC_H_

#include "caors/serialization/vector_codec.h"

#include <map>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace caors::serialization {

/** @brief 基于 `traits::associative_traits<T>` 的通用关联容器序列化 codec。 */
template <typename Associative> struct associative_codec {
  using container_traits = traits::associative_traits<Associative>;
  using key_type = typename container_traits::key_type;
  using mapped_type = typename container_traits::mapped_type;

  static constexpr bool can_write = container_traits::is_associative;
  static constexpr bool can_read =
      container_traits::is_associative && std::is_default_constructible_v<Associative> &&
      std::is_default_constructible_v<key_type> && std::is_default_constructible_v<mapped_type>;
  static constexpr bool can_make_value = container_traits::is_associative;

  [[nodiscard]] static error write(writer& writer, const Associative& values) {
    error status = writer.begin_dictionary(container_traits::size(values));
    if (status) {
      return status;
    }

    std::size_t index = 0;
    error visitor_status{};
    container_traits::for_each(values, [&](const key_type& key, const mapped_type& value) {
      if (visitor_status) {
        return;
      }

      status = writer.begin_entry(index);
      if (status) {
        visitor_status = status;
        return;
      }

      status = writer.begin_key();
      if (status) {
        visitor_status = status;
        return;
      }

      status = detail::write_vector_element(writer, key);
      if (status) {
        visitor_status = status;
        return;
      }

      status = writer.end_key();
      if (status) {
        visitor_status = status;
        return;
      }

      status = writer.begin_value();
      if (status) {
        visitor_status = status;
        return;
      }

      status = detail::write_vector_element(writer, value);
      if (status) {
        visitor_status = status;
        return;
      }

      status = writer.end_value();
      if (status) {
        visitor_status = status;
        return;
      }

      status = writer.end_entry();
      if (status) {
        visitor_status = status;
        return;
      }

      ++index;
    });

    return visitor_status ? visitor_status : writer.end_dictionary();
  }

  [[nodiscard]] static error read(reader& reader, Associative& values) {
    std::size_t size = 0;
    error status = reader.begin_dictionary(size);
    if (status) {
      return status;
    }

    Associative decoded{};
    if constexpr (requires(Associative& container, std::size_t count) {
                    container.reserve(count);
                  }) {
      decoded.reserve(size);
    }

    for (std::size_t index = 0; index < size; ++index) {
      status = reader.begin_entry(index);
      if (status) {
        return status;
      }

      status = reader.begin_key();
      if (status) {
        return status;
      }

      key_type key{};
      status = detail::read_vector_element(reader, key);
      if (status) {
        return status;
      }

      status = reader.end_key();
      if (status) {
        return status;
      }

      status = reader.begin_value();
      if (status) {
        return status;
      }

      mapped_type value{};
      status = detail::read_vector_element(reader, value);
      if (status) {
        return status;
      }

      status = reader.end_value();
      if (status) {
        return status;
      }

      status = reader.end_entry();
      if (status) {
        return status;
      }

      if (!container_traits::insert(decoded, std::move(key), std::move(value))) {
        return {error_domain::serialization, errc::already_exists};
      }
    }

    status = reader.end_dictionary();
    if (status) {
      return status;
    }

    values = std::move(decoded);
    return {};
  }

  [[nodiscard]] static variant_result make_value(const Associative& values) {
    variant::dictionary_type entries;
    entries.reserve(container_traits::size(values));
    variant_result visitor_result{};

    container_traits::for_each(values, [&](const key_type& key, const mapped_type& value) {
      if (visitor_result.failed()) {
        return;
      }

      variant_result encoded_key = detail::make_vector_element_value(key);
      if (encoded_key.failed()) {
        visitor_result = encoded_key;
        return;
      }

      variant_result encoded_value = detail::make_vector_element_value(value);
      if (encoded_value.failed()) {
        visitor_result = encoded_value;
        return;
      }

      entries.push_back(
          variant_entry{std::move(encoded_key.value), std::move(encoded_value.value)});
    });

    if (visitor_result.failed()) {
      return visitor_result;
    }

    return {{}, variant::dictionary(std::move(entries))};
  }
};

/** @brief `std::unordered_map` / `caors::hash_map` 的字典序列化 codec。 */
template <typename K, typename V, typename Hash, typename Equal, typename Allocator>
struct codec<std::unordered_map<K, V, Hash, Equal, Allocator>>
    : associative_codec<std::unordered_map<K, V, Hash, Equal, Allocator>> {};

/** @brief `std::map` 的字典序列化 codec。 */
template <typename K, typename V, typename Compare, typename Allocator>
struct codec<std::map<K, V, Compare, Allocator>>
    : associative_codec<std::map<K, V, Compare, Allocator>> {};

} // namespace caors::serialization

#endif
