// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

#include "caors/object/ref_view.h"
#include "caors/reflection/registry.h"

#include <limits>
#include <string>
#include <string_view>

namespace caors::serialization::detail {

variant_result make_serialized_value(const any& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const caors::meta::detail::type_descriptor* descriptor =
      caors::value::detail::descriptor_of(data);
  if (descriptor != nullptr) {
    const meta::type_kind kind = meta::detail::descriptor_kind(*descriptor);
    if (descriptor->object_ref != nullptr || kind == meta::type_kind::raw_pointer ||
        kind == meta::type_kind::wrapper || kind == meta::type_kind::enumeration ||
        kind == meta::type_kind::sequence || kind == meta::type_kind::set ||
        kind == meta::type_kind::associative || kind == meta::type_kind::pair ||
        kind == meta::type_kind::tuple || kind == meta::type_kind::variant) {
      return make_serialized_value(data.ref(), serialization::context{});
    }
  }

  const std::optional<codec_data> codec = get_codec_registry().find_codec(data.type());
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

  if (type_uses_object_ref(value)) {
    return make_object_ref_value(value, context);
  }

  if (type_uses_inline_indirect_value(value)) {
    const value::indirect_view indirect = value.create_indirect_view();
    return make_indirect_serialized_value(indirect, context);
  }

  if (type_is_enumeration(value)) {
    return make_enum_value(value);
  }

  value::sequence_view sequence = value.create_sequence_view();
  if (sequence.valid()) {
    return make_sequence_value(sequence, context);
  }

  value::set_view set = value.create_set_view();
  if (set.valid()) {
    return make_set_value(set, context);
  }

  value::associative_view associative = value.create_associative_view();
  if (associative.valid()) {
    return make_associative_value(associative, context);
  }

  value::pair_view pair = value.create_pair_view();
  if (pair.valid()) {
    return make_pair_value(pair, context);
  }

  value::tuple_view tuple = value.create_tuple_view();
  if (tuple.valid()) {
    return make_tuple_value(tuple, context);
  }

  value::variant_view variant = value.create_variant_view();
  if (variant.valid()) {
    return make_variant_value(variant, context);
  }

  const std::optional<codec_data> codec = get_codec_registry().find_codec(value.type());
  if (codec.has_value() && codec->make_value != nullptr) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return codec->make_value(copied);
  }

  const reflection::type nested_type = reflection::find(value.type());
  if (nested_type && serialization::contains(value.type())) {
    return to_serialization_value(nested_type, value.data(), context);
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

  if (type_uses_object_ref(value)) {
    return write_object_ref_value(writer, value, context);
  }

  if (type_uses_inline_indirect_value(value)) {
    const value::indirect_view indirect = value.create_indirect_view();
    return write_indirect_serialized_value(writer, indirect, context);
  }

  if (type_is_enumeration(value)) {
    return write_enum_value(writer, value);
  }

  value::sequence_view sequence = value.create_sequence_view();
  if (sequence.valid()) {
    return write_sequence_value(writer, sequence, context);
  }

  value::set_view set = value.create_set_view();
  if (set.valid()) {
    return write_set_value(writer, set, context);
  }

  value::associative_view associative = value.create_associative_view();
  if (associative.valid()) {
    return write_associative_value(writer, associative, context);
  }

  value::pair_view pair = value.create_pair_view();
  if (pair.valid()) {
    return write_pair_value(writer, pair, context);
  }

  value::tuple_view tuple = value.create_tuple_view();
  if (tuple.valid()) {
    return write_tuple_value(writer, tuple, context);
  }

  value::variant_view variant = value.create_variant_view();
  if (variant.valid()) {
    return write_variant_value(writer, variant, context);
  }

  const std::optional<codec_data> codec = get_codec_registry().find_codec(value.type());
  if (codec.has_value() && codec->write != nullptr) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return codec->write(writer, copied);
  }

  const reflection::type nested_type = reflection::find(value.type());
  if (nested_type && serialization::contains(value.type())) {
    return serialize(nested_type, value.data(), writer, context);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!try_write_scalar_value(writer, copied)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

error read_serialized_value(reader& reader, const any_ref& target,
                            const serialization::context& context) {
  if (!target.valid() || !target.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (type_uses_object_ref(target)) {
    return read_object_ref_value(reader, target, context);
  }

  if (type_uses_inline_indirect_value(target)) {
    const value::indirect_view indirect = target.create_indirect_view();
    return read_indirect_serialized_value(reader, indirect, context);
  }

  if (type_is_enumeration(target)) {
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

  const std::optional<codec_data> codec = get_codec_registry().find_codec(target.type());
  if (codec.has_value() && codec->read_value != nullptr) {
    return codec->read_value(reader, target);
  }

  const reflection::type nested_type = reflection::find(target.type());
  if (nested_type && serialization::contains(target.type())) {
    return deserialize(nested_type, target.mutable_data(), reader, context);
  }

  return read_scalar_value(reader, target, context);
}

error write_field(writer& writer, const reflection::field& field, const void* object,
                  const serialization::field_data& field_data,
                  const serialization::context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (objects::is_object_ref(field.get_ref(object))) {
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
    status = write_indirect_serialized_value(writer, field.get_ref(object).create_indirect_view(),
                                             context);
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

  if (objects::is_object_ref(field.get_ref(object))) {
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
    const std::optional<codec_data> codec = get_codec_registry().find_codec(field.type());
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
    status = read_indirect_serialized_value(reader, field.get_ref(object).create_indirect_view(),
                                            context);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  if (use_legacy_codec_read) {
    const std::optional<codec_data> codec = get_codec_registry().find_codec(field.type());
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
