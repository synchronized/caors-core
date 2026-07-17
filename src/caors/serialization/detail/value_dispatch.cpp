// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "helpers.h"

#include <limits>
#include <string>
#include <string_view>

namespace caors::serialization::detail {

namespace {

[[nodiscard]] serialization::codec_capability find_codec_capability(const meta::type& type) {
  return type.get_capability<serialization::codec_capability>();
}

} // namespace

variant_result make_serialized_value(const any& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const meta::type type = data.get_type();
  if (type) {
    const meta::type_kind kind = type.kind();
    const any_ref ref = data.ref();
    if (kind == meta::type_kind::raw_pointer || kind == meta::type_kind::wrapper ||
        kind == meta::type_kind::enumeration || kind == meta::type_kind::sequence ||
        kind == meta::type_kind::set || kind == meta::type_kind::associative ||
        kind == meta::type_kind::pair || kind == meta::type_kind::tuple ||
        kind == meta::type_kind::variant) {
      return make_serialized_value(ref, serialization::context{});
    }
  }

  const serialization::codec_capability codec = find_codec_capability(data.get_type());
  if (codec.can_make_value()) {
    return codec.make_value(serialization::context{}, data);
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

  if (type_uses_inline_indirect_value(value)) {
    const meta::indirect_view indirect = value.get_capability<meta::indirect_capability>();
    return make_indirect_serialized_value(indirect, context);
  }

  if (type_is_enumeration(value)) {
    return make_enum_value(value);
  }

  meta::sequence_view sequence = value.get_capability<meta::sequence_capability>();
  if (sequence.valid()) {
    return make_sequence_value(sequence, context);
  }

  meta::set_view set = value.get_capability<meta::set_capability>();
  if (set.valid()) {
    return make_set_value(set, context);
  }

  meta::associative_view associative = value.get_capability<meta::associative_capability>();
  if (associative.valid()) {
    return make_associative_value(associative, context);
  }

  meta::pair_view pair = value.get_capability<meta::pair_capability>();
  if (pair.valid()) {
    return make_pair_value(pair, context);
  }

  meta::tuple_view tuple = value.get_capability<meta::tuple_capability>();
  if (tuple.valid()) {
    return make_tuple_value(tuple, context);
  }

  meta::variant_view variant = value.get_capability<meta::variant_capability>();
  if (variant.valid()) {
    return make_variant_value(variant, context);
  }

  const type_id value_type = value.get_type().id();
  const serialization::codec_capability codec = find_codec_capability(value.get_type());
  if (codec.can_make_value()) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return codec.make_value(context, copied);
  }

  const meta::type nested_type = meta::find(value_type);
  if (nested_type && serialization::contains(value_type)) {
    return to_serialization_value(context, nested_type, value.data());
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

  if (type_uses_inline_indirect_value(value)) {
    const meta::indirect_view indirect = value.get_capability<meta::indirect_capability>();
    return write_indirect_serialized_value(writer, indirect, context);
  }

  if (type_is_enumeration(value)) {
    return write_enum_value(writer, value);
  }

  meta::sequence_view sequence = value.get_capability<meta::sequence_capability>();
  if (sequence.valid()) {
    return write_sequence_value(writer, sequence, context);
  }

  meta::set_view set = value.get_capability<meta::set_capability>();
  if (set.valid()) {
    return write_set_value(writer, set, context);
  }

  meta::associative_view associative = value.get_capability<meta::associative_capability>();
  if (associative.valid()) {
    return write_associative_value(writer, associative, context);
  }

  meta::pair_view pair = value.get_capability<meta::pair_capability>();
  if (pair.valid()) {
    return write_pair_value(writer, pair, context);
  }

  meta::tuple_view tuple = value.get_capability<meta::tuple_capability>();
  if (tuple.valid()) {
    return write_tuple_value(writer, tuple, context);
  }

  meta::variant_view variant = value.get_capability<meta::variant_capability>();
  if (variant.valid()) {
    return write_variant_value(writer, variant, context);
  }

  const type_id value_type = value.get_type().id();
  const serialization::codec_capability codec = find_codec_capability(value.get_type());
  if (codec.can_write()) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return codec.write(context, writer, copied);
  }

  const meta::type nested_type = meta::find(value_type);
  if (nested_type && serialization::contains(value_type)) {
    return serialize(context, nested_type, value.data(), writer);
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

  if (type_uses_inline_indirect_value(target)) {
    const meta::indirect_view indirect = target.get_capability<meta::indirect_capability>();
    return read_indirect_serialized_value(reader, indirect, context);
  }

  if (type_is_enumeration(target)) {
    return read_enum_value(reader, target);
  }

  if (target.get_capability<meta::sequence_capability>().valid()) {
    return read_sequence_value(reader, target, context);
  }

  if (target.get_capability<meta::set_capability>().valid()) {
    return read_set_value(reader, target, context);
  }

  if (target.get_capability<meta::associative_capability>().valid()) {
    return read_associative_value(reader, target, context);
  }

  if (target.get_capability<meta::pair_capability>().valid()) {
    return read_pair_value(reader, target, context);
  }

  if (target.get_capability<meta::tuple_capability>().valid()) {
    return read_tuple_value(reader, target, context);
  }

  if (target.get_capability<meta::variant_capability>().valid()) {
    return read_variant_value(reader, target, context);
  }

  const type_id target_type = target.get_type().id();
  const serialization::codec_capability codec = find_codec_capability(target.get_type());
  if (codec.can_read()) {
    return codec.read(context, reader, target);
  }

  const meta::type nested_type = meta::find(target_type);
  if (nested_type && serialization::contains(target_type)) {
    return deserialize(context, nested_type, target.mutable_data(), reader);
  }

  return read_scalar_value(reader, target, context);
}

error write_field(writer& writer, const meta::property& field, const void* object,
                  const serialization::field_schema& field_schema,
                  const serialization::context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
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

  error status = validate_field_for_writer(field_schema, writer.features());
  if (status) {
    return status;
  }

  status = writer.begin_field(field_schema);
  if (status) {
    return status;
  }

  if (uses_indirect_value) {
    status = write_indirect_serialized_value(
        writer, field.get_ref(object).get_capability<meta::indirect_capability>(), context);
  } else {
    status = write_serialized_value(writer, field_value, context);
  }

  if (status) {
    return status;
  }

  return writer.end_field();
}

error read_field(reader& reader, const meta::property& field, void* object,
                 const serialization::field_schema& field_schema,
                 const serialization::context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (!field.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  error status = validate_field_for_reader(field_schema, reader.features());
  if (status) {
    return status;
  }

  const bool uses_indirect_value = field_uses_inline_indirect_value(field);
  any_ref target{};
  if (field_uses_direct_value(field)) {
    target = field.get_ref(object);
  } else if (!uses_indirect_value) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!uses_indirect_value &&
      (!target.valid() || !target.writable() || target.mutable_data() == nullptr)) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = reader.begin_field(field_schema);
  if (status) {
    return status;
  }

  serialization::context field_context = context;
  field_context.state.construction.owner_type = field.get_owner_type();
  field_context.state.construction.field_name = field.name();
  field_context.state.construction.type = field.get_type();
  field_context.state.construction.site = construction_site::field;
  field_context.state.construction.index = construction_request_no_index;

  if (uses_indirect_value) {
    status = read_indirect_serialized_value(
        reader, field.get_ref(object).get_capability<meta::indirect_capability>(), field_context);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  status = read_serialized_value(reader, target, field_context);
  if (status) {
    return status;
  }

  return reader.end_field();
}

error read_configured_field(reader& reader, const meta::property& field, void* object,
                            const serialization::field_schema& field_schema,
                            const serialization::context& context) {
  const error status = read_field(reader, field, object, field_schema, context);
  if (status.value() == errc::not_found && !field_schema.required) {
    return {};
  }

  return status;
}

} // namespace caors::serialization::detail
