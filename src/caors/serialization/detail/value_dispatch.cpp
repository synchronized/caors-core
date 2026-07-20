// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

#include "caors/meta/query.h"
#include "caors/meta/type.h"
#include "caors/meta/value.h"

namespace caors::serialization::detail {

namespace {

[[nodiscard]] serialization::codec_capability find_codec_capability(const meta::type& type) {
  return type.get_capability<serialization::codec_capability>();
}

[[nodiscard]] variant_result
make_value_or_object_serialized_value(const any_ref& value, const serialization::context& context) {
  const meta::type type = value.get_type();
  const serialization::codec_capability codec = find_codec_capability(type);
  if (codec.can_make_value()) {
    return codec.make_value(context, value);
  }

  const type_id value_type = type.id();
  const meta::type nested_type = meta::find(value_type);
  if (nested_type && serialization::contains(value_type)) {
    return to_serialization_value(context, nested_type, value.data());
  }

  return make_scalar_value(value);
}

[[nodiscard]] error write_value_or_object_serialized_value(writer& writer, const any_ref& value,
                                                           const serialization::context& context) {
  const meta::type type = value.get_type();
  const serialization::codec_capability codec = find_codec_capability(type);
  if (codec.can_write()) {
    return codec.write(context, writer, value);
  }

  const type_id value_type = type.id();
  const meta::type nested_type = meta::find(value_type);
  if (nested_type && serialization::contains(value_type)) {
    return serialize(context, nested_type, value.data(), writer);
  }

  if (!try_write_scalar_value(writer, value)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

[[nodiscard]] error read_value_or_object_serialized_value(reader& reader, const any_ref& target,
                                                          const serialization::context& context) {
  const meta::type type = target.get_type();
  const serialization::codec_capability codec = find_codec_capability(type);
  if (codec.can_read()) {
    return codec.read(context, reader, target);
  }

  const type_id target_type = type.id();
  const meta::type nested_type = meta::find(target_type);
  if (nested_type && serialization::contains(target_type)) {
    return deserialize(context, nested_type, target.mutable_data(), reader);
  }

  return read_scalar_value(reader, target, context);
}

} // namespace

variant_result make_serialized_value(const any& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_serialized_value(data.ref(), serialization::context{});
}

variant_result make_serialized_value(const any_ref& value, const serialization::context& context) {
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  switch (value.get_type().kind()) {
  case meta::type_kind::value:
    return make_value_or_object_serialized_value(value, context);

  case meta::type_kind::enumeration:
    return make_enum_value(value);

  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
    if (!type_uses_inline_indirect_value(value)) {
      return make_value_or_object_serialized_value(value, context);
    }

    {
      const meta::indirect_view indirect = value.get_capability<meta::indirect_capability>();
      return make_indirect_serialized_value(indirect, context);
    }

  case meta::type_kind::sequence: {
    meta::sequence_view sequence = value.get_capability<meta::sequence_capability>();
    return sequence.valid() ? make_sequence_value(sequence, context)
                            : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  case meta::type_kind::set: {
    meta::set_view set = value.get_capability<meta::set_capability>();
    return set.valid() ? make_set_value(set, context)
                       : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  case meta::type_kind::associative: {
    meta::associative_view associative = value.get_capability<meta::associative_capability>();
    return associative.valid()
               ? make_associative_value(associative, context)
               : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  case meta::type_kind::pair: {
    meta::pair_view pair = value.get_capability<meta::pair_capability>();
    return pair.valid() ? make_pair_value(pair, context)
                        : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  case meta::type_kind::tuple: {
    meta::tuple_view tuple = value.get_capability<meta::tuple_capability>();
    return tuple.valid() ? make_tuple_value(tuple, context)
                         : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  case meta::type_kind::variant: {
    meta::variant_view variant = value.get_capability<meta::variant_capability>();
    return variant.valid() ? make_variant_value(variant, context)
                           : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

error write_serialized_value(writer& writer, const any_ref& value,
                             const serialization::context& context) {
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  switch (value.get_type().kind()) {
  case meta::type_kind::value:
    return write_value_or_object_serialized_value(writer, value, context);

  case meta::type_kind::enumeration:
    return write_enum_value(writer, value);

  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
    if (!type_uses_inline_indirect_value(value)) {
      return write_value_or_object_serialized_value(writer, value, context);
    }

    {
      const meta::indirect_view indirect = value.get_capability<meta::indirect_capability>();
      return write_indirect_serialized_value(writer, indirect, context);
    }

  case meta::type_kind::sequence: {
    meta::sequence_view sequence = value.get_capability<meta::sequence_capability>();
    return sequence.valid() ? write_sequence_value(writer, sequence, context)
                            : error{error_domain::serialization, errc::unsupported};
  }

  case meta::type_kind::set: {
    meta::set_view set = value.get_capability<meta::set_capability>();
    return set.valid() ? write_set_value(writer, set, context)
                       : error{error_domain::serialization, errc::unsupported};
  }

  case meta::type_kind::associative: {
    meta::associative_view associative = value.get_capability<meta::associative_capability>();
    return associative.valid() ? write_associative_value(writer, associative, context)
                               : error{error_domain::serialization, errc::unsupported};
  }

  case meta::type_kind::pair: {
    meta::pair_view pair = value.get_capability<meta::pair_capability>();
    return pair.valid() ? write_pair_value(writer, pair, context)
                        : error{error_domain::serialization, errc::unsupported};
  }

  case meta::type_kind::tuple: {
    meta::tuple_view tuple = value.get_capability<meta::tuple_capability>();
    return tuple.valid() ? write_tuple_value(writer, tuple, context)
                         : error{error_domain::serialization, errc::unsupported};
  }

  case meta::type_kind::variant: {
    meta::variant_view variant = value.get_capability<meta::variant_capability>();
    return variant.valid() ? write_variant_value(writer, variant, context)
                           : error{error_domain::serialization, errc::unsupported};
  }
  }

  return {error_domain::serialization, errc::unsupported};
}

error read_serialized_value(reader& reader, const any_ref& target,
                            const serialization::context& context) {
  if (!target.valid() || !target.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  switch (target.get_type().kind()) {
  case meta::type_kind::value:
    return read_value_or_object_serialized_value(reader, target, context);

  case meta::type_kind::enumeration:
    return read_enum_value(reader, target);

  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
    if (!type_uses_inline_indirect_value(target)) {
      return read_value_or_object_serialized_value(reader, target, context);
    }

    {
      const meta::indirect_view indirect = target.get_capability<meta::indirect_capability>();
      return read_indirect_serialized_value(reader, indirect, context);
    }

  case meta::type_kind::sequence:
    return target.get_capability<meta::sequence_capability>().valid()
               ? read_sequence_value(reader, target, context)
               : error{error_domain::serialization, errc::unsupported};

  case meta::type_kind::set:
    return target.get_capability<meta::set_capability>().valid()
               ? read_set_value(reader, target, context)
               : error{error_domain::serialization, errc::unsupported};

  case meta::type_kind::associative:
    return target.get_capability<meta::associative_capability>().valid()
               ? read_associative_value(reader, target, context)
               : error{error_domain::serialization, errc::unsupported};

  case meta::type_kind::pair:
    return target.get_capability<meta::pair_capability>().valid()
               ? read_pair_value(reader, target, context)
               : error{error_domain::serialization, errc::unsupported};

  case meta::type_kind::tuple:
    return target.get_capability<meta::tuple_capability>().valid()
               ? read_tuple_value(reader, target, context)
               : error{error_domain::serialization, errc::unsupported};

  case meta::type_kind::variant:
    return target.get_capability<meta::variant_capability>().valid()
               ? read_variant_value(reader, target, context)
               : error{error_domain::serialization, errc::unsupported};
  }

  return {error_domain::serialization, errc::unsupported};
}

error write_field(writer& writer, const meta::property& field, const void* object,
                  const serialization::field_schema& field_schema,
                  const serialization::context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  const bool uses_indirect_value = field_uses_inline_indirect_value(field);
  const meta::instance instance{meta::any_ref::from_raw(field.get_owner_type(), object, false)};
  any_ref field_value{};
  if (field_uses_direct_value(field)) {
    field_value = field.get_ref(instance);
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
        writer, field.get_ref(instance).get_capability<meta::indirect_capability>(), context);
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
  const meta::instance instance{meta::any_ref::from_raw(field.get_owner_type(), object, true)};
  any_ref target{};
  if (field_uses_direct_value(field)) {
    target = field.get_ref(instance);
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
        reader, field.get_ref(instance).get_capability<meta::indirect_capability>(), field_context);
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
