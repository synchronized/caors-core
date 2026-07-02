// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_VECTOR_CODEC_H_
#define CAORS_SERIALIZATION_VECTOR_CODEC_H_

#include "caors/serialization/operations.h"

#include <array>
#include <deque>
#include <forward_list>
#include <limits>
#include <list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace caors::detail {

template <typename T>
[[nodiscard]] error write_vector_element(serialization_writer& writer, const T& value) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_same_v<type, bool>) {
    return writer.write_bool(value);
  } else if constexpr (std::is_same_v<type, char>) {
    return writer.write_string(std::string_view{&value, 1});
  } else if constexpr (std::is_integral_v<type> && std::is_signed_v<type>) {
    return writer.write_i64(static_cast<std::int64_t>(value));
  } else if constexpr (std::is_integral_v<type> && std::is_unsigned_v<type>) {
    return writer.write_u64(static_cast<std::uint64_t>(value));
  } else if constexpr (std::is_floating_point_v<type>) {
    return writer.write_f64(static_cast<double>(value));
  } else if constexpr (std::is_same_v<type, string> || std::is_same_v<type, std::string> ||
                       std::is_same_v<type, std::string_view>) {
    return writer.write_string(value);
  } else {
    const std::optional<meta_type> element_type = caors::find_type<type>();
    if (!element_type.has_value()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return caors::serialize(element_type->id(), &value, writer);
  }
}

template <typename T>
[[nodiscard]] error read_vector_element(serialization_reader& reader, T& value) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_same_v<type, bool>) {
    return reader.read_bool(value);
  } else if constexpr (std::is_same_v<type, char>) {
    string decoded;
    error status = reader.read_string(decoded);
    if (status) {
      return status;
    }

    if (decoded.size() != 1) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    value = decoded[0];
    return {};
  } else if constexpr (std::is_integral_v<type> && std::is_signed_v<type>) {
    std::int64_t decoded = 0;
    error status = reader.read_i64(decoded);
    if (status) {
      std::uint64_t unsigned_decoded = 0;
      status = reader.read_u64(unsigned_decoded);
      if (status) {
        return status;
      }

      if (unsigned_decoded > static_cast<std::uint64_t>(std::numeric_limits<type>::max())) {
        return {error_domain::serialization, errc::invalid_argument};
      }

      value = static_cast<type>(unsigned_decoded);
      return {};
    }

    if (decoded < static_cast<std::int64_t>(std::numeric_limits<type>::min()) ||
        decoded > static_cast<std::int64_t>(std::numeric_limits<type>::max())) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    value = static_cast<type>(decoded);
    return {};
  } else if constexpr (std::is_integral_v<type> && std::is_unsigned_v<type>) {
    std::uint64_t decoded = 0;
    error status = reader.read_u64(decoded);
    if (status) {
      return status;
    }

    if (decoded > static_cast<std::uint64_t>(std::numeric_limits<type>::max())) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    value = static_cast<type>(decoded);
    return {};
  } else if constexpr (std::is_floating_point_v<type>) {
    double decoded = 0.0;
    error status = reader.read_f64(decoded);
    if (status) {
      return status;
    }

    value = static_cast<type>(decoded);
    return {};
  } else if constexpr (std::is_same_v<type, string>) {
    return reader.read_string(value);
  } else if constexpr (std::is_same_v<type, std::string>) {
    string decoded;
    error status = reader.read_string(decoded);
    if (status) {
      return status;
    }

    value.assign(decoded.data(), decoded.size());
    return {};
  } else {
    const std::optional<meta_type> element_type = caors::find_type<type>();
    if (!element_type.has_value()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return caors::deserialize(element_type->id(), &value, reader);
  }
}

template <typename T> [[nodiscard]] variant_result make_vector_element_value(const T& value) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_same_v<type, bool>) {
    return {{}, variant::from(value)};
  } else if constexpr (std::is_same_v<type, char>) {
    return {{}, variant::from(std::string_view{&value, 1})};
  } else if constexpr (std::is_integral_v<type> && std::is_signed_v<type>) {
    return {{}, variant::from(static_cast<std::int64_t>(value))};
  } else if constexpr (std::is_integral_v<type> && std::is_unsigned_v<type>) {
    return {{}, variant::from(static_cast<std::uint64_t>(value))};
  } else if constexpr (std::is_floating_point_v<type>) {
    return {{}, variant::from(static_cast<double>(value))};
  } else if constexpr (std::is_same_v<type, string> || std::is_same_v<type, std::string> ||
                       std::is_same_v<type, std::string_view>) {
    return {{}, variant::from(value)};
  } else {
    const std::optional<meta_type> element_type = caors::find_type<type>();
    if (!element_type.has_value()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return caors::to_serialization_value(element_type->id(), &value);
  }
}

} // namespace caors::detail

