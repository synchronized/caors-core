// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/reflection/registry.h"

#include <optional>
#include <utility>

namespace caors::serialization {
namespace {

[[nodiscard]] core::string_name polymorphic_type_field() { return core::string_name{"$type"}; }

[[nodiscard]] core::string_name polymorphic_value_field() { return core::string_name{"$value"}; }

[[nodiscard]] variant_result make_polymorphic_envelope(const objects::type& dynamic_type,
                                                       variant value) {
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
      std::move(value),
  });

  return {{}, variant::object(std::move(members))};
}

[[nodiscard]] result<type_guid> read_polymorphic_type_guid(const variant& envelope) {
  if (!envelope.is_object()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
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

  type_guid result{*parsed};
  if (!result) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  return {{}, result};
}

[[nodiscard]] const variant* read_polymorphic_value(const variant& envelope) {
  if (!envelope.is_object()) {
    return nullptr;
  }

  const value::variant_member* value_member = envelope.find_member(polymorphic_value_field());
  return value_member != nullptr ? &value_member->value : nullptr;
}

[[nodiscard]] result<objects::type> resolve_polymorphic_type(objects::object_type_id declared_base,
                                                             const variant& envelope) {
  if (!declared_base) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  result<type_guid> guid = read_polymorphic_type_guid(envelope);
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

variant_result to_polymorphic_serialization_value(const objects::object& object,
                                                  const serialization::context& context) {
  const objects::object_type_id dynamic_id = object.type_id();
  if (!dynamic_id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const objects::type dynamic_type = objects::find(dynamic_id);
  if (!dynamic_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  variant_result value = to_serialization_value(dynamic_type.id(), &object, context);
  if (value.failed()) {
    return value;
  }

  return make_polymorphic_envelope(dynamic_type, std::move(value.value));
}

variant_result to_polymorphic_serialization_value(const objects::object* object,
                                                  const serialization::context& context) {
  if (object == nullptr) {
    return {{}, variant::null()};
  }

  return to_polymorphic_serialization_value(*object, context);
}

result<objects::object*> deserialize_polymorphic_object(objects::object_type_id declared_base,
                                                        const variant& value,
                                                        const serialization::context& context) {
  if (value.is_null()) {
    return {{}, nullptr};
  }

  if (context.object_manager == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, nullptr};
  }

  result<objects::type> dynamic_type = resolve_polymorphic_type(declared_base, value);
  if (dynamic_type.failed()) {
    return {dynamic_type.status, nullptr};
  }

  const variant* stored_value = read_polymorphic_value(value);
  if (stored_value == nullptr) {
    return {{error_domain::serialization, errc::not_found}, nullptr};
  }

  objects::object* instance = context.object_manager->create(dynamic_type.value.id());
  if (instance == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, nullptr};
  }

  variant_reader reader{*stored_value};
  const error status = deserialize(dynamic_type.value.id(), instance, reader, context);
  if (status) {
    (void)context.object_manager->destroy(instance);
    return {status, nullptr};
  }

  return {{}, instance};
}

serialization_text_result serialize_polymorphic_json(const objects::object& object,
                                                     const serialization::context& context) {
  variant_result value = to_polymorphic_serialization_value(object, context);
  if (value.failed()) {
    return {value.status, {}};
  }

  serialization::json_writer writer;
  const error status = write(writer, value.value);
  if (status) {
    return {status, {}};
  }

  return {{}, writer.text()};
}

result<objects::object*> deserialize_polymorphic_json(objects::object_type_id declared_base,
                                                      std::string_view text,
                                                      const serialization::context& context) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return {value.status, nullptr};
  }

  return deserialize_polymorphic_object(declared_base, value.value, context);
}

} // namespace caors::serialization
