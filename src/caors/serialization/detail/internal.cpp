// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

#include "caors/meta/detail/type_capabilities.h"
#include "caors/object/ref_view.h"
#include "caors/reflection/registry.h"

#include <limits>
#include <optional>
#include <string>

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

[[nodiscard]] variant_result make_object_ref_identity_value(const object* referenced,
                                                            const serialization::context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  const reference_resolver& resolver = *context.reference_resolver;
  if (resolver.uses_persistent_guid()) {
    if (referenced == nullptr) {
      return {{}, variant::null()};
    }

    const object_guid guid = resolver.object_to_guid(referenced, resolver.user_data);
    if (!guid) {
      return {{error_domain::serialization, errc::not_found}, {}};
    }

    const std::string text = guid.string();
    return {{}, variant::from(std::string_view{text.data(), text.size()})};
  }

  const object_id id = referenced != nullptr ? resolver.object_to_id(referenced, resolver.user_data)
                                             : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return {{}, variant::from(id.value())};
}

[[nodiscard]] error write_object_ref_identity(writer& writer, const object* referenced,
                                              const serialization::context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const reference_resolver& resolver = *context.reference_resolver;
  if (resolver.uses_persistent_guid()) {
    if (referenced == nullptr) {
      return writer.write_null();
    }

    const object_guid guid = resolver.object_to_guid(referenced, resolver.user_data);
    if (!guid) {
      return {error_domain::serialization, errc::not_found};
    }

    const std::string text = guid.string();
    return writer.write_string(std::string_view{text.data(), text.size()});
  }

  const object_id id = referenced != nullptr ? resolver.object_to_id(referenced, resolver.user_data)
                                             : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {error_domain::serialization, errc::not_found};
  }

  return writer.write_u64(id.value());
}

[[nodiscard]] error read_object_ref_identity(reader& reader, const serialization::context& context,
                                             object*& referenced) {
  referenced = nullptr;
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_read()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const reference_resolver& resolver = *context.reference_resolver;
  if (resolver.uses_persistent_guid()) {
    if (reader.is_null()) {
      return {};
    }

    string encoded_guid{};
    error status = reader.read_string(encoded_guid);
    if (status) {
      return status;
    }

    const std::optional<object_guid> parsed =
        object_guid::parse(std::string_view{encoded_guid.data(), encoded_guid.size()});
    if (!parsed.has_value()) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    if (!*parsed) {
      return {};
    }

    referenced = resolver.guid_to_object(*parsed, resolver.user_data);
    if (referenced == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }

    return {};
  }

  std::uint64_t encoded_id = 0;
  error status = reader.read_u64(encoded_id);
  if (status) {
    return status;
  }

  if (encoded_id != 0) {
    referenced = resolver.id_to_object(object_id{encoded_id}, resolver.user_data);
    if (referenced == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }
  }

  return {};
}

[[nodiscard]] variant_result make_object_ref_value(const any_ref& value,
                                                   const serialization::context& context) {
  const objects::object_ref_view object_ref = objects::object_ref(value);
  if (!object_ref) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_object_ref_identity_value(object_ref.get(), context);
}

[[nodiscard]] error write_object_ref_value(writer& writer, const any_ref& value,
                                           const serialization::context& context) {
  const objects::object_ref_view object_ref = objects::object_ref(value);
  if (!object_ref) {
    return {error_domain::serialization, errc::unsupported};
  }

  return write_object_ref_identity(writer, object_ref.get(), context);
}

