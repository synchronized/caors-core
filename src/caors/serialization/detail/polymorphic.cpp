// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "polymorphic.h"

#include "caors/meta/query.h"

#include <optional>
#include <string_view>
#include <utility>

namespace caors::serialization::detail {

core::string_name polymorphic_type_field() { return core::string_name{"$type"}; }

core::string_name polymorphic_value_field() { return core::string_name{"$value"}; }

const value::variant_member* find_variant_member(const variant& source,
                                                 core::string_name name) noexcept {
  return source.is_object() ? source.find_member(name) : nullptr;
}

bool is_polymorphic_envelope(const variant& source) noexcept {
  const value::variant_member* type_member = find_variant_member(source, polymorphic_type_field());
  const value::variant_member* value_member =
      find_variant_member(source, polymorphic_value_field());
  return type_member != nullptr && type_member->value.is_string() && value_member != nullptr;
}

result<type_guid> read_polymorphic_type_guid(const variant& envelope) {
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

const variant* read_polymorphic_value(const variant& envelope) noexcept {
  const value::variant_member* value_member =
      find_variant_member(envelope, polymorphic_value_field());
  return value_member != nullptr ? &value_member->value : nullptr;
}

result<meta::type> resolve_polymorphic_meta_type(type_id declared_base, const variant& envelope) {
  if (!declared_base || !envelope.is_object()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  result<type_guid> guid = read_polymorphic_type_guid(envelope);
  if (guid.failed()) {
    return {guid.status, {}};
  }

  const meta::type dynamic_type = meta::find(guid.value);
  if (!dynamic_type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  if (!meta::is_a(dynamic_type.id(), declared_base)) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  return {{}, dynamic_type};
}

variant_result make_polymorphic_envelope(type_guid guid, variant stored_value) {
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

variant_result make_polymorphic_envelope(const meta::type& dynamic_type, variant stored_value) {
  return make_polymorphic_envelope(dynamic_type.guid(), std::move(stored_value));
}

} // namespace caors::serialization::detail
