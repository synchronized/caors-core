// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "serialization_internal.h"

namespace caors {

error write(serialization_writer& writer, const variant& value) {
  switch (value.kind()) {
  case variant_kind::null:
    return writer.write_null();
  case variant_kind::boolean:
    return writer.write_bool(value.as_bool());
  case variant_kind::signed_integer:
    return writer.write_i64(value.as_i64());
  case variant_kind::unsigned_integer:
    return writer.write_u64(value.as_u64());
  case variant_kind::floating:
    return writer.write_f64(value.as_f64());
  case variant_kind::string:
    return writer.write_string(value.as_string());
  case variant_kind::array: {
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

      status = write(writer, element);
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
  case variant_kind::dictionary: {
    error status = writer.begin_dictionary(value.as_dictionary().size());
    if (status) {
      return status;
    }

    std::size_t index = 0;
    for (const variant_entry& entry : value.as_dictionary()) {
      status = writer.begin_entry(index);
      if (status) {
        return status;
      }

      status = writer.begin_key();
      if (status) {
        return status;
      }

      status = write(writer, entry.key);
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

      status = write(writer, entry.value);
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
  case variant_kind::object: {
    serialization::type_data type_data;
    type_data.fields.reserve(value.as_object().size());
    for (const variant_member& member : value.as_object()) {
      type_data.fields.push_back(serialization::field_data{
          member.name,
          member.name,
          member.field_id,
          0,
          0,
          false,
          false,
      });
    }

    error status = writer.begin_object(type_data);
    if (status) {
      return status;
    }

    for (std::size_t index = 0; index < value.as_object().size(); ++index) {
      const variant_member& member = value.as_object()[index];
      status = detail::validate_field_for_writer(type_data.fields[index], writer.features());
      if (status) {
        return status;
      }

      status = writer.begin_field(type_data.fields[index]);
      if (status) {
        return status;
      }

      status = write(writer, member.value);
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

} // namespace caors