[[nodiscard]] error read_object_ref_value(reader& reader, const any_ref& target,
                                          const serialization::context& context) {
  const objects::object_ref_view object_ref = objects::object_ref(target);
  if (!object_ref || !object_ref.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  object* referenced = nullptr;
  error status = read_object_ref_identity(reader, context, referenced);
  if (status) {
    return status;
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

[[nodiscard]] core::string_name polymorphic_type_field() { return core::string_name{"$type"}; }

[[nodiscard]] core::string_name polymorphic_value_field() { return core::string_name{"$value"}; }

[[nodiscard]] const value::variant_member* find_variant_member(const variant& source,
                                                               core::string_name name) noexcept {
  return source.is_object() ? source.find_member(name) : nullptr;
}

[[nodiscard]] bool is_polymorphic_envelope(const variant& source) noexcept {
  const value::variant_member* type_member = find_variant_member(source, polymorphic_type_field());
  const value::variant_member* value_member =
      find_variant_member(source, polymorphic_value_field());
  return type_member != nullptr && type_member->value.is_string() && value_member != nullptr;
}

[[nodiscard]] result<meta::type> resolve_polymorphic_meta_type(type_id declared_base,
                                                               const variant& envelope) {
  if (!declared_base || !envelope.is_object()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const value::variant_member* type_member = envelope.find_member(polymorphic_type_field());
  if (type_member == nullptr || !type_member->value.is_string()) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  const string& text = type_member->value.as_string();
  const std::optional<guid> parsed = guid::parse(std::string_view{text.data(), text.size()});
  if (!parsed.has_value()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const meta::type dynamic_type = meta::find(type_guid{*parsed});
  if (!dynamic_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  if (!meta::is_a(dynamic_type.id(), declared_base)) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  return {{}, dynamic_type};
}

[[nodiscard]] variant_result make_polymorphic_envelope(const meta::type& dynamic_type,
                                                       variant stored_value) {
  const type_guid guid = dynamic_type.guid();
  if (!guid) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  variant::object_type members;
  members.reserve(2);
  members.push_back(value::variant_member{
      polymorphic_type_field(),
      0,
      variant::from(guid.value.string()),
  });
  members.push_back(value::variant_member{
      polymorphic_value_field(),
      0,
      std::move(stored_value),
  });

  return {{}, variant::object(std::move(members))};
}

[[nodiscard]] variant_result
make_polymorphic_indirect_value(const value::indirect_view& indirect,
                                const serialization::context& context) {
  if (!indirect.valid() || !indirect.has_value() || !indirect.can_set_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const type_id declared_type = indirect.value_type();
  const type_id dynamic_type_id = indirect.dynamic_value_type();
  if (!declared_type) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }
  if (!dynamic_type_id) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }
  if (dynamic_type_id == declared_type) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (!meta::is_a(dynamic_type_id, declared_type)) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const meta::type dynamic_type = meta::find(dynamic_type_id);
  if (!dynamic_type || !dynamic_type.guid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const any_ref dynamic_value = indirect.dynamic_value_ref();
  if (!dynamic_value.valid() || dynamic_value.type() != dynamic_type_id) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant_result value = make_serialized_value(dynamic_value, context);
  if (value.failed()) {
    return value;
  }

  return make_polymorphic_envelope(dynamic_type, std::move(value.value));
}

[[nodiscard]] error read_polymorphic_indirect_value(const variant& envelope,
                                                    const value::indirect_view& indirect,
                                                    const serialization::context& context) {
  if (!indirect.valid() || !indirect.writable() || !indirect.can_set_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<meta::type> dynamic_type = resolve_polymorphic_meta_type(indirect.value_type(), envelope);
  if (dynamic_type.failed()) {
    return dynamic_type.status;
  }

  const value::variant_member* value_member = envelope.find_member(polymorphic_value_field());
  if (value_member == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  const meta::detail::type_descriptor* descriptor =
      meta::get_registry().find_type_descriptor(dynamic_type.value.id());
  if (descriptor == nullptr || descriptor->ops.default_construct == nullptr ||
      descriptor->ops.destroy == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  void* instance = descriptor->ops.default_construct();
  if (instance == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  variant_reader value_reader{value_member->value};
  error status = deserialize(dynamic_type.value.id(), instance, value_reader, context);
  if (status) {
    descriptor->ops.destroy(instance);
    return status;
  }

  if (!indirect.set_value(dynamic_type.value.id(), instance)) {
    descriptor->ops.destroy(instance);
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

[[nodiscard]] variant_result make_indirect_serialized_value(const value::indirect_view& indirect,
                                                            const serialization::context& context) {
  if (!indirect.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (!indirect.has_value()) {
    return {{}, variant::null()};
  }

  variant_result polymorphic = make_polymorphic_indirect_value(indirect, context);
  if (polymorphic.succeeded()) {
    return polymorphic;
  }
  if (polymorphic.status.value() != errc::unsupported) {
    return polymorphic;
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

  variant_result polymorphic = make_polymorphic_indirect_value(indirect, context);
  if (polymorphic.succeeded()) {
    return write(writer, polymorphic.value);
  }
  if (polymorphic.status.value() != errc::unsupported) {
    return polymorphic.status;
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

  if (const variant* current = reader.current_variant();
      current != nullptr && is_polymorphic_envelope(*current)) {
    return read_polymorphic_indirect_value(*current, indirect, context);
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
