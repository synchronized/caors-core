// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MATH_SERIALIZATION_H_
#define CAORS_MATH_SERIALIZATION_H_

#include "caors/math.h"
#include "caors/serialization/codec.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace caors::serialization::math_codec_detail {

[[nodiscard]] inline error read_float(reader& reader, float& value) {
  double floating = 0.0;
  error status = reader.read_f64(floating);
  if (!status) {
    value = static_cast<float>(floating);
    return {};
  }

  std::int64_t signed_value = 0;
  status = reader.read_i64(signed_value);
  if (!status) {
    value = static_cast<float>(signed_value);
    return {};
  }

  std::uint64_t unsigned_value = 0;
  status = reader.read_u64(unsigned_value);
  if (!status) {
    value = static_cast<float>(unsigned_value);
    return {};
  }

  return status;
}

[[nodiscard]] inline error write_float_array(writer& writer, const float* values,
                                             std::size_t size) {
  error status = writer.begin_array(size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    status = writer.write_f64(values[index]);
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

[[nodiscard]] inline error read_float_array(reader& reader, float* values, std::size_t expected) {
  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != expected) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    status = read_float(reader, values[index]);
    if (status) {
      return status;
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] inline variant_result make_float_array(const float* values, std::size_t size) {
  variant::array_type elements;
  elements.reserve(size);
  for (std::size_t index = 0; index < size; ++index) {
    elements.push_back(variant::from(static_cast<double>(values[index])));
  }
  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] inline const type_schema& rect_schema() {
  static const type_schema schema = make_type_schema({make_field("position"), make_field("size")});
  return schema;
}

[[nodiscard]] inline const type_schema& aabb_schema() {
  static const type_schema schema = make_type_schema({make_field("min"), make_field("max")});
  return schema;
}

} // namespace caors::serialization::math_codec_detail

namespace caors::serialization {

/** @brief `math::vec2` 按 `[x, y]` 数组读写。 */
template <> struct codec_traits<math::vec2> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, math::vec2 value) {
    const float values[2]{value.x, value.y};
    return math_codec_detail::write_float_array(writer, values, 2);
  }

  [[nodiscard]] static error read(reader& reader, math::vec2& value) {
    float values[2]{};
    const error status = math_codec_detail::read_float_array(reader, values, 2);
    if (status) {
      return status;
    }

    value = {values[0], values[1]};
    return {};
  }

  [[nodiscard]] static variant_result make_value(math::vec2 value) {
    const float values[2]{value.x, value.y};
    return math_codec_detail::make_float_array(values, 2);
  }
};

/** @brief `math::vec3` 按 `[x, y, z]` 数组读写。 */
template <> struct codec_traits<math::vec3> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, math::vec3 value) {
    const float values[3]{value.x, value.y, value.z};
    return math_codec_detail::write_float_array(writer, values, 3);
  }

  [[nodiscard]] static error read(reader& reader, math::vec3& value) {
    float values[3]{};
    const error status = math_codec_detail::read_float_array(reader, values, 3);
    if (status) {
      return status;
    }

    value = {values[0], values[1], values[2]};
    return {};
  }

  [[nodiscard]] static variant_result make_value(math::vec3 value) {
    const float values[3]{value.x, value.y, value.z};
    return math_codec_detail::make_float_array(values, 3);
  }
};

/** @brief `math::vec4` 按 `[x, y, z, w]` 数组读写。 */
template <> struct codec_traits<math::vec4> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, math::vec4 value) {
    const float values[4]{value.x, value.y, value.z, value.w};
    return math_codec_detail::write_float_array(writer, values, 4);
  }

  [[nodiscard]] static error read(reader& reader, math::vec4& value) {
    float values[4]{};
    const error status = math_codec_detail::read_float_array(reader, values, 4);
    if (status) {
      return status;
    }

    value = {values[0], values[1], values[2], values[3]};
    return {};
  }

  [[nodiscard]] static variant_result make_value(math::vec4 value) {
    const float values[4]{value.x, value.y, value.z, value.w};
    return math_codec_detail::make_float_array(values, 4);
  }
};

/** @brief `math::quat` 按 `[x, y, z, w]` 数组读写，其中 `w` 是标量部分。 */
template <> struct codec_traits<math::quat> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, math::quat value) {
    const float values[4]{value.x, value.y, value.z, value.w};
    return math_codec_detail::write_float_array(writer, values, 4);
  }

  [[nodiscard]] static error read(reader& reader, math::quat& value) {
    float values[4]{};
    const error status = math_codec_detail::read_float_array(reader, values, 4);
    if (status) {
      return status;
    }

    value = {values[0], values[1], values[2], values[3]};
    return {};
  }

  [[nodiscard]] static variant_result make_value(math::quat value) {
    const float values[4]{value.x, value.y, value.z, value.w};
    return math_codec_detail::make_float_array(values, 4);
  }
};

