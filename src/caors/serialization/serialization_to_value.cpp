// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/reflection/registry.h"

#include "serialization_internal.h"

#include <utility>

namespace caors {
namespace {

[[nodiscard]] variant_result
object_ref_to_serialization_value(const reflection_field& field, const void* object,
                                  const serialization_context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  const caors::object* referenced = field.get_object_ref(object);
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return {{}, variant::from(id.value())};
}

[[nodiscard]] bool field_uses_inline_indirect_value(const reflection_field& field) noexcept {
  return (field.kind() == type_kind::raw_pointer || field.kind() == type_kind::wrapper) &&
         field.wrapper_semantics() == wrapper_semantics::inline_value;
}

[[nodiscard]] variant_result
value_ref_to_serialization_value(const any_ref& value, const serialization_context& context) {
  return detail::make_serialized_value(value, context);
}

[[nodiscard]] variant_result field_to_serialization_value(const reflection_field& field,
                                                          const void* object,
                                                          const serialization_context& context) {
  if (field.wrapper_semantics() == wrapper_semantics::object_ref) {
    return object_ref_to_serialization_value(field, object, context);
  }

  any_ref field_ref{};
  if (field_uses_inline_indirect_value(field)) {
    const indirect_view indirect = field.create_indirect_view(object);
    if (!indirect.valid() || !indirect.has_value()) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    field_ref = indirect.value_ref();
  } else {
    field_ref = field.get_ref(object);
  }

  if (!field_ref.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return value_ref_to_serialization_value(field_ref, context);
}

} // namespace

variant_result to_variant(const any& data) { return detail::make_serialized_value(data); }

variant_result to_variant(const any_ref& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  return detail::make_serialized_value(data, serialization_context{});
}

variant_result to_serialization_value(const reflection_type& type, const void* object) {
  return to_serialization_value(type, object, serialization_context{});
}

variant_result to_serialization_value(const reflection_type& type, const void* object,
                                      const serialization_context& context) {
  if (!type.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (object == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  variant::object_type members;

  if (!serialization->fields.empty()) {
    members.reserve(serialization->fields.size());
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped || !detail::field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::not_found}, {}};
      }

      variant_result serialized = field_to_serialization_value(*field, object, context);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(variant_member{
          detail::serialized_field_name(field_data),
          field_data.field_id,
          std::move(serialized.value),
      });
    }
  } else {
    members.reserve(type.field_count());
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::invalid_field}, {}};
      }

      variant_result serialized = field_to_serialization_value(*field, object, context);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(variant_member{
          field->name(),
          0,
          std::move(serialized.value),
      });
    }
  }

  return {{}, variant::object(std::move(members))};
}

variant_result to_serialization_value(type_id id, const void* object) {
  return to_serialization_value(id, object, serialization_context{});
}

variant_result to_serialization_value(type_id id, const void* object,
                                      const serialization_context& context) {
  if (!id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (!get_serialization_registry().contains(id)) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return to_serialization_value(*type, object, context);
}

} // namespace caors
