// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_internal.h"

#include <limits>
#include <string>
#include <string_view>

namespace caors::detail {
namespace {

template <typename T>
[[nodiscard]] bool try_write_i64(serialization_writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_i64(static_cast<std::int64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_u64(serialization_writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_u64(static_cast<std::uint64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_f64(serialization_writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_f64(static_cast<double>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_read_signed_field(serialization_reader& reader,
                                         const reflection_field& field, void* object) {
  std::int64_t value = 0;
  if (reader.read_i64(value).succeeded()) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return field.try_set<T>(object, static_cast<T>(value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).failed() ||
      unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return field.try_set<T>(object, static_cast<T>(unsigned_value));
}

template <typename T>
[[nodiscard]] bool try_read_unsigned_field(serialization_reader& reader,
                                           const reflection_field& field, void* object) {
  std::uint64_t value = 0;
  if (reader.read_u64(value).succeeded()) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return field.try_set<T>(object, static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).failed() || signed_value < 0 ||
      static_cast<std::uint64_t>(signed_value) >
          static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return field.try_set<T>(object, static_cast<T>(signed_value));
}

template <typename T>
[[nodiscard]] bool try_read_float_field(serialization_reader& reader, const reflection_field& field,
                                        void* object) {
  double value = 0.0;
  if (reader.read_f64(value).succeeded()) {
    return field.try_set<T>(object, static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return field.try_set<T>(object, static_cast<T>(signed_value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return field.try_set<T>(object, static_cast<T>(unsigned_value));
  }

  return false;
}

} // namespace

string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

bool field_visible_in_context(const serialization_field_data& field,
                              const serialization_context& context) noexcept {
  if (context.schema_version == 0) {
    return true;
  }

  if (field.version_added != 0 && context.schema_version < field.version_added) {
    return false;
  }

  if (field.version_removed != 0 && context.schema_version >= field.version_removed) {
    return false;
  }

  return true;
}

error validate_field_for_writer(const serialization_field_data& field,
                                serialization_format_features features) {
  if (has_feature(features, serialization_format_features::named_fields) &&
      serialized_field_name(field).empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (has_feature(features, serialization_format_features::numeric_fields) && field.field_id == 0) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

error validate_field_for_reader(const serialization_field_data& field,
                                serialization_format_features features) {
  const bool can_use_name = has_feature(features, serialization_format_features::named_fields) &&
                            !serialized_field_name(field).empty();
  const bool can_use_id =
      has_feature(features, serialization_format_features::numeric_fields) && field.field_id != 0;

  if (!can_use_name && !can_use_id) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

bool try_write_serialized_value(serialization_writer& writer, const any& data) {
  if (const bool* stored = data.try_get<bool>()) {
    return writer.write_bool(*stored).succeeded();
  }

  if (const char* stored = data.try_get<char>()) {
    return writer.write_string(std::string_view{stored, 1}).succeeded();
  }

  if (try_write_i64<signed char>(writer, data) || try_write_i64<short>(writer, data) ||
      try_write_i64<int>(writer, data) || try_write_i64<long>(writer, data) ||
      try_write_i64<long long>(writer, data) || try_write_u64<unsigned char>(writer, data) ||
      try_write_u64<unsigned short>(writer, data) || try_write_u64<unsigned int>(writer, data) ||
      try_write_u64<unsigned long>(writer, data) ||
      try_write_u64<unsigned long long>(writer, data) || try_write_f64<float>(writer, data) ||
      try_write_f64<double>(writer, data) || try_write_f64<long double>(writer, data)) {
    return true;
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return writer.write_string(*stored).succeeded();
  }

  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return writer.write_string(*stored).succeeded();
  }

  return false;
}

variant_result make_serialized_value(const any& data) {
  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(data.type());
  if (codec.has_value() && codec->make_value != nullptr) {
    return codec->make_value(data);
  }

  if (const bool* stored = data.try_get<bool>()) {
    return {{}, variant::from(*stored)};
  }

  if (const char* stored = data.try_get<char>()) {
    return {{}, variant::from(std::string_view{stored, 1})};
  }

  if (const signed char* stored = data.try_get<signed char>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const short* stored = data.try_get<short>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const int* stored = data.try_get<int>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long* stored = data.try_get<long>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long long* stored = data.try_get<long long>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }

  if (const unsigned char* stored = data.try_get<unsigned char>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned short* stored = data.try_get<unsigned short>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned int* stored = data.try_get<unsigned int>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long* stored = data.try_get<unsigned long>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long long* stored = data.try_get<unsigned long long>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }

  if (const float* stored = data.try_get<float>()) {
    return {{}, variant::from(static_cast<double>(*stored))};
  }
  if (const double* stored = data.try_get<double>()) {
    return {{}, variant::from(*stored)};
  }
  if (const long double* stored = data.try_get<long double>()) {
    return {{}, variant::from(static_cast<double>(*stored))};
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return {{}, variant::from(*stored)};
  }
  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return {{}, variant::from(*stored)};
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

error write_field(serialization_writer& writer, const reflection_field& field, const void* object,
                  const serialization_field_data& field_data) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.kind() != field_kind::value) {
    return {error_domain::serialization, errc::unsupported};
  }

  any field_value = field.get_value(object);
  if (!field_value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = validate_field_for_writer(field_data, writer.features());
  if (status) {
    return status;
  }

  status = writer.begin_field(field_data);
  if (status) {
    return status;
  }

  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(field_value.type());
  if (codec.has_value() && codec->write != nullptr) {
    status = codec->write(writer, field_value);
    if (status) {
      return status;
    }

    return writer.end_field();
  }

  if (!try_write_serialized_value(writer, field_value)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return writer.end_field();
}

error read_field(serialization_reader& reader, const reflection_field& field, void* object,
                 const serialization_field_data& field_data) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (!field.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.kind() != field_kind::value) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = validate_field_for_reader(field_data, reader.features());
  if (status) {
    return status;
  }

  status = reader.begin_field(field_data);
  if (status) {
    return status;
  }

  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(field.type());
  if (codec.has_value() && codec->read != nullptr) {
    status = codec->read(reader, field, object);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  bool bool_value = false;
  bool written =
      reader.read_bool(bool_value).succeeded() && field.try_set<bool>(object, bool_value);

  if (!written) {
    written = try_read_signed_field<signed char>(reader, field, object) ||
              try_read_signed_field<short>(reader, field, object) ||
              try_read_signed_field<int>(reader, field, object) ||
              try_read_signed_field<long>(reader, field, object) ||
              try_read_signed_field<long long>(reader, field, object);
  }

  if (!written) {
    written = try_read_unsigned_field<unsigned char>(reader, field, object) ||
              try_read_unsigned_field<unsigned short>(reader, field, object) ||
              try_read_unsigned_field<unsigned int>(reader, field, object) ||
              try_read_unsigned_field<unsigned long>(reader, field, object) ||
              try_read_unsigned_field<unsigned long long>(reader, field, object);
  }

  if (!written) {
    written = try_read_float_field<float>(reader, field, object) ||
              try_read_float_field<double>(reader, field, object) ||
              try_read_float_field<long double>(reader, field, object);
  }

  if (!written) {
    string string_value;
    if (reader.read_string(string_value).succeeded()) {
      const std::string standard_string{string_value.data(), string_value.size()};
      written = field.try_set<std::string>(object, standard_string);
    }
  }

  if (!written) {
    return {error_domain::serialization, errc::unsupported};
  }

  return reader.end_field();
}

error read_configured_field(serialization_reader& reader, const reflection_field& field,
                            void* object, const serialization_field_data& field_data) {
  const error status = read_field(reader, field, object, field_data);
  if (status.value() == errc::not_found && !field_data.required) {
    return {};
  }

  return status;
}

} // namespace caors::detail
