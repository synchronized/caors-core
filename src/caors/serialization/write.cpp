// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "detail/internal.h"

namespace caors::serialization {

error write_variant(writer& writer, const variant& value) {
  switch (value.kind()) {
  case meta::variant_kind::null:
    return writer.write_null();
  case meta::variant_kind::boolean:
    return writer.write_bool(value.as_bool());
  case meta::variant_kind::signed_integer:
    return writer.write_i64(value.as_i64());
  case meta::variant_kind::unsigned_integer:
    return writer.write_u64(value.as_u64());
  case meta::variant_kind::floating:
    return writer.write_f64(value.as_f64());
  case meta::variant_kind::string:
    return writer.write_string(value.as_string());
  case meta::variant_kind::array: {
    error status = writer.begin_array(value.as_array().size());
    if (status) {
      return status;
    }

    std::size_t index = 0;
    for (const variant& element : value.as_array()) {
      status = writer.begin_element(index);
      if (status) {
        return status;
      }

      status = write_variant(writer, element);
      if (status) {
        return status;
      }

      status = writer.end_element();
      if (status) {
        return status;
      }

      ++index;
    }

    return writer.end_array();
  }
  case meta::variant_kind::dictionary: {
    error status = writer.begin_dictionary(value.as_dictionary().size());
    if (status) {
      return status;
    }

    std::size_t index = 0;
    for (const meta::variant_entry& entry : value.as_dictionary()) {
      status = writer.begin_entry(index);
      if (status) {
        return status;
      }

      status = writer.begin_key();
      if (status) {
        return status;
      }

      status = write_variant(writer, entry.key);
      if (status) {
        return status;
      }

      status = writer.end_key();
      if (status) {
        return status;
      }

      status = writer.begin_value();
      if (status) {
        return status;
      }

      status = write_variant(writer, entry.value);
      if (status) {
        return status;
      }

      status = writer.end_value();
      if (status) {
        return status;
      }

      status = writer.end_entry();
      if (status) {
        return status;
      }

      ++index;
    }

    return writer.end_dictionary();
  }
  case meta::variant_kind::object: {
    serialization::type_schema type_schema;
    type_schema.has_explicit_fields = true;
    type_schema.fields.reserve(value.as_object().size());
    for (const meta::variant_member& member : value.as_object()) {
      serialization::field_options options = serialization::make_field(member.name.view());
      options.field_id = member.field_id;
      type_schema.fields.emplace_back(options);
    }

    error status = writer.begin_object(type_schema);
    if (status) {
      return status;
    }

    for (std::size_t index = 0; index < value.as_object().size(); ++index) {
      const meta::variant_member& member = value.as_object()[index];
      status = serialization::detail::validate_field_for_writer(type_schema.fields[index],
                                                                writer.features());
      if (status) {
        return status;
      }

      status = writer.begin_field(type_schema.fields[index]);
      if (status) {
        return status;
      }

      status = write_variant(writer, member.value);
      if (status) {
        return status;
      }

      status = writer.end_field();
      if (status) {
        return status;
      }
    }

    return writer.end_object();
  }
  }

  return {error_domain::serialization, errc::invalid_argument};
}

} // namespace caors::serialization
