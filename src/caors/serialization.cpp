// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace caors {
namespace {

template <typename T>
[[nodiscard]] bool try_write_i64(serialization_writer& writer, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_i64(static_cast<std::int64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_u64(serialization_writer& writer, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_u64(static_cast<std::uint64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_f64(serialization_writer& writer, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_f64(static_cast<double>(*stored)).succeeded();
}

[[nodiscard]] bool try_write_serialized_value(serialization_writer& writer, const value& data) {
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

[[nodiscard]] serialization_value_result make_serialized_value(const value& data) {
  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(data.type());
  if (codec.has_value() && codec->make_value != nullptr) {
    return codec->make_value(data);
  }

  if (const bool* stored = data.try_get<bool>()) {
    return {{}, serialization_value::from(*stored)};
  }

  if (const char* stored = data.try_get<char>()) {
    return {{}, serialization_value::from(std::string_view{stored, 1})};
  }

  if (const signed char* stored = data.try_get<signed char>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const short* stored = data.try_get<short>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const int* stored = data.try_get<int>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long* stored = data.try_get<long>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long long* stored = data.try_get<long long>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }

  if (const unsigned char* stored = data.try_get<unsigned char>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned short* stored = data.try_get<unsigned short>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned int* stored = data.try_get<unsigned int>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long* stored = data.try_get<unsigned long>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long long* stored = data.try_get<unsigned long long>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }

  if (const float* stored = data.try_get<float>()) {
    return {{}, serialization_value::from(static_cast<double>(*stored))};
  }
  if (const double* stored = data.try_get<double>()) {
    return {{}, serialization_value::from(*stored)};
  }
  if (const long double* stored = data.try_get<long double>()) {
    return {{}, serialization_value::from(static_cast<double>(*stored))};
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return {{}, serialization_value::from(*stored)};
  }
  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return {{}, serialization_value::from(*stored)};
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

[[nodiscard]] string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

[[nodiscard]] bool field_visible_in_context(const serialization_field_data& field,
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

[[nodiscard]] error validate_field_for_writer(const serialization_field_data& field,
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

[[nodiscard]] error validate_field_for_reader(const serialization_field_data& field,
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

[[nodiscard]] error write_field(serialization_writer& writer, const reflection_field& field,
                                const void* object, const serialization_field_data& field_data) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.kind() != field_kind::value) {
    return {error_domain::serialization, errc::unsupported};
  }

  value field_value = field.get_value(object);
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

[[nodiscard]] error read_field(serialization_reader& reader, const reflection_field& field,
                               void* object, const serialization_field_data& field_data) {
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

[[nodiscard]] error read_configured_field(serialization_reader& reader,
                                          const reflection_field& field, void* object,
                                          const serialization_field_data& field_data) {
  const error status = read_field(reader, field, object, field_data);
  if (status.value() == errc::not_found && !field_data.required) {
    return {};
  }

  return status;
}

} // namespace

serialization_writer::~serialization_writer() = default;

serialization_reader::~serialization_reader() = default;

error write(serialization_writer& writer, const serialization_value& value) {
  switch (value.kind()) {
  case serialization_value_kind::null:
    return {error_domain::serialization, errc::unsupported};
  case serialization_value_kind::boolean:
    return writer.write_bool(value.as_bool());
  case serialization_value_kind::signed_integer:
    return writer.write_i64(value.as_i64());
  case serialization_value_kind::unsigned_integer:
    return writer.write_u64(value.as_u64());
  case serialization_value_kind::floating:
    return writer.write_f64(value.as_f64());
  case serialization_value_kind::string:
    return writer.write_string(value.as_string());
  case serialization_value_kind::array: {
    error status = writer.begin_array(value.as_array().size());
    if (status) {
      return status;
    }

    std::size_t index = 0;
    for (const serialization_value& element : value.as_array()) {
      status = writer.begin_element(index);
      if (status) {
        return status;
      }

      status = write(writer, element);
      if (status) {
        return status;
      }

      status = writer.end_element();
      if (status) {
        return status;
      }

      ++index;
    }

    return writer.end_array();
  }
  case serialization_value_kind::object: {
    serialization_type_data type_data;
    type_data.fields.reserve(value.as_object().size());
    for (const serialization_member& member : value.as_object()) {
      type_data.fields.push_back(serialization_field_data{
          member.name,
          member.name,
          member.field_id,
          0,
          0,
          false,
          false,
      });
    }

    error status = writer.begin_object(type_data);
    if (status) {
      return status;
    }

    for (std::size_t index = 0; index < value.as_object().size(); ++index) {
      const serialization_member& member = value.as_object()[index];
      status = validate_field_for_writer(type_data.fields[index], writer.features());
      if (status) {
        return status;
      }

      status = writer.begin_field(type_data.fields[index]);
      if (status) {
        return status;
      }

      status = write(writer, member.value);
      if (status) {
        return status;
      }

      status = writer.end_field();
      if (status) {
        return status;
      }
    }

    return writer.end_object();
  }
  }

  return {error_domain::serialization, errc::invalid_argument};
}

error serialize(const reflection_type& type, const void* object, serialization_writer& writer) {
  return serialize(type, object, writer, serialization_context{});
}

error serialize(const reflection_type& type, const void* object, serialization_writer& writer,
                const serialization_context& context) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = writer.begin_object(*serialization);
  if (status) {
    return status;
  }

  if (!serialization->fields.empty()) {
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped || !field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::not_found};
      }

      status = write_field(writer, *field, object, field_data);
      if (status) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization_field_data field_data = make_serialization_field(field->name().view());
      status = write_field(writer, *field, object, field_data);
      if (status) {
        return status;
      }
    }
  }

  return writer.end_object();
}

error deserialize(const reflection_type& type, void* object, serialization_reader& reader) {
  return deserialize(type, object, reader, serialization_context{});
}

error deserialize(const reflection_type& type, void* object, serialization_reader& reader,
                  const serialization_context& context) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = reader.begin_object(*serialization);
  if (status) {
    return status;
  }

  if (!serialization->fields.empty()) {
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped || !field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::not_found};
      }

      status = read_configured_field(reader, *field, object, field_data);
      if (status) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization_field_data field_data = make_serialization_field(field->name().view());
      status = read_field(reader, *field, object, field_data);
      if (status) {
        return status;
      }
    }
  }

  return reader.end_object();
}

serialization_value_result to_serialization_value(const reflection_type& type, const void* object) {
  return to_serialization_value(type, object, serialization_context{});
}

serialization_value_result to_serialization_value(const reflection_type& type, const void* object,
                                                  const serialization_context& context) {
  if (!type.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (object == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  serialization_value::object_type members;

  if (!serialization->fields.empty()) {
    members.reserve(serialization->fields.size());
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped || !field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::not_found}, {}};
      }

      value field_value = field->get_value(object);
      if (!field_value.valid()) {
        return {{error_domain::serialization, errc::unsupported}, {}};
      }

      serialization_value_result serialized = make_serialized_value(field_value);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(serialization_member{
          serialized_field_name(field_data),
          field_data.field_id,
          std::move(serialized.value),
      });
    }
  } else {
    members.reserve(type.field_count());
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::invalid_field}, {}};
      }

      value field_value = field->get_value(object);
      if (!field_value.valid()) {
        return {{error_domain::serialization, errc::unsupported}, {}};
      }

      serialization_value_result serialized = make_serialized_value(field_value);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(serialization_member{
          field->name(),
          0,
          std::move(serialized.value),
      });
    }
  }

  return {{}, serialization_value::object(std::move(members))};
}

