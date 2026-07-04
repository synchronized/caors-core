// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/object/ref_view.h"
#include "caors/reflection/registry.h"
#include "caors/value/indirect_view.h"

#include "detail/helpers.h"
#include "detail/internal.h"

#include <utility>

namespace caors::serialization {
namespace {

[[nodiscard]] variant_result
object_ref_to_serialization_value(const reflection::field& field, const void* object,
                                  const serialization::context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  return serialization::detail::make_object_ref_identity_value(field.get_object_ref(object),
                                                               context);
}

[[nodiscard]] bool field_uses_inline_indirect_value(const reflection::field& field) noexcept {
  return (field.kind() == meta::type_kind::raw_pointer ||
          field.kind() == meta::type_kind::wrapper) &&
         field.wrapper_semantics() == meta::wrapper_semantics::inline_value;
}

[[nodiscard]] variant_result
value_ref_to_serialization_value(const any_ref& value, const serialization::context& context) {
  return serialization::detail::make_serialized_value(value, context);
}

[[nodiscard]] variant_result field_to_serialization_value(const reflection::field& field,
                                                          const void* object,
                                                          const serialization::context& context) {
  if (objects::is_object_ref(field.get_ref(object))) {
    return object_ref_to_serialization_value(field, object, context);
  }

  any_ref field_ref{};
  if (field_uses_inline_indirect_value(field)) {
    const value::indirect_view indirect = field.get_ref(object).create_indirect_view();
    if (!indirect.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    if (!indirect.has_value()) {
      return {{}, variant::null()};
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

variant_result to_variant(const any& data) {
  return serialization::detail::make_serialized_value(data);
}

variant_result to_variant(const any_ref& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  return serialization::detail::make_serialized_value(data, serialization::context{});
}

variant_result to_serialization_value(const reflection::type& type, const void* object) {
  return to_serialization_value(type, object, serialization::context{});
}

variant_result to_serialization_value(const reflection::type& type, const void* object,
                                      const serialization::context& context) {
  if (!type.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (object == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  if (!serialization::contains(type.id())) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<serialization::type_data> serialization_data = serialization::find(type.id());
  if (!serialization_data.has_value() || !serialization_data->valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  variant::object_type members;

  if (!serialization_data->fields.empty()) {
    members.reserve(serialization_data->fields.size());
    for (const serialization::field_data& field_data : serialization_data->fields) {
      if (field_data.skipped ||
          !serialization::detail::field_visible_in_context(field_data, context)) {
        continue;
      }

      const reflection::field field = type.find_field(field_data.field_name);
      if (!field) {
        return {{error_domain::serialization, errc::not_found}, {}};
      }

      variant_result serialized = field_to_serialization_value(field, object, context);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(value::variant_member{
          serialization::detail::serialized_field_name(field_data),
          field_data.field_id,
          std::move(serialized.value),
      });
    }
  } else {
    members.reserve(type.field_count());
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const reflection::field field = type.field_at(index);
      if (!field) {
        return {{error_domain::serialization, errc::invalid_field}, {}};
      }

      variant_result serialized = field_to_serialization_value(field, object, context);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(value::variant_member{
          field.name(),
          0,
          std::move(serialized.value),
      });
    }
  }

  return {{}, variant::object(std::move(members))};
}

variant_result to_serialization_value(type_id id, const void* object) {
  return to_serialization_value(id, object, serialization::context{});
}

variant_result to_serialization_value(type_id id, const void* object,
                                      const serialization::context& context) {
  if (!id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (!serialization::contains(id)) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const reflection::type type = reflection::find(id);
  if (!type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return to_serialization_value(type, object, context);
}

} // namespace caors::serialization
