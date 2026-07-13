// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"
#include "polymorphic.h"

#include "caors/reflection/registry.h"

#include <limits>
#include <new>
#include <optional>
#include <string>

namespace caors::serialization::detail {
namespace {

struct constructed_value {
  void* value = nullptr;
  memory::memory_resource* resource = nullptr;
  meta::value_resource_destroy_fn resource_destroy = nullptr;
  meta::value_destroy_fn default_destroy = nullptr;
  bool transfer_destroy_context = false;

  constructed_value() = default;

  constructed_value(void* constructed, memory::memory_resource* memory,
                    meta::value_resource_destroy_fn destroy, bool transfer_context) noexcept
      : value(constructed), resource(memory), resource_destroy(destroy),
        transfer_destroy_context(transfer_context) {}

  constructed_value(void* constructed, meta::value_destroy_fn destroy) noexcept
      : value(constructed), default_destroy(destroy) {}

  constructed_value(const constructed_value&) = delete;
  constructed_value& operator=(const constructed_value&) = delete;

  constructed_value(constructed_value&& other) noexcept
      : value(other.value), resource(other.resource), resource_destroy(other.resource_destroy),
        default_destroy(other.default_destroy),
        transfer_destroy_context(other.transfer_destroy_context) {
    other.release();
  }

  constructed_value& operator=(constructed_value&& other) noexcept {
    if (this != &other) {
      reset();
      value = other.value;
      resource = other.resource;
      resource_destroy = other.resource_destroy;
      default_destroy = other.default_destroy;
      transfer_destroy_context = other.transfer_destroy_context;
      other.release();
    }

    return *this;
  }

  ~constructed_value() { reset(); }

  [[nodiscard]] bool valid() const noexcept { return value != nullptr; }
  [[nodiscard]] bool uses_resource_destroy() const noexcept {
    return value != nullptr && resource_destroy != nullptr && transfer_destroy_context;
  }

  void release() noexcept {
    value = nullptr;
    resource = nullptr;
    resource_destroy = nullptr;
    default_destroy = nullptr;
    transfer_destroy_context = false;
  }

  void reset() noexcept {
    if (value == nullptr) {
      return;
    }

    if (resource_destroy != nullptr) {
      resource_destroy(resource, value);
    } else if (default_destroy != nullptr) {
      default_destroy(value);
    }

    release();
  }
};

struct polymorphic_destroy_context {
  memory::memory_resource* allocation_resource = nullptr;
  memory::memory_resource* value_resource = nullptr;
  meta::value_resource_destroy_fn destroy = nullptr;
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

[[nodiscard]] constructed_value construct_polymorphic_value(const meta::type& dynamic_type,
                                                            const value::indirect_view& indirect,
                                                            const serialization::context& context) {
  const meta::value_factory_ops registered_factory =
      meta::get_registry().find_type_factory(dynamic_type.id());
  memory::memory_resource* resource = context.effective_memory_resource();

  if (resource != nullptr && indirect.can_set_value_with_context()) {
    if (registered_factory.valid()) {
      return {registered_factory.construct(resource), resource, registered_factory.destroy, true};
    }

    const auto resource_construct = meta::get_resource_construct_value_fn(dynamic_type);
    const auto resource_destroy = meta::get_resource_destroy_value_fn(dynamic_type);
    if (resource_construct != nullptr && resource_destroy != nullptr) {
      return {resource_construct(resource), resource, resource_destroy, true};
    }
  }

  if (registered_factory.valid()) {
    return {registered_factory.construct(nullptr), nullptr, registered_factory.destroy, false};
  }

  const auto default_construct = meta::get_default_construct_value_fn(dynamic_type);
  const auto destroy = meta::get_destroy_value_fn(dynamic_type);
  if (default_construct == nullptr || destroy == nullptr) {
    return {};
  }

  return {default_construct(), destroy};
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
  return value.get_type().get_capability<meta::enum_ops>();
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
make_polymorphic_indirect_value(const value::indirect_view& indirect,
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

[[nodiscard]] error read_polymorphic_indirect_value(const variant& envelope,
                                                    const value::indirect_view& indirect,
                                                    const serialization::context& context) {
  if (!indirect.valid() || !indirect.writable() || !indirect.can_set_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<meta::type> dynamic_type =
      resolve_polymorphic_meta_type(indirect.get_value_type().id(), envelope, context);
  if (dynamic_type.failed()) {
    return dynamic_type.status;
  }

  const variant* stored_value = read_polymorphic_value(envelope, context);
  if (stored_value == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  if (!dynamic_type.value) {
    return {error_domain::serialization, errc::unsupported};
  }

  constructed_value instance = construct_polymorphic_value(dynamic_type.value, indirect, context);
  if (!instance.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  variant_reader value_reader{*stored_value};
  const reflection::type reflected_dynamic_type = reflection::find(dynamic_type.value.id());
  if (!reflected_dynamic_type) {
    return {error_domain::serialization, errc::not_found};
  }

  error status = deserialize(context, reflected_dynamic_type, instance.value, value_reader);
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

[[nodiscard]] variant_result make_indirect_serialized_value(const value::indirect_view& indirect,
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
                                                    const value::indirect_view& indirect,
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

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  if (const variant* current = reader.current_variant();
      current != nullptr && is_polymorphic_envelope(*current, context)) {
    return read_polymorphic_indirect_value(*current, indirect, nested_context.value);
  }

  if (!indirect.has_value() && !indirect.ensure_value(context.effective_memory_resource())) {
    if (indirect.can_set_value() || indirect.can_set_value_with_context()) {
      return {error_domain::serialization, errc::invalid_type};
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
