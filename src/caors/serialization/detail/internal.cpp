// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

#include "caors/meta/detail/type_capabilities.h"
#include "caors/object/ref_view.h"
#include "caors/reflection/registry.h"

#include <limits>

namespace caors::serialization::detail {

[[nodiscard]] error read_size_value(reader& reader, std::size_t& value) {
  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    value = static_cast<std::size_t>(unsigned_value);
    return {};
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).failed() || signed_value < 0 ||
      static_cast<std::uint64_t>(signed_value) >
          static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  value = static_cast<std::size_t>(signed_value);
  return {};
}

[[nodiscard]] bool field_uses_inline_indirect_value(const reflection::field& field) noexcept {
  return (field.kind() == meta::type_kind::raw_pointer ||
          field.kind() == meta::type_kind::wrapper) &&
         field.wrapper_semantics() == meta::wrapper_semantics::inline_value;
}

[[nodiscard]] bool field_uses_direct_value(const reflection::field& field) noexcept {
  return field.kind() == meta::type_kind::value || field.kind() == meta::type_kind::enumeration ||
         field.kind() == meta::type_kind::sequence || field.kind() == meta::type_kind::set ||
         field.kind() == meta::type_kind::associative || field.kind() == meta::type_kind::pair ||
         field.kind() == meta::type_kind::tuple || field.kind() == meta::type_kind::variant;
}

[[nodiscard]] const meta::detail::type_descriptor*
descriptor_of_value(const any_ref& value) noexcept {
  return caors::value::detail::descriptor_of(value);
}

[[nodiscard]] const meta::detail::enum_data* enum_data(const any_ref& value) noexcept {
  const caors::meta::detail::type_descriptor* descriptor = descriptor_of_value(value);
  return meta::detail::descriptor_capability<meta::detail::enum_data>(descriptor);
}

[[nodiscard]] bool type_uses_inline_indirect_value(const any_ref& value) noexcept {
  const meta::detail::type_descriptor* descriptor = descriptor_of_value(value);
  return descriptor != nullptr &&
         (meta::detail::descriptor_kind(*descriptor) == meta::type_kind::raw_pointer ||
          meta::detail::descriptor_kind(*descriptor) == meta::type_kind::wrapper) &&
         meta::detail::descriptor_wrapper_semantics(*descriptor) ==
             meta::wrapper_semantics::inline_value;
}

[[nodiscard]] bool type_is_enumeration(const any_ref& value) noexcept {
  const meta::detail::type_descriptor* descriptor = descriptor_of_value(value);
  return descriptor != nullptr &&
         meta::detail::descriptor_kind(*descriptor) == meta::type_kind::enumeration;
}

[[nodiscard]] bool type_uses_object_ref(const any_ref& value) noexcept {
  return objects::is_object_ref(value);
}

[[nodiscard]] variant_result make_object_ref_value(const any_ref& value,
                                                   const serialization::context& context) {
  const objects::object_ref_view object_ref = objects::object_ref(value);
  if (!object_ref) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  const object* referenced = object_ref.get();
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return {{}, variant::from(id.value())};
}

[[nodiscard]] error write_object_ref_value(writer& writer, const any_ref& value,
                                           const serialization::context& context) {
  const objects::object_ref_view object_ref = objects::object_ref(value);
  if (!object_ref) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const object* referenced = object_ref.get();
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {error_domain::serialization, errc::not_found};
  }

  return writer.write_u64(id.value());
}