namespace caors {

/** @brief 基于 `sequence_traits<T>` 的通用顺序容器序列化 codec。 */
template <typename Sequence> struct sequence_serialization_codec {
  using traits = sequence_traits<Sequence>;
  using element_type = typename traits::element_type;

  static constexpr bool traits_can_read = [] {
    if constexpr (requires { traits::can_read; }) {
      return traits::can_read;
    } else {
      return true;
    }
  }();
  static constexpr bool can_write = traits::is_sequence;
  static constexpr bool can_read = traits::is_sequence && traits_can_read &&
                                   std::is_default_constructible_v<Sequence> &&
                                   std::is_default_constructible_v<element_type>;
  static constexpr bool can_make_value = traits::is_sequence;

  [[nodiscard]] static error write(serialization_writer& writer, const Sequence& values) {
    const std::size_t size = traits::size(values);
    error status = writer.begin_array(size);
    if (status) {
      return status;
    }

    for (std::size_t index = 0; index < size; ++index) {
      status = writer.begin_element(index);
      if (status) {
        return status;
      }

      const element_type* element = traits::element(values, index);
      if (element == nullptr) {
        return {error_domain::serialization, errc::invalid_argument};
      }

      status = detail::write_vector_element(writer, *element);
      if (status) {
        return status;
      }

      status = writer.end_element();
      if (status) {
        return status;
      }
    }

    return writer.end_array();
  }

  [[nodiscard]] static error read(serialization_reader& reader, Sequence& values) {
    std::size_t size = 0;
    error status = reader.begin_array(size);
    if (status) {
      return status;
    }

    Sequence decoded{};
    if constexpr (requires(Sequence& container, std::size_t count) { container.reserve(count); }) {
      decoded.reserve(size);
    }

    traits::clear(decoded);
    if (!traits::resize(decoded, size)) {
      return {error_domain::serialization, errc::unsupported};
    }

    for (std::size_t index = 0; index < size; ++index) {
      status = reader.begin_element(index);
      if (status) {
        return status;
      }

      element_type* element = traits::mutable_element(decoded, index);
      if (element == nullptr) {
        return {error_domain::serialization, errc::invalid_argument};
      }

      status = detail::read_vector_element(reader, *element);
      if (status) {
        return status;
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

  [[nodiscard]] static variant_result make_value(const Sequence& values) {
    variant::array_type elements;
    const std::size_t size = traits::size(values);
    elements.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      const element_type* value = traits::element(values, index);
      if (value == nullptr) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }

      variant_result element = detail::make_vector_element_value(*value);
      if (element.failed()) {
        return element;
      }

      elements.push_back(std::move(element.value));
    }

    return {{}, variant::array(std::move(elements))};
  }
};

/** @brief `std::vector` / `caors::vector` 的数组序列化 codec。 */
template <typename T, typename Allocator>
struct serialization_codec<std::vector<T, Allocator>>
    : sequence_serialization_codec<std::vector<T, Allocator>> {};

/** @brief `std::array` 的固定长度数组序列化 codec。 */
template <typename T, std::size_t Size>
struct serialization_codec<std::array<T, Size>>
    : sequence_serialization_codec<std::array<T, Size>> {};

/** @brief `std::deque` 的数组序列化 codec。 */
template <typename T, typename Allocator>
struct serialization_codec<std::deque<T, Allocator>>
    : sequence_serialization_codec<std::deque<T, Allocator>> {};

/** @brief `std::list` / `caors::list` 的数组序列化 codec。 */
template <typename T, typename Allocator>
struct serialization_codec<std::list<T, Allocator>>
    : sequence_serialization_codec<std::list<T, Allocator>> {};

/** @brief `std::forward_list` 的数组序列化 codec。 */
template <typename T, typename Allocator>
struct serialization_codec<std::forward_list<T, Allocator>>
    : sequence_serialization_codec<std::forward_list<T, Allocator>> {};

} // namespace caors

#endif