/** @brief `math::mat3` 按行主序 9 元素数组读写。 */
template <> struct codec_traits<math::mat3> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const math::mat3& value) {
    return math_codec_detail::write_float_array(writer, value.values, 9);
  }

  [[nodiscard]] static error read(reader& reader, math::mat3& value) {
    return math_codec_detail::read_float_array(reader, value.values, 9);
  }

  [[nodiscard]] static variant_result make_value(const math::mat3& value) {
    return math_codec_detail::make_float_array(value.values, 9);
  }
};

/** @brief `math::mat4` 按行主序 16 元素数组读写。 */
template <> struct codec_traits<math::mat4> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const math::mat4& value) {
    return math_codec_detail::write_float_array(writer, value.values, 16);
  }

  [[nodiscard]] static error read(reader& reader, math::mat4& value) {
    return math_codec_detail::read_float_array(reader, value.values, 16);
  }

  [[nodiscard]] static variant_result make_value(const math::mat4& value) {
    return math_codec_detail::make_float_array(value.values, 16);
  }
};

/** @brief `math::rect` 按 `{ position, size }` 对象读写。 */
template <> struct codec_traits<math::rect> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const math::rect& value) {
    const type_schema& schema = math_codec_detail::rect_schema();
    error status = writer.begin_object(schema);
    if (status) {
      return status;
    }

    status = writer.begin_field(schema.fields[0]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec2>::write(writer, value.position);
    if (status) {
      return status;
    }
    status = writer.end_field();
    if (status) {
      return status;
    }

    status = writer.begin_field(schema.fields[1]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec2>::write(writer, value.size);
    if (status) {
      return status;
    }
    status = writer.end_field();
    if (status) {
      return status;
    }

    return writer.end_object();
  }

  [[nodiscard]] static error read(reader& reader, math::rect& value) {
    const type_schema& schema = math_codec_detail::rect_schema();
    error status = reader.begin_object(schema);
    if (status) {
      return status;
    }

    status = reader.begin_field(schema.fields[0]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec2>::read(reader, value.position);
    if (status) {
      return status;
    }
    status = reader.end_field();
    if (status) {
      return status;
    }

    status = reader.begin_field(schema.fields[1]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec2>::read(reader, value.size);
    if (status) {
      return status;
    }
    status = reader.end_field();
    if (status) {
      return status;
    }

    return reader.end_object();
  }

  [[nodiscard]] static variant_result make_value(const math::rect& value) {
    variant_result position = codec_traits<math::vec2>::make_value(value.position);
    if (position.failed()) {
      return position;
    }
    variant_result size = codec_traits<math::vec2>::make_value(value.size);
    if (size.failed()) {
      return size;
    }

    variant::object_type members;
    members.reserve(2);
    members.push_back(meta::variant_member{string_name{"position"}, 0, std::move(position.value)});
    members.push_back(meta::variant_member{string_name{"size"}, 0, std::move(size.value)});
    return {{}, variant::object(std::move(members))};
  }
};

/** @brief `math::aabb` 按 `{ min, max }` 对象读写。 */
template <> struct codec_traits<math::aabb> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const math::aabb& value) {
    const type_schema& schema = math_codec_detail::aabb_schema();
    error status = writer.begin_object(schema);
    if (status) {
      return status;
    }

    status = writer.begin_field(schema.fields[0]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec3>::write(writer, value.min);
    if (status) {
      return status;
    }
    status = writer.end_field();
    if (status) {
      return status;
    }

    status = writer.begin_field(schema.fields[1]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec3>::write(writer, value.max);
    if (status) {
      return status;
    }
    status = writer.end_field();
    if (status) {
      return status;
    }

    return writer.end_object();
  }

  [[nodiscard]] static error read(reader& reader, math::aabb& value) {
    const type_schema& schema = math_codec_detail::aabb_schema();
    error status = reader.begin_object(schema);
    if (status) {
      return status;
    }

    status = reader.begin_field(schema.fields[0]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec3>::read(reader, value.min);
    if (status) {
      return status;
    }
    status = reader.end_field();
    if (status) {
      return status;
    }

    status = reader.begin_field(schema.fields[1]);
    if (status) {
      return status;
    }
    status = codec_traits<math::vec3>::read(reader, value.max);
    if (status) {
      return status;
    }
    status = reader.end_field();
    if (status) {
      return status;
    }

    return reader.end_object();
  }

  [[nodiscard]] static variant_result make_value(const math::aabb& value) {
    variant_result min = codec_traits<math::vec3>::make_value(value.min);
    if (min.failed()) {
      return min;
    }
    variant_result max = codec_traits<math::vec3>::make_value(value.max);
    if (max.failed()) {
      return max;
    }

    variant::object_type members;
    members.reserve(2);
    members.push_back(meta::variant_member{string_name{"min"}, 0, std::move(min.value)});
    members.push_back(meta::variant_member{string_name{"max"}, 0, std::move(max.value)});
    return {{}, variant::object(std::move(members))};
  }
};

} // namespace caors::serialization

#endif
