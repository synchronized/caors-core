// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_MAP_CODEC_H_
#define CAORS_SERIALIZATION_MAP_CODEC_H_

#include "caors/serialization/vector_codec.h"

#include <type_traits>
#include <unordered_map>
#include <utility>

namespace caors {

/** @brief `std::unordered_map` / `caors::hash_map` 的字典序列化 codec。 */
template <typename K, typename V, typename Hash, typename Equal, typename Allocator>
struct serialization_codec<std::unordered_map<K, V, Hash, Equal, Allocator>> {
  using map_type = std::unordered_map<K, V, Hash, Equal, Allocator>;

  static constexpr bool can_write = true;
  static constexpr bool can_read =
      std::is_default_constructible_v<K> && std::is_default_constructible_v<V>;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(serialization_writer& writer, const map_type& values) {
    error status = writer.begin_dictionary(values.size());
    if (status) {
      return status;
    }

    std::size_t index = 0;
    for (const auto& [key, value] : values) {
      status = writer.begin_entry(index);
      if (status) {
        return status;
      }

      status = writer.begin_key();
      if (status) {
        return status;
      }

      status = detail::write_vector_element(writer, key);
      if (status) {
        return status;
      }

      status = writer.end_key();
      if (status) {
        return status;
      }

      status = writer.begin_value();
      if (status) {
        return status;
      }

      status = detail::write_vector_element(writer, value);
      if (status) {
        return status;
      }

      status = writer.end_value();
      if (status) {
        return status;
      }

      status = writer.end_entry();
      if (status) {
        return status;
      }

      ++index;
    }

    return writer.end_dictionary();
  }

  [[nodiscard]] static error read(serialization_reader& reader, map_type& values) {
    std::size_t size = 0;
    error status = reader.begin_dictionary(size);
    if (status) {
      return status;
    }

    map_type decoded;
    decoded.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      status = reader.begin_entry(index);
      if (status) {
        return status;
      }

      status = reader.begin_key();
      if (status) {
        return status;
      }

      K key{};
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

      V value{};
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

      if (!decoded.emplace(std::move(key), std::move(value)).second) {
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

  [[nodiscard]] static variant_result make_value(const map_type& values) {
    variant::dictionary_type entries;
    entries.reserve(values.size());
    for (const auto& [key, value] : values) {
      variant_result encoded_key = detail::make_vector_element_value(key);
      if (encoded_key.failed()) {
        return encoded_key;
      }

      variant_result encoded_value = detail::make_vector_element_value(value);
      if (encoded_value.failed()) {
        return encoded_value;
      }

      entries.push_back(
          variant_entry{std::move(encoded_key.value), std::move(encoded_value.value)});
    }

    return {{}, variant::dictionary(std::move(entries))};
  }
};

} // namespace caors

#endif
