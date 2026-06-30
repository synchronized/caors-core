// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_VECTOR_CODEC_H_
#define CAORS_SERIALIZATION_VECTOR_CODEC_H_

#include "caors/serialization/operations.h"

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
  } else if constexpr (std::is_integral_v<type> && std::is_signed_v<type>) {
    std::int64_t decoded = 0;
    error status = reader.read_i64(decoded);
    if (status) {
      return status;
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

/** @brief `caors::vector<T>` 的数组序列化 codec。 */
template <typename T> struct serialization_codec<vector<T>> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = std::is_default_constructible_v<T>;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(serialization_writer& writer, const vector<T>& values) {
    error status = writer.begin_array(values.size());
    if (status) {
      return status;
    }

    for (std::size_t index = 0; index < values.size(); ++index) {
      status = writer.begin_element(index);
      if (status) {
        return status;
      }

      status = detail::write_vector_element(writer, values[index]);
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

  [[nodiscard]] static error read(serialization_reader& reader, vector<T>& values) {
    std::size_t size = 0;
    error status = reader.begin_array(size);
    if (status) {
      return status;
    }

    vector<T> decoded;
    decoded.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      status = reader.begin_element(index);
      if (status) {
        return status;
      }

      T element{};
      status = detail::read_vector_element(reader, element);
      if (status) {
        return status;
      }

      decoded.push_back(std::move(element));
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

  [[nodiscard]] static variant_result make_value(const vector<T>& values) {
    variant::array_type elements;
    elements.reserve(values.size());
    for (const T& value : values) {
      variant_result element = detail::make_vector_element_value(value);
      if (element.failed()) {
        return element;
      }

      elements.push_back(std::move(element.value));
    }

    return {{}, variant::array(std::move(elements))};
  }
};

} // namespace caors

#endif