[[nodiscard]] error read_object_ref_value(reader& reader, const any_ref& target,
                                          const serialization::context& context) {
  const objects::object_ref_view object_ref = objects::object_ref(target);
  if (!object_ref || !object_ref.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (context.reference_resolver == nullptr || !context.reference_resolver->can_read()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  std::uint64_t encoded_id = 0;
  error status = reader.read_u64(encoded_id);
  if (status) {
    return status;
  }

  object* referenced = nullptr;
  if (encoded_id != 0) {
    referenced = context.reference_resolver->id_to_object(object_id{encoded_id},
                                                          context.reference_resolver->user_data);
    if (referenced == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }
  }

  if (!object_ref.set(referenced)) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return {};
}

[[nodiscard]] variant_result make_enum_value(const any_ref& value) {
  const meta::detail::type_descriptor* descriptor = descriptor_of_value(value);
  const meta::detail::enum_data* data = enum_data(value);
  if (descriptor == nullptr ||
      meta::detail::descriptor_kind(*descriptor) != meta::type_kind::enumeration ||
      data == nullptr || value.data() == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (data->is_signed) {
    if (data->to_i64 == nullptr) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return {{}, variant::from(data->to_i64(value.data()))};
  }

  if (data->to_u64 == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, variant::from(data->to_u64(value.data()))};
}

[[nodiscard]] error write_enum_value(writer& writer, const any_ref& value) {
  const meta::detail::type_descriptor* descriptor = descriptor_of_value(value);
  const meta::detail::enum_data* data = enum_data(value);
  if (descriptor == nullptr ||
      meta::detail::descriptor_kind(*descriptor) != meta::type_kind::enumeration ||
      data == nullptr || value.data() == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (data->is_signed) {
    if (data->to_i64 == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    return writer.write_i64(data->to_i64(value.data()));
  }

  if (data->to_u64 == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return writer.write_u64(data->to_u64(value.data()));
}

[[nodiscard]] error read_enum_value(reader& reader, const any_ref& target) {
  const meta::detail::type_descriptor* descriptor = descriptor_of_value(target);
  const meta::detail::enum_data* data = enum_data(target);
  if (descriptor == nullptr ||
      meta::detail::descriptor_kind(*descriptor) != meta::type_kind::enumeration ||
      data == nullptr || target.mutable_data() == nullptr || data->from_i64 == nullptr ||
      data->from_u64 == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (data->is_signed) {
    std::int64_t signed_value = 0;
    if (reader.read_i64(signed_value).succeeded()) {
      return data->from_i64(target.mutable_data(), signed_value)
                 ? error{}
                 : error{error_domain::serialization, errc::invalid_argument};
    }

    std::uint64_t unsigned_value = 0;
    if (reader.read_u64(unsigned_value).succeeded()) {
      return data->from_u64(target.mutable_data(), unsigned_value)
                 ? error{}
                 : error{error_domain::serialization, errc::invalid_argument};
    }

    return {error_domain::serialization, errc::unsupported};
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return data->from_u64(target.mutable_data(), unsigned_value)
               ? error{}
               : error{error_domain::serialization, errc::invalid_argument};
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return data->from_i64(target.mutable_data(), signed_value)
               ? error{}
               : error{error_domain::serialization, errc::invalid_argument};
  }

  return {error_domain::serialization, errc::unsupported};
}

[[nodiscard]] variant_result make_indirect_serialized_value(const value::indirect_view& indirect,
                                                            const serialization::context& context) {
  if (!indirect.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (!indirect.has_value()) {
    return {{}, variant::null()};
  }

  const any_ref value = indirect.value_ref();
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_serialized_value(value, context);
}

[[nodiscard]] error write_indirect_serialized_value(writer& writer,
                                                    const value::indirect_view& indirect,
                                                    const serialization::context& context) {
  if (!indirect.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!indirect.has_value()) {
    return writer.write_null();
  }

  const any_ref value = indirect.value_ref();
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  return write_serialized_value(writer, value, context);
}

[[nodiscard]] error read_indirect_serialized_value(reader& reader,
                                                   const value::indirect_view& indirect,
                                                   const serialization::context& context) {
  if (!indirect.valid() || !indirect.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (reader.is_null()) {
    if (!indirect.reset()) {
      return {error_domain::serialization, errc::invalid_field};
    }

    return {};
  }

  if (!indirect.has_value() && !indirect.ensure_value()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  any_ref value = indirect.value_ref();
  if (!value.valid() || !value.writable() || value.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return read_serialized_value(reader, value, context);
}

} // namespace caors::serialization::detail
