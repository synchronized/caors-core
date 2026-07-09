// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "detail/polymorphic.h"

#include "caors/reflection/registry.h"

#include <utility>

namespace caors::serialization {
namespace {

[[nodiscard]] result<objects::type>
resolve_polymorphic_type(objects::object_type_id declared_base, const variant& envelope,
                         const serialization::context& context) {
  if (!declared_base) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  result<type_guid> guid = detail::read_polymorphic_type_guid(envelope, context);
  if (guid.failed()) {
    return {guid.status, {}};
  }

  const meta::type meta_type = meta::find(guid.value);
  if (!meta_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const objects::type object_type = objects::find(meta_type.id());
  if (!object_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  if (!objects::is_a(object_type.id(), declared_base)) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (!object_type.constructible()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, object_type};
}

} // namespace

namespace detail {

bool object_requires_polymorphic_envelope(type_id declared_id,
                                          const objects::object& object) noexcept {
  const objects::object_type_id dynamic_id = object.type_id();
  return declared_id && dynamic_id && dynamic_id != declared_id;
}

variant_result to_polymorphic_object_serialization_value(const serialization::context& context,
                                                         type_id declared_id,
                                                         const objects::object& object) {
  const objects::object_type_id dynamic_id = object.type_id();
  if (!declared_id || !dynamic_id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (dynamic_id == declared_id) {
    const reflection::type declared_type = reflection::find(declared_id);
    if (!declared_type) {
      return {{error_domain::serialization, errc::not_found}, {}};
    }

    return to_serialization_value(context, declared_type, &object);
  }

  if (!objects::is_a(dynamic_id, declared_id)) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const objects::type dynamic_type = objects::find(dynamic_id);
  if (!dynamic_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const reflection::type reflected_dynamic_type = reflection::find(dynamic_type.id());
  if (!reflected_dynamic_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  variant_result value = to_serialization_value(context, reflected_dynamic_type, &object);
  if (value.failed()) {
    return value;
  }

  return make_polymorphic_envelope(dynamic_type.guid(), dynamic_type.name(), std::move(value.value),
                                   context);
}

error serialize_polymorphic_object(const serialization::context& context, type_id declared_id,
                                   const objects::object& object, writer& writer) {
  variant_result value = to_polymorphic_object_serialization_value(context, declared_id, object);
  if (value.failed()) {
    return value.status;
  }

  return serialization::write_variant(writer, value.value);
}

bool is_polymorphic_value(const serialization::context& context, const variant& value) noexcept {
  return is_polymorphic_envelope(value, context);
}

error deserialize_polymorphic_object_into(const serialization::context& context,
                                          type_id declared_id, objects::object& object,
                                          const variant& value) {
  result<meta::type> dynamic_type = resolve_polymorphic_meta_type(declared_id, value, context);
  if (dynamic_type.failed()) {
    return dynamic_type.status;
  }

  if (object.type_id() != dynamic_type.value.id()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  const variant* stored_value = read_polymorphic_value(value, context);
  if (stored_value == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  variant_reader reader{*stored_value};
  const reflection::type reflected_dynamic_type = reflection::find(dynamic_type.value.id());
  if (!reflected_dynamic_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return deserialize(context, reflected_dynamic_type, &object, reader);
}

} // namespace detail

variant_result to_polymorphic_serialization_value(const objects::object& object) {
  return to_polymorphic_serialization_value(serialization::context{}, object);
}

variant_result to_polymorphic_serialization_value(const serialization::context& context,
                                                  const objects::object& object) {
  const objects::object_type_id dynamic_id = object.type_id();
  if (!dynamic_id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const objects::type dynamic_type = objects::find(dynamic_id);
  if (!dynamic_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const reflection::type reflected_dynamic_type = reflection::find(dynamic_type.id());
  if (!reflected_dynamic_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  variant_result value = detail::to_serialization_value(context, reflected_dynamic_type, &object);
  if (value.failed()) {
    return value;
  }

  return detail::make_polymorphic_envelope(dynamic_type.guid(), dynamic_type.name(),
                                           std::move(value.value), context);
}

variant_result to_polymorphic_serialization_value(const objects::object* object) {
  return to_polymorphic_serialization_value(serialization::context{}, object);
}

variant_result to_polymorphic_serialization_value(const serialization::context& context,
                                                  const objects::object* object) {
  if (object == nullptr) {
    return {{}, variant::null()};
  }

  return to_polymorphic_serialization_value(context, *object);
}

result<objects::object*> deserialize_polymorphic_object(const serialization::context& context,
                                                        objects::object_type_id declared_base,
                                                        const variant& value) {
  if (value.is_null()) {
    return {{}, nullptr};
  }

  objects::manager* manager = context.effective_object_manager();
  if (manager == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, nullptr};
  }

  result<objects::type> dynamic_type = resolve_polymorphic_type(declared_base, value, context);
  if (dynamic_type.failed()) {
    return {dynamic_type.status, nullptr};
  }

  const variant* stored_value = detail::read_polymorphic_value(value, context);
  if (stored_value == nullptr) {
    return {{error_domain::serialization, errc::not_found}, nullptr};
  }

  objects::object* instance = manager->create(dynamic_type.value.id());
  if (instance == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, nullptr};
  }

  variant_reader reader{*stored_value};
  const reflection::type reflected_dynamic_type = reflection::find(dynamic_type.value.id());
  if (!reflected_dynamic_type) {
    (void)manager->destroy(instance);
    return {{error_domain::serialization, errc::not_found}, nullptr};
  }

  const error status = detail::deserialize(context, reflected_dynamic_type, instance, reader);
  if (status) {
    (void)manager->destroy(instance);
    return {status, nullptr};
  }

  return {{}, instance};
}

serialization_text_result serialize_polymorphic_json(const objects::object& object) {
  return serialize_polymorphic_json(serialization::context{}, object);
}

serialization_text_result serialize_polymorphic_json(const serialization::context& context,
                                                     const objects::object& object) {
  variant_result value = to_polymorphic_serialization_value(context, object);
  if (value.failed()) {
    return {value.status, {}};
  }

  serialization::json_writer writer;
  const error status = write_variant(writer, value.value);
  if (status) {
    return {status, {}};
  }

  return {{}, writer.text()};
}

result<objects::object*> deserialize_polymorphic_json(const serialization::context& context,
                                                      objects::object_type_id declared_base,
                                                      std::string_view text) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return {value.status, nullptr};
  }

  return deserialize_polymorphic_object(context, declared_base, value.value);
}

} // namespace caors::serialization