error serialize(type_id id, const void* object, serialization_writer& writer) {
  return serialize(id, object, writer, serialization_context{});
}

error serialize(type_id id, const void* object, serialization_writer& writer,
                const serialization_context& context) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!get_serialization_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return serialize(*type, object, writer, context);
}

error deserialize(type_id id, void* object, serialization_reader& reader) {
  return deserialize(id, object, reader, serialization_context{});
}

error deserialize(type_id id, void* object, serialization_reader& reader,
                  const serialization_context& context) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!get_serialization_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return deserialize(*type, object, reader, context);
}

serialization_value_result to_serialization_value(type_id id, const void* object) {
  return to_serialization_value(id, object, serialization_context{});
}

serialization_value_result to_serialization_value(type_id id, const void* object,
                                                  const serialization_context& context) {
  if (!id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (!get_serialization_registry().contains(id)) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return to_serialization_value(*type, object, context);
}

serialization_text_result serialize_text(const reflection_type& type, const void* object) {
  return serialize_text(type, object, serialization_context{});
}

serialization_text_result serialize_text(const reflection_type& type, const void* object,
                                         const serialization_context& context) {
  text_serialization_writer writer;
  const error status = serialize(type, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

serialization_text_result serialize_text(type_id id, const void* object) {
  return serialize_text(id, object, serialization_context{});
}

serialization_text_result serialize_text(type_id id, const void* object,
                                         const serialization_context& context) {
  text_serialization_writer writer;
  const error status = serialize(id, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

} // namespace caors
