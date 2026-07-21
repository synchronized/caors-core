// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

#include "caors/meta/detail/property_data.h"
#include "caors/meta/query.h"
#include "caors/meta/type.h"
#include "caors/meta/value.h"

namespace caors::serialization::detail {

namespace {

[[nodiscard]] bool codec_can_make_value(const serialization::codec_ops* ops) noexcept {
  return ops != nullptr && (ops->make_value != nullptr || ops->make_value_ref != nullptr);
}

[[nodiscard]] bool codec_can_write(const serialization::codec_ops* ops) noexcept {
  return ops != nullptr && (ops->write != nullptr || ops->write_ref != nullptr);
}

[[nodiscard]] bool codec_can_read(const serialization::codec_ops* ops) noexcept {
  return ops != nullptr && ops->read != nullptr;
}

[[nodiscard]] variant_result make_value_with_codec_ops(const serialization::codec_ops* ops,
                                                       const serialization::context& context,
                                                       const any_ref& value) {
  if (!codec_can_make_value(ops)) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (ops->make_value_ref != nullptr) {
    return ops->make_value_ref(context, value);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return ops->make_value(context, copied);
}

[[nodiscard]] error write_with_codec_ops(const serialization::codec_ops* ops,
                                         const serialization::context& context, writer& writer,
                                         const any_ref& value) {
  if (!codec_can_write(ops)) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (ops->write_ref != nullptr) {
    return ops->write_ref(context, writer, value);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  return ops->write(context, writer, copied);
}

[[nodiscard]] error read_with_codec_ops(const serialization::codec_ops* ops,
                                        const serialization::context& context, reader& reader,
                                        const any_ref& target) {
  return codec_can_read(ops) ? ops->read(context, reader, target)
                             : error{error_domain::serialization, errc::unsupported};
}

[[nodiscard]] const serialization::codec_ops*
cached_codec_ops(const serialization::field_schema* field, const meta::type& type) noexcept {
  return field != nullptr ? field->codec_operations : type.get_raw_ops<serialization::codec_ops>();
}

[[nodiscard]] const meta::scalar_ops* cached_scalar_ops(const serialization::field_schema* field,
                                                        const meta::type& type) noexcept {
  return field != nullptr ? field->scalar_operations : type.get_raw_ops<meta::scalar_ops>();
}

[[nodiscard]] variant_result
make_value_or_object_serialized_value(const any_ref& value, const serialization::context& context,
                                      const serialization::field_schema* field) {
  const meta::type type = value.get_type();
  const serialization::codec_ops* codec = cached_codec_ops(field, type);
  if (codec_can_make_value(codec)) {
    return make_value_with_codec_ops(codec, context, value);
  }

  const meta::scalar_ops* scalar = cached_scalar_ops(field, type);
  if (scalar != nullptr) {
    return make_scalar_value(value, scalar);
  }

  const type_id value_type = type.id();
  const meta::type nested_type = meta::find(value_type);
  if (serialization::contains(nested_type)) {
    return to_serialization_value(context, nested_type, value.data());
  }

  return make_scalar_value(value);
}

[[nodiscard]] error
write_value_or_object_serialized_value(writer& writer, const any_ref& value,
                                       const serialization::context& context,
                                       const serialization::field_schema* field) {
  const meta::type type = value.get_type();
  const serialization::codec_ops* codec = cached_codec_ops(field, type);
  if (codec_can_write(codec)) {
    return write_with_codec_ops(codec, context, writer, value);
  }

  const meta::scalar_ops* scalar = cached_scalar_ops(field, type);
  if (scalar != nullptr) {
    return try_write_scalar_value(writer, value, scalar)
               ? error{}
               : error{error_domain::serialization, errc::unsupported};
  }

  const type_id value_type = type.id();
  const meta::type nested_type = meta::find(value_type);
  if (serialization::contains(nested_type)) {
    return serialize(context, nested_type, value.data(), writer);
  }

  if (!try_write_scalar_value(writer, value)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

[[nodiscard]] error
read_value_or_object_serialized_value(reader& reader, const any_ref& target,
                                      const serialization::context& context,
                                      const serialization::field_schema* field) {
  const meta::type type = target.get_type();
  const serialization::codec_ops* codec = cached_codec_ops(field, type);
  if (codec_can_read(codec)) {
    return read_with_codec_ops(codec, context, reader, target);
  }

  const meta::scalar_ops* scalar = cached_scalar_ops(field, type);
  if (scalar != nullptr) {
    return read_scalar_value(reader, target, context, scalar);
  }

  const type_id target_type = type.id();
  const meta::type nested_type = meta::find(target_type);
  if (serialization::contains(nested_type)) {
    return deserialize(context, nested_type, target.mutable_data(), reader);
  }

  return read_scalar_value(reader, target, context);
}

} // namespace

any_ref get_field_ref(const serialization::field_schema& field_schema, const void* object) {
  if (field_schema.property_data != nullptr) {
    return field_schema.property_data->get_ref_unchecked(field_schema.declared_type, object);
  }

  const meta::instance instance{meta::any_ref::from_raw(field_schema.owner_type, object, false)};
  return field_schema.property.get_ref(instance);
}

any_ref get_field_ref(const serialization::field_schema& field_schema, void* object) {
  if (field_schema.property_data != nullptr) {
    return field_schema.property_data->get_ref_unchecked(field_schema.declared_type, object);
  }

  const meta::instance instance{meta::any_ref::from_raw(field_schema.owner_type, object, true)};
  return field_schema.property.get_ref(instance);
}

variant_result make_serialized_value(const any& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_serialized_value(data.ref(), serialization::context{});
}

variant_result make_serialized_value(const any_ref& value, const serialization::context& context) {
  return make_serialized_value(value, context, nullptr);
}

variant_result make_serialized_value(const any_ref& value, const serialization::context& context,
                                     const serialization::field_schema* field) {
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  switch (value.get_type().kind()) {
  case meta::type_kind::value:
    return make_value_or_object_serialized_value(value, context, field);

  case meta::type_kind::enumeration:
    return make_enum_value(value);

  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
    if (!type_uses_inline_indirect_value(value)) {
      return make_value_or_object_serialized_value(value, context, field);
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
  return write_serialized_value(writer, value, context, nullptr);
}

error write_serialized_value(writer& writer, const any_ref& value,
                             const serialization::context& context,
                             const serialization::field_schema* field) {
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  switch (value.get_type().kind()) {
  case meta::type_kind::value:
    return write_value_or_object_serialized_value(writer, value, context, field);

  case meta::type_kind::enumeration:
    return write_enum_value(writer, value);

  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
    if (!type_uses_inline_indirect_value(value)) {
      return write_value_or_object_serialized_value(writer, value, context, field);
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
  return read_serialized_value(reader, target, context, nullptr);
}

error read_serialized_value(reader& reader, const any_ref& target,
                            const serialization::context& context,
                            const serialization::field_schema* field) {
  if (!target.valid() || !target.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  switch (target.get_type().kind()) {
  case meta::type_kind::value:
    return read_value_or_object_serialized_value(reader, target, context, field);

  case meta::type_kind::enumeration:
    return read_enum_value(reader, target);

  case meta::type_kind::raw_pointer:
  case meta::type_kind::wrapper:
    if (!type_uses_inline_indirect_value(target)) {
      return read_value_or_object_serialized_value(reader, target, context, field);
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

error write_field(writer& writer, const void* object,
                  const serialization::field_schema& field_schema,
                  const serialization::context& context) {
  const meta::property& field = field_schema.property;
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  const bool uses_indirect_value = field_schema.uses_inline_indirect_value;
  any_ref field_value{};
  if (field_schema.uses_direct_value) {
    field_value = get_field_ref(field_schema, object);
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
        writer, get_field_ref(field_schema, object).get_capability<meta::indirect_capability>(),
        context);
  } else {
    status = write_serialized_value(writer, field_value, context, &field_schema);
  }

  if (status) {
    return status;
  }

  return writer.end_field();
}

error read_field(reader& reader, void* object, const serialization::field_schema& field_schema,
                 const serialization::context& context) {
  const meta::property& field = field_schema.property;
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

  const bool uses_indirect_value = field_schema.uses_inline_indirect_value;
  any_ref target{};
  if (field_schema.uses_direct_value) {
    target = get_field_ref(field_schema, object);
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

  if (uses_indirect_value) {
    status = read_indirect_serialized_value(
        reader, get_field_ref(field_schema, object).get_capability<meta::indirect_capability>(),
        context);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  status = read_serialized_value(reader, target, context, &field_schema);
  if (status) {
    return status;
  }

  return reader.end_field();
}

error read_configured_field(reader& reader, void* object,
                            const serialization::field_schema& field_schema,
                            const serialization::context& context) {
  const error status = read_field(reader, object, field_schema, context);
  if (status.value() == errc::not_found && !field_schema.required) {
    return {};
  }

  return status;
}

} // namespace caors::serialization::detail
