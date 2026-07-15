// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "detail/polymorphic.h"

#include "caors/reflection/registry.h"

#include <utility>

namespace caors::serialization {

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

} // namespace caors::serialization
