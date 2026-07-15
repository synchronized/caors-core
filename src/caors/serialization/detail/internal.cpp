// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "construction.h"
#include "helpers.h"
#include "polymorphic.h"

#include "caors/reflection/registry.h"

#include <limits>
#include <new>
#include <optional>
#include <string>

namespace caors::serialization::detail {
namespace {

struct polymorphic_destroy_context {
  memory::memory_resource* allocation_resource = nullptr;
  memory::memory_resource* value_resource = nullptr;
  meta::value_ops::resource_destroy_fn destroy = nullptr;
};

void destroy_polymorphic_constructed_value(void* user_data, void* value) noexcept {
  auto* context = static_cast<polymorphic_destroy_context*>(user_data);
  if (context == nullptr) {
    return;
  }

  if (context->destroy != nullptr && value != nullptr) {
    context->destroy(context->value_resource, value);
  }

  memory::destroy(context, context->allocation_resource);
}

[[nodiscard]] owned_constructed_value
construct_polymorphic_value(const meta::type& dynamic_type, const meta::indirect_view& indirect,
                            const serialization::context& context) {
  serialization::context construction_context = context;
  const bool can_transfer_destroy_context =
      context.effective_memory_resource() != nullptr && indirect.can_set_value_with_context();
  if (!can_transfer_destroy_context) {
    construction_context.resolvers.memory.resource = nullptr;
  }

  return construct_owned_value(dynamic_type, construction_context,
                               construction_site::polymorphic_value, construction_request_no_index,
                               true, can_transfer_destroy_context);
}

} // namespace

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

[[nodiscard]] const meta::enum_ops* enum_ops(const any_ref& value) noexcept {
  return value.get_type().get_raw_ops<meta::enum_ops>();
}

[[nodiscard]] bool type_uses_inline_indirect_value(const any_ref& value) noexcept {
  const meta::type type = value.get_type();
  return type &&
         (type.kind() == meta::type_kind::raw_pointer || type.kind() == meta::type_kind::wrapper) &&
         type.wrapper() == meta::wrapper_semantics::inline_value;
}

[[nodiscard]] bool type_is_enumeration(const any_ref& value) noexcept {
  return value.get_type().kind() == meta::type_kind::enumeration;
}

[[nodiscard]] variant_result make_enum_value(const any_ref& value) {
  const meta::enum_ops* data = enum_ops(value);
  if (value.get_type().kind() != meta::type_kind::enumeration || data == nullptr ||
      value.data() == nullptr) {
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
  const meta::enum_ops* data = enum_ops(value);
  if (value.get_type().kind() != meta::type_kind::enumeration || data == nullptr ||
      value.data() == nullptr) {
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
  const meta::enum_ops* data = enum_ops(target);
  if (target.get_type().kind() != meta::type_kind::enumeration || data == nullptr ||
      target.mutable_data() == nullptr || data->from_i64 == nullptr || data->from_u64 == nullptr) {
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

[[nodiscard]] variant_result
make_polymorphic_indirect_value(const meta::indirect_view& indirect,
                                const serialization::context& context) {
  if (!indirect.valid() || !indirect.has_value() || !indirect.can_set_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const type_id declared_type = indirect.get_value_type().id();
  const type_id dynamic_type_id = indirect.get_dynamic_value_type().id();
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
  if (!dynamic_value.valid() || dynamic_value.get_type().id() != dynamic_type_id) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant_result value = make_serialized_value(dynamic_value, context);
  if (value.failed()) {
    return value;
  }

  return make_polymorphic_envelope(dynamic_type, std::move(value.value), context);
}

[[nodiscard]] error read_polymorphic_indirect_value(reader& reader,
                                                    const meta::indirect_view& indirect,
                                                    const serialization::context& context) {
  if (!indirect.valid() || !indirect.writable() ||
      (!indirect.can_set_value() && !indirect.can_set_value_with_context())) {
    return {error_domain::serialization, errc::unsupported};
  }

  type_guid dynamic_guid{};
  error status = reader.read_polymorphic_type_guid(context, dynamic_guid);
  if (status) {
    return status;
  }

  result<meta::type> dynamic_type =
      resolve_polymorphic_meta_type(indirect.get_value_type().id(), dynamic_guid);
  if (dynamic_type.failed()) {
    return dynamic_type.status;
  }

  if (!dynamic_type.value) {
    return {error_domain::serialization, errc::unsupported};
  }

  owned_constructed_value instance =
      construct_polymorphic_value(dynamic_type.value, indirect, context);
  if (!instance.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  const reflection::type reflected_dynamic_type = reflection::find(dynamic_type.value.id());
  if (!reflected_dynamic_type) {
    return {error_domain::serialization, errc::not_found};
  }

  status = reader.begin_polymorphic_value(context);
  if (status.value() == errc::unsupported) {
    const variant* current = reader.current_variant();
    if (current == nullptr || !is_polymorphic_envelope(*current, context)) {
      return status;
    }

    const variant* stored_value = read_polymorphic_value(*current, context);
    if (stored_value == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }

    variant_reader value_reader{*stored_value};
    status = deserialize(context, reflected_dynamic_type, instance.value, value_reader);
    if (status) {
      return status;
    }
  } else {
    if (status) {
      return status;
    }

    status = deserialize(context, reflected_dynamic_type, instance.value, reader);
    const error end_status = reader.end_polymorphic_value();
    if (status) {
      return status;
    }
    if (end_status) {
      return end_status;
    }
  }

  if (instance.uses_resource_destroy()) {
    memory::memory_resource* resource = context.effective_memory_resource();
    if (resource == nullptr) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    void* context_storage = resource->try_allocate(sizeof(polymorphic_destroy_context),
                                                   alignof(polymorphic_destroy_context));
    if (context_storage == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    auto* destroy_context = ::new (context_storage) polymorphic_destroy_context{
        resource,
        instance.resource,
        instance.resource_destroy,
    };
    const traits::wrapper_set_context set_context{
        destroy_context,
        &destroy_polymorphic_constructed_value,
        resource,
    };
    if (!indirect.set_value(dynamic_type.value.id(), instance.value, set_context)) {
      memory::destroy(destroy_context, destroy_context->allocation_resource);
      return {error_domain::serialization, errc::invalid_field};
    }

    instance.release();
    return {};
  }

  if (!indirect.set_value(dynamic_type.value.id(), instance.value)) {
    return {error_domain::serialization, errc::invalid_field};
  }

  instance.release();
  return {};
}

[[nodiscard]] error read_constructed_indirect_value(reader& reader,
                                                    const meta::indirect_view& indirect,
                                                    const serialization::context& context) {
  const meta::type value_type = indirect.get_value_type();
  if (!value_type || (!indirect.can_emplace_value() && !indirect.can_set_value() &&
                      !indirect.can_set_value_with_context())) {
    return {error_domain::serialization, errc::invalid_type};
  }

  serialization::context construction_context = context;
  const bool can_emplace_constructed_value = indirect.can_emplace_value();
  const bool can_transfer_destroy_context =
      context.effective_memory_resource() != nullptr && indirect.can_set_value_with_context();
  if (!can_transfer_destroy_context && !can_emplace_constructed_value) {
    construction_context.resolvers.memory.resource = nullptr;
  }

  owned_constructed_value instance =
      construct_owned_value(value_type, construction_context, construction_site::indirect_value,
                            construction_request_no_index, true, can_transfer_destroy_context);
  if (!instance.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  any_ref value = any_ref::from_raw(value_type, instance.value, true);
  if (!value.valid() || !value.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  error status = read_serialized_value(reader, value, context);
  if (status) {
    return status;
  }

  if (instance.uses_resource_destroy()) {
    memory::memory_resource* resource = context.effective_memory_resource();
    if (resource == nullptr) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    void* context_storage = resource->try_allocate(sizeof(polymorphic_destroy_context),
                                                   alignof(polymorphic_destroy_context));
    if (context_storage == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    auto* destroy_context = ::new (context_storage) polymorphic_destroy_context{
        resource,
        instance.resource,
        instance.resource_destroy,
    };
    const traits::wrapper_set_context set_context{
        destroy_context,
        &destroy_polymorphic_constructed_value,
        resource,
    };
    if (!indirect.set_value(value_type.id(), instance.value, set_context)) {
      memory::destroy(destroy_context, destroy_context->allocation_resource);
      return {error_domain::serialization, errc::invalid_field};
    }

    instance.release();
    return {};
  }

  if (indirect.can_emplace_value()) {
    if (!indirect.emplace_value(value_type.id(), instance.value)) {
      return {error_domain::serialization, errc::invalid_field};
    }

    return {};
  }

  if (!indirect.set_value(value_type.id(), instance.value)) {
    return {error_domain::serialization, errc::invalid_field};
  }

  instance.release();
  return {};
}

[[nodiscard]] variant_result make_indirect_serialized_value(const meta::indirect_view& indirect,
                                                            const serialization::context& context) {
  if (!indirect.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (!indirect.has_value()) {
    return {{}, variant::null()};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return {nested_context.status, {}};
  }

  variant_result polymorphic = make_polymorphic_indirect_value(indirect, nested_context.value);
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

  return make_serialized_value(value, nested_context.value);
}

[[nodiscard]] error write_indirect_serialized_value(writer& writer,
                                                    const meta::indirect_view& indirect,
                                                    const serialization::context& context) {
  if (!indirect.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!indirect.has_value()) {
    return writer.write_null();
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  variant_result polymorphic = make_polymorphic_indirect_value(indirect, nested_context.value);
  if (polymorphic.succeeded()) {
    return serialization::write_variant(writer, polymorphic.value);
  }
  if (polymorphic.status.value() != errc::unsupported) {
    return polymorphic.status;
  }

  const any_ref value = indirect.value_ref();
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  return write_serialized_value(writer, value, nested_context.value);
}

[[nodiscard]] error read_indirect_serialized_value(reader& reader,
                                                   const meta::indirect_view& indirect,
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

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  if (reader.is_polymorphic_envelope(context)) {
    return read_polymorphic_indirect_value(reader, indirect, nested_context.value);
  }

  if (!indirect.has_value() && !indirect.ensure_value(context.effective_memory_resource())) {
    if (indirect.can_emplace_value() || indirect.can_set_value() ||
        indirect.can_set_value_with_context()) {
      return read_constructed_indirect_value(reader, indirect, nested_context.value);
    }

    return {error_domain::serialization, errc::invalid_field};
  }

  any_ref value = indirect.value_ref();
  if (!value.valid() || !value.writable() || value.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return read_serialized_value(reader, value, nested_context.value);
}

} // namespace caors::serialization::detail
