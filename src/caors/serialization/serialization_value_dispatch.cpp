// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_internal_helpers.h"

#include "caors/reflection/registry.h"

#include <limits>
#include <string>
#include <string_view>

namespace caors::serialization::detail {

namespace {

template <typename T> [[nodiscard]] bool try_write_i64(writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_i64(static_cast<std::int64_t>(*stored)).succeeded();
}

template <typename T> [[nodiscard]] bool try_write_u64(writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_u64(static_cast<std::uint64_t>(*stored)).succeeded();
}

template <typename T> [[nodiscard]] bool try_write_f64(writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_f64(static_cast<double>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_read_signed_value(reader& reader, const any_ref& target) {
  std::int64_t value = 0;
  if (reader.read_i64(value).succeeded()) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return target.try_set<T>(static_cast<T>(value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).failed() ||
      unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return target.try_set<T>(static_cast<T>(unsigned_value));
}

template <typename T>
[[nodiscard]] bool try_read_unsigned_value(reader& reader, const any_ref& target) {
  std::uint64_t value = 0;
  if (reader.read_u64(value).succeeded()) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return target.try_set<T>(static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).failed() || signed_value < 0 ||
      static_cast<std::uint64_t>(signed_value) >
          static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return target.try_set<T>(static_cast<T>(signed_value));
}

template <typename T>
[[nodiscard]] bool try_read_float_value(reader& reader, const any_ref& target) {
  double value = 0.0;
  if (reader.read_f64(value).succeeded()) {
    return target.try_set<T>(static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return target.try_set<T>(static_cast<T>(signed_value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return target.try_set<T>(static_cast<T>(unsigned_value));
  }

  return false;
}

[[nodiscard]] bool try_read_char_value(reader& reader, const any_ref& target) {
  core::string value;
  if (reader.read_string(value).failed() || value.size() != 1) {
    return false;
  }

  return target.try_set<char>(value[0]);
}

[[nodiscard]] bool try_read_string_view_value(reader& reader, const any_ref& target,
                                              const serialization::context& context) {
  if (context.string_view_storage == nullptr || !target.matches_type<std::string_view>()) {
    return false;
  }

  core::string value;
  if (reader.read_string(value).failed()) {
    return false;
  }

  *context.string_view_storage = std::move(value);
  return target.try_set<std::string_view>(
      std::string_view{context.string_view_storage->data(), context.string_view_storage->size()});
}

} // namespace

bool try_write_serialized_value(writer& writer, const any& data) {
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
  if (!data.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const meta::type_semantics_data* semantics =
      meta::get_registry().find_ptr<meta::type_semantics_data>(data.type());
  if (semantics != nullptr &&
      (semantics->kind == meta::type_kind::raw_pointer ||
       semantics->kind == meta::type_kind::wrapper ||
       semantics->kind == meta::type_kind::enumeration ||
       semantics->kind == meta::type_kind::sequence || semantics->kind == meta::type_kind::set ||
       semantics->kind == meta::type_kind::associative ||
       semantics->kind == meta::type_kind::pair || semantics->kind == meta::type_kind::tuple ||
       semantics->kind == meta::type_kind::variant)) {
    return make_serialized_value(data.ref(), serialization::context{});
  }

  const std::optional<serialization::codec_data> codec =
      serialization::get_codec_registry().find_codec(data.type());
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

variant_result make_serialized_value(const any_ref& value, const serialization::context& context) {
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (type_uses_inline_indirect_value(value.type())) {
    const indirect_view indirect = value.create_indirect_view();
    return make_indirect_serialized_value(indirect, context);
  }

  if (type_uses_object_ref(value.type())) {
    return make_object_ref_value(value, context);
  }

  if (type_is_enumeration(value.type())) {
    return make_enum_value(value);
  }

  sequence_view sequence = value.create_sequence_view();
  if (sequence.valid()) {
    return make_sequence_value(sequence, context);
  }

  set_view set = value.create_set_view();
  if (set.valid()) {
    return make_set_value(set, context);
  }

  associative_view associative = value.create_associative_view();
  if (associative.valid()) {
    return make_associative_value(associative, context);
  }

  pair_view pair = value.create_pair_view();
  if (pair.valid()) {
    return make_pair_value(pair, context);
  }

  tuple_view tuple = value.create_tuple_view();
  if (tuple.valid()) {
    return make_tuple_value(tuple, context);
  }

  variant_view variant = value.create_variant_view();
  if (variant.valid()) {
    return make_variant_value(variant, context);
  }

  const std::optional<serialization::codec_data> codec =
      serialization::get_codec_registry().find_codec(value.type());
  if (codec.has_value() && codec->make_value != nullptr) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return codec->make_value(copied);
  }

  const std::optional<reflection::type> nested_type = reflection::find(value.type());
  if (nested_type.has_value() && serialization::contains(value.type())) {
    return to_serialization_value(*nested_type, value.data(), context);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_serialized_value(copied);
}

error write_serialized_value(writer& writer, const any_ref& value,
                             const serialization::context& context) {
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (type_uses_inline_indirect_value(value.type())) {
    const indirect_view indirect = value.create_indirect_view();
    return write_indirect_serialized_value(writer, indirect, context);
  }

  if (type_uses_object_ref(value.type())) {
    return write_object_ref_value(writer, value, context);
  }

  if (type_is_enumeration(value.type())) {
    return write_enum_value(writer, value);
  }

  sequence_view sequence = value.create_sequence_view();
  if (sequence.valid()) {
    return write_sequence_value(writer, sequence, context);
  }

  set_view set = value.create_set_view();
  if (set.valid()) {
    return write_set_value(writer, set, context);
  }

  associative_view associative = value.create_associative_view();
  if (associative.valid()) {
    return write_associative_value(writer, associative, context);
  }

  pair_view pair = value.create_pair_view();
  if (pair.valid()) {
    return write_pair_value(writer, pair, context);
  }

  tuple_view tuple = value.create_tuple_view();
  if (tuple.valid()) {
    return write_tuple_value(writer, tuple, context);
  }

  variant_view variant = value.create_variant_view();
  if (variant.valid()) {
    return write_variant_value(writer, variant, context);
  }

  const std::optional<serialization::codec_data> codec =
      serialization::get_codec_registry().find_codec(value.type());
  if (codec.has_value() && codec->write != nullptr) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return codec->write(writer, copied);
  }

  const std::optional<reflection::type> nested_type = reflection::find(value.type());
  if (nested_type.has_value() && serialization::contains(value.type())) {
    return serialize(*nested_type, value.data(), writer, context);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!try_write_serialized_value(writer, copied)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

error read_serialized_value(reader& reader, const any_ref& target,
                            const serialization::context& context) {
  if (!target.valid() || !target.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (type_uses_inline_indirect_value(target.type())) {
    const indirect_view indirect = target.create_indirect_view();
    return read_indirect_serialized_value(reader, indirect, context);
  }

  if (type_uses_object_ref(target.type())) {
    return read_object_ref_value(reader, target, context);
  }

  if (type_is_enumeration(target.type())) {
    return read_enum_value(reader, target);
  }

  if (target.create_sequence_view().valid()) {
    return read_sequence_value(reader, target, context);
  }

  if (target.create_set_view().valid()) {
    return read_set_value(reader, target, context);
  }

  if (target.create_associative_view().valid()) {
    return read_associative_value(reader, target, context);
  }

  if (target.create_pair_view().valid()) {
    return read_pair_value(reader, target, context);
  }

  if (target.create_tuple_view().valid()) {
    return read_tuple_value(reader, target, context);
  }

  if (target.create_variant_view().valid()) {
    return read_variant_value(reader, target, context);
  }

  const std::optional<serialization::codec_data> codec =
      serialization::get_codec_registry().find_codec(target.type());
  if (codec.has_value() && codec->read_value != nullptr) {
    return codec->read_value(reader, target);
  }

  const std::optional<reflection::type> nested_type = reflection::find(target.type());
  if (nested_type.has_value() && serialization::contains(target.type())) {
    return deserialize(*nested_type, target.mutable_data(), reader, context);
  }

  bool bool_value = false;
  bool written = reader.read_bool(bool_value).succeeded() && target.try_set<bool>(bool_value);

  if (!written) {
    written = try_read_char_value(reader, target);
  }

  if (!written) {
    written = try_read_signed_value<signed char>(reader, target) ||
              try_read_signed_value<short>(reader, target) ||
              try_read_signed_value<int>(reader, target) ||
              try_read_signed_value<long>(reader, target) ||
              try_read_signed_value<long long>(reader, target);
  }

  if (!written) {
    written = try_read_unsigned_value<unsigned char>(reader, target) ||
              try_read_unsigned_value<unsigned short>(reader, target) ||
              try_read_unsigned_value<unsigned int>(reader, target) ||
              try_read_unsigned_value<unsigned long>(reader, target) ||
              try_read_unsigned_value<unsigned long long>(reader, target);
  }

  if (!written) {
    written = try_read_float_value<float>(reader, target) ||
              try_read_float_value<double>(reader, target) ||
              try_read_float_value<long double>(reader, target);
  }

  if (!written) {
    written = try_read_string_view_value(reader, target, context);
  }

  if (!written) {
    core::string string_value;
    if (reader.read_string(string_value).succeeded()) {
      const std::string standard_string{string_value.data(), string_value.size()};
      written = target.try_set<core::string>(string_value) ||
                target.try_set<std::string>(standard_string);
    }
  }

  if (!written) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

error write_field(writer& writer, const reflection::field& field, const void* object,
                  const serialization::field_data& field_data,
                  const serialization::context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.wrapper_semantics() == meta::wrapper_semantics::object_ref) {
    return write_object_ref_field(writer, field, object, field_data, context);
  }

  const bool uses_indirect_value = field_uses_inline_indirect_value(field);
  any_ref field_value{};
  if (field_uses_direct_value(field)) {
    field_value = field.get_ref(object);
  } else if (!uses_indirect_value) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!uses_indirect_value && !field_value.valid()) {
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

  if (uses_indirect_value) {
    status = write_indirect_serialized_value(writer, field.create_indirect_view(object), context);
  } else {
    status = write_serialized_value(writer, field_value, context);
  }

  if (status) {
    return status;
  }

  return writer.end_field();
}

error read_field(reader& reader, const reflection::field& field, void* object,
                 const serialization::field_data& field_data,
                 const serialization::context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (!field.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.wrapper_semantics() == meta::wrapper_semantics::object_ref) {
    return read_object_ref_field(reader, field, object, field_data, context);
  }

  error status = validate_field_for_reader(field_data, reader.features());
  if (status) {
    return status;
  }

  const bool uses_indirect_value = field_uses_inline_indirect_value(field);
  any_ref target{};
  bool use_legacy_codec_read = false;
  if (field_uses_direct_value(field)) {
    const std::optional<serialization::codec_data> codec =
        serialization::get_codec_registry().find_codec(field.type());
    use_legacy_codec_read =
        codec.has_value() && codec->read != nullptr && codec->read_value == nullptr;
    target = field.get_ref(object);
  } else if (!uses_indirect_value) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!uses_indirect_value && !use_legacy_codec_read &&
      (!target.valid() || !target.writable() || target.mutable_data() == nullptr)) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = reader.begin_field(field_data);
  if (status) {
    return status;
  }

  if (uses_indirect_value) {
    status = read_indirect_serialized_value(reader, field.create_indirect_view(object), context);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  if (use_legacy_codec_read) {
    const std::optional<serialization::codec_data> codec =
        serialization::get_codec_registry().find_codec(field.type());
    if (!codec.has_value() || codec->read == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = codec->read(reader, field, object);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  status = read_serialized_value(reader, target, context);
  if (status) {
    return status;
  }

  return reader.end_field();
}

error read_configured_field(reader& reader, const reflection::field& field, void* object,
                            const serialization::field_data& field_data,
                            const serialization::context& context) {
  const error status = read_field(reader, field, object, field_data, context);
  if (status.value() == errc::not_found && !field_data.required) {
    return {};
  }

  return status;
}

} // namespace caors::serialization::detail
