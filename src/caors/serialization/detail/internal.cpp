// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"
#include "polymorphic.h"

#include "caors/meta/detail/type_capabilities.h"
#include "caors/object/ptr_view.h"
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
  meta::detail::value_resource_destroy_fn resource_destroy = nullptr;
  meta::detail::value_destroy_fn default_destroy = nullptr;
  bool transfer_destroy_context = false;

  constructed_value() = default;

  constructed_value(void* constructed, memory::memory_resource* memory,
                    meta::detail::value_resource_destroy_fn destroy, bool transfer_context) noexcept
      : value(constructed), resource(memory), resource_destroy(destroy),
        transfer_destroy_context(transfer_context) {}

  constructed_value(void* constructed, meta::detail::value_destroy_fn destroy) noexcept
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
  meta::detail::value_resource_destroy_fn destroy = nullptr;
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

[[nodiscard]] constructed_value construct_polymorphic_value(
    const meta::type& dynamic_type, const meta::detail::type_descriptor& descriptor,
    const value::indirect_view& indirect, const serialization::context& context) {
  const meta::detail::value_factory_ops registered_factory =
      meta::get_registry().find_type_factory(dynamic_type.id());

  if (context.memory_resource != nullptr && indirect.can_set_value_with_context()) {
    if (registered_factory.valid()) {
      return {registered_factory.construct(context.memory_resource), context.memory_resource,
              registered_factory.destroy, true};
    }

    if (descriptor.ops.resource_construct != nullptr &&
        descriptor.ops.resource_destroy != nullptr) {
      return {descriptor.ops.resource_construct(context.memory_resource), context.memory_resource,
              descriptor.ops.resource_destroy, true};
    }
  }

  if (registered_factory.valid()) {
    return {registered_factory.construct(nullptr), nullptr, registered_factory.destroy, false};
  }

  if (descriptor.ops.default_construct == nullptr || descriptor.ops.destroy == nullptr) {
    return {};
  }

  return {descriptor.ops.default_construct(), descriptor.ops.destroy};
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

[[nodiscard]] bool type_uses_object_ptr(const any_ref& value) noexcept {
  return objects::is_ptr(value);
}

[[nodiscard]] variant_result make_object_ptr_identity_value(const object* referenced,
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

[[nodiscard]] error write_object_ptr_identity(writer& writer, const object* referenced,
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

[[nodiscard]] error read_object_ptr_identity(reader& reader, const serialization::context& context,
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

[[nodiscard]] variant_result make_object_ptr_value(const any_ref& value,
                                                   const serialization::context& context) {
  const objects::ptr_view object_ptr = objects::ptr_view_of(value);
  if (!object_ptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_object_ptr_identity_value(object_ptr.get(), context);
}

[[nodiscard]] error write_object_ptr_value(writer& writer, const any_ref& value,
                                           const serialization::context& context) {
  const objects::ptr_view object_ptr = objects::ptr_view_of(value);
  if (!object_ptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return write_object_ptr_identity(writer, object_ptr.get(), context);
}

[[nodiscard]] error read_object_ptr_value(reader& reader, const any_ref& target,
                                          const serialization::context& context) {
  const objects::ptr_view object_ptr = objects::ptr_view_of(target);
  if (!object_ptr || !object_ptr.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  object* referenced = nullptr;
  error status = read_object_ptr_identity(reader, context, referenced);
  if (status) {
    return status;
  }

  if (!object_ptr.set(referenced)) {
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

  return make_polymorphic_envelope(dynamic_type, std::move(value.value), context);
}

[[nodiscard]] error read_polymorphic_indirect_value(const variant& envelope,
                                                    const value::indirect_view& indirect,
                                                    const serialization::context& context) {
  if (!indirect.valid() || !indirect.writable() || !indirect.can_set_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<meta::type> dynamic_type =
      resolve_polymorphic_meta_type(indirect.value_type(), envelope, context);
  if (dynamic_type.failed()) {
    return dynamic_type.status;
  }

  const variant* stored_value = read_polymorphic_value(envelope, context);
  if (stored_value == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  const meta::detail::type_descriptor* descriptor =
      meta::get_registry().find_type_descriptor(dynamic_type.value.id());
  if (descriptor == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  constructed_value instance =
      construct_polymorphic_value(dynamic_type.value, *descriptor, indirect, context);
  if (!instance.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  variant_reader value_reader{*stored_value};
  error status = deserialize(dynamic_type.value.id(), instance.value, value_reader, context);
  if (status) {
    return status;
  }

  if (instance.uses_resource_destroy()) {
    void* context_storage = context.memory_resource->try_allocate(
        sizeof(polymorphic_destroy_context), alignof(polymorphic_destroy_context));
    if (context_storage == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    auto* destroy_context = ::new (context_storage) polymorphic_destroy_context{
        context.memory_resource,
        instance.resource,
        instance.resource_destroy,
    };
    const traits::wrapper_set_context set_context{
        destroy_context,
        &destroy_polymorphic_constructed_value,
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
    return serialization::write_variant(writer, polymorphic.value);
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
      current != nullptr && is_polymorphic_envelope(*current, context)) {
    return read_polymorphic_indirect_value(*current, indirect, context);
  }

  if (!indirect.has_value() && !indirect.ensure_value(context.memory_resource)) {
    if (indirect.can_set_value() || indirect.can_set_value_with_context()) {
      return {error_domain::serialization, errc::invalid_type};
    }

    return {error_domain::serialization, errc::invalid_field};
  }

  any_ref value = indirect.value_ref();
  if (!value.valid() || !value.writable() || value.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return read_serialized_value(reader, value, context);
}

} // namespace caors::serialization::detail
