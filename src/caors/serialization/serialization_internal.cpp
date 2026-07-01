// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_internal.h"

#include "caors/reflection/registry.h"

#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace caors::detail {
namespace {

template <typename T>
[[nodiscard]] bool try_write_i64(serialization_writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_i64(static_cast<std::int64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_u64(serialization_writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_u64(static_cast<std::uint64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_f64(serialization_writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_f64(static_cast<double>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_read_signed_value(serialization_reader& reader, const any_ref& target) {
  std::int64_t value = 0;
  if (reader.read_i64(value).succeeded()) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return target.try_set<T>(static_cast<T>(value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).failed() ||
      unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return target.try_set<T>(static_cast<T>(unsigned_value));
}

template <typename T>
[[nodiscard]] bool try_read_unsigned_value(serialization_reader& reader, const any_ref& target) {
  std::uint64_t value = 0;
  if (reader.read_u64(value).succeeded()) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return target.try_set<T>(static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).failed() || signed_value < 0 ||
      static_cast<std::uint64_t>(signed_value) >
          static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return target.try_set<T>(static_cast<T>(signed_value));
}

template <typename T>
[[nodiscard]] bool try_read_float_value(serialization_reader& reader, const any_ref& target) {
  double value = 0.0;
  if (reader.read_f64(value).succeeded()) {
    return target.try_set<T>(static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return target.try_set<T>(static_cast<T>(signed_value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return target.try_set<T>(static_cast<T>(unsigned_value));
  }

  return false;
}

[[nodiscard]] bool try_read_char_value(serialization_reader& reader, const any_ref& target) {
  string value;
  if (reader.read_string(value).failed() || value.size() != 1) {
    return false;
  }

  return target.try_set<char>(value[0]);
}

[[nodiscard]] error read_size_value(serialization_reader& reader, std::size_t& value) {
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

[[nodiscard]] bool field_uses_inline_indirect_value(const reflection_field& field) noexcept {
  return (field.kind() == type_kind::raw_pointer || field.kind() == type_kind::wrapper) &&
         field.wrapper_semantics() == wrapper_semantics::inline_value;
}

[[nodiscard]] bool field_uses_direct_value(const reflection_field& field) noexcept {
  return field.kind() == type_kind::value || field.kind() == type_kind::enumeration ||
         field.kind() == type_kind::sequence || field.kind() == type_kind::set ||
         field.kind() == type_kind::associative || field.kind() == type_kind::pair ||
         field.kind() == type_kind::tuple || field.kind() == type_kind::variant;
}

[[nodiscard]] const meta_type_semantics_data* type_semantics(type_id id) noexcept {
  return get_meta_registry().find_ptr<meta_type_semantics_data>(id);
}

[[nodiscard]] bool type_uses_inline_indirect_value(type_id id) noexcept {
  const meta_type_semantics_data* semantics = type_semantics(id);
  return semantics != nullptr &&
         (semantics->kind == type_kind::raw_pointer || semantics->kind == type_kind::wrapper) &&
         semantics->wrapper_semantics == wrapper_semantics::inline_value;
}

[[nodiscard]] bool type_is_enumeration(type_id id) noexcept {
  const meta_type_semantics_data* semantics = type_semantics(id);
  return semantics != nullptr && semantics->kind == type_kind::enumeration;
}

[[nodiscard]] bool type_uses_object_ref(type_id id) noexcept {
  const meta_type_semantics_data* semantics = type_semantics(id);
  return semantics != nullptr && semantics->kind == type_kind::wrapper &&
         semantics->wrapper_semantics == wrapper_semantics::object_ref;
}

[[nodiscard]] const object*
object_ref_value_from_ref(const any_ref& value,
                          const meta_type_semantics_data& semantics) noexcept {
  if (!value.valid() || value.data() == nullptr || semantics.get_object_ref == nullptr) {
    return nullptr;
  }

  return semantics.get_object_ref(value.data());
}

[[nodiscard]] variant_result make_object_ref_value(const any_ref& value,
                                                   const serialization_context& context) {
  const meta_type_semantics_data* semantics = type_semantics(value.type());
  if (semantics == nullptr || semantics->wrapper_semantics != wrapper_semantics::object_ref ||
      semantics->get_object_ref == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  const object* referenced = object_ref_value_from_ref(value, *semantics);
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return {{}, variant::from(id.value())};
}

[[nodiscard]] error write_object_ref_value(serialization_writer& writer, const any_ref& value,
                                           const serialization_context& context) {
  const meta_type_semantics_data* semantics = type_semantics(value.type());
  if (semantics == nullptr || semantics->wrapper_semantics != wrapper_semantics::object_ref ||
      semantics->get_object_ref == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const object* referenced = object_ref_value_from_ref(value, *semantics);
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {error_domain::serialization, errc::not_found};
  }

  return writer.write_u64(id.value());
}

[[nodiscard]] error read_object_ref_value(serialization_reader& reader, const any_ref& target,
                                          const serialization_context& context) {
  const meta_type_semantics_data* semantics = type_semantics(target.type());
  if (semantics == nullptr || semantics->wrapper_semantics != wrapper_semantics::object_ref ||
      semantics->set_object_ref == nullptr || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (context.reference_resolver == nullptr || !context.reference_resolver->can_read()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  std::uint64_t encoded_id = 0;
  error status = reader.read_u64(encoded_id);
  if (status) {
    return status;
  }

  object* referenced = nullptr;
  if (encoded_id != 0) {
    referenced = context.reference_resolver->id_to_object(object_id{encoded_id},
                                                          context.reference_resolver->user_data);
    if (referenced == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }
  }

  if (!semantics->set_object_ref(target.mutable_data(), referenced)) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return {};
}

[[nodiscard]] variant_result make_enum_value(const any_ref& value) {
  const meta_type_semantics_data* semantics = type_semantics(value.type());
  if (semantics == nullptr || semantics->kind != type_kind::enumeration ||
      value.data() == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (semantics->enum_signed) {
    if (semantics->enum_to_i64 == nullptr) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return {{}, variant::from(semantics->enum_to_i64(value.data()))};
  }

  if (semantics->enum_to_u64 == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, variant::from(semantics->enum_to_u64(value.data()))};
}

[[nodiscard]] error write_enum_value(serialization_writer& writer, const any_ref& value) {
  const meta_type_semantics_data* semantics = type_semantics(value.type());
  if (semantics == nullptr || semantics->kind != type_kind::enumeration ||
      value.data() == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (semantics->enum_signed) {
    if (semantics->enum_to_i64 == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    return writer.write_i64(semantics->enum_to_i64(value.data()));
  }

  if (semantics->enum_to_u64 == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return writer.write_u64(semantics->enum_to_u64(value.data()));
}

[[nodiscard]] error read_enum_value(serialization_reader& reader, const any_ref& target) {
  const meta_type_semantics_data* semantics = type_semantics(target.type());
  if (semantics == nullptr || semantics->kind != type_kind::enumeration ||
      target.mutable_data() == nullptr || semantics->enum_from_i64 == nullptr ||
      semantics->enum_from_u64 == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (semantics->enum_signed) {
    std::int64_t signed_value = 0;
    if (reader.read_i64(signed_value).succeeded()) {
      return semantics->enum_from_i64(target.mutable_data(), signed_value)
                 ? error{}
                 : error{error_domain::serialization, errc::invalid_argument};
    }

    std::uint64_t unsigned_value = 0;
    if (reader.read_u64(unsigned_value).succeeded()) {
      return semantics->enum_from_u64(target.mutable_data(), unsigned_value)
                 ? error{}
                 : error{error_domain::serialization, errc::invalid_argument};
    }

    return {error_domain::serialization, errc::unsupported};
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return semantics->enum_from_u64(target.mutable_data(), unsigned_value)
               ? error{}
               : error{error_domain::serialization, errc::invalid_argument};
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return semantics->enum_from_i64(target.mutable_data(), signed_value)
               ? error{}
               : error{error_domain::serialization, errc::invalid_argument};
  }

  return {error_domain::serialization, errc::unsupported};
}

[[nodiscard]] variant_result make_indirect_serialized_value(const indirect_view& indirect,
                                                            const serialization_context& context) {
  if (!indirect.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (!indirect.has_value()) {
    return {{}, variant::null()};
  }

  const any_ref value = indirect.value_ref();
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_serialized_value(value, context);
}

[[nodiscard]] error write_indirect_serialized_value(serialization_writer& writer,
                                                    const indirect_view& indirect,
                                                    const serialization_context& context) {
  if (!indirect.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!indirect.has_value()) {
    return writer.write_null();
  }

  const any_ref value = indirect.value_ref();
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  return write_serialized_value(writer, value, context);
}

[[nodiscard]] error read_indirect_serialized_value(serialization_reader& reader,
                                                   const indirect_view& indirect,
                                                   const serialization_context& context) {
  if (!indirect.valid() || !indirect.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (reader.is_null()) {
    if (!indirect.reset()) {
      return {error_domain::serialization, errc::invalid_field};
    }

    return {};
  }

  if (!indirect.has_value() && !indirect.ensure_value()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  any_ref value = indirect.value_ref();
  if (!value.valid() || !value.writable() || value.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return read_serialized_value(reader, value, context);
}

[[nodiscard]] error write_sequence_value(serialization_writer& writer, const sequence_view& view,
                                         const serialization_context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(*size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < *size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = write_serialized_value(writer, element, context);
    if (status) {
      return status;
    }

    status = writer.end_element();
    if (status) {
      return status;
    }
  }

  return writer.end_array();
}

[[nodiscard]] error write_set_value(serialization_writer& writer, const set_view& view,
                                    const serialization_context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(*size);
  if (status) {
    return status;
  }

  struct visitor_context {
    serialization_writer* writer = nullptr;
    const serialization_context* context = nullptr;
    std::size_t index = 0;
    error status{};
  } visitor{&writer, &context, 0, {}};

  const bool completed = view.for_each_ref(
      [](any_ref element, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        visitor->status = visitor->writer->begin_element(visitor->index);
        if (visitor->status) {
          return false;
        }

        visitor->status = write_serialized_value(*visitor->writer, element, *visitor->context);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_element();
        if (visitor->status) {
          return false;
        }

        ++visitor->index;
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.status ? visitor.status : error{error_domain::serialization, errc::unsupported};
  }

  return writer.end_array();
}

[[nodiscard]] error write_associative_value(serialization_writer& writer,
                                            const associative_view& view,
                                            const serialization_context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_dictionary(*size);
  if (status) {
    return status;
  }

  struct visitor_context {
    serialization_writer* writer = nullptr;
    const serialization_context* context = nullptr;
    std::size_t index = 0;
    error status{};
  } visitor{&writer, &context, 0, {}};

  const bool completed = view.for_each_ref(
      [](any_ref key, any_ref value, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        visitor->status = visitor->writer->begin_entry(visitor->index);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->begin_key();
        if (visitor->status) {
          return false;
        }

        visitor->status = write_serialized_value(*visitor->writer, key, *visitor->context);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_key();
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->begin_value();
        if (visitor->status) {
          return false;
        }

        visitor->status = write_serialized_value(*visitor->writer, value, *visitor->context);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_value();
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_entry();
        if (visitor->status) {
          return false;
        }

        ++visitor->index;
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.status ? visitor.status : error{error_domain::serialization, errc::unsupported};
  }

  return writer.end_dictionary();
}

[[nodiscard]] error write_pair_value(serialization_writer& writer, const pair_view& view,
                                     const serialization_context& context) {
  if (!view.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(2);
  if (status) {
    return status;
  }

  status = writer.begin_element(0);
  if (status) {
    return status;
  }

  any_ref first = view.first_ref();
  if (!first.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = write_serialized_value(writer, first, context);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  status = writer.begin_element(1);
  if (status) {
    return status;
  }

  any_ref second = view.second_ref();
  if (!second.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = write_serialized_value(writer, second, context);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  return writer.end_array();
}

[[nodiscard]] error write_tuple_value(serialization_writer& writer, const tuple_view& view,
                                      const serialization_context& context) {
  if (!view.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  const std::size_t size = view.size();
  error status = writer.begin_array(size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = write_serialized_value(writer, element, context);
    if (status) {
      return status;
    }

    status = writer.end_element();
    if (status) {
      return status;
    }
  }

  return writer.end_array();
}

[[nodiscard]] error write_variant_value(serialization_writer& writer, const variant_view& view,
                                        const serialization_context& context) {
  const std::optional<std::size_t> active = view.index();
  if (!view.valid() || !active.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(2);
  if (status) {
    return status;
  }

  status = writer.begin_element(0);
  if (status) {
    return status;
  }

  status = writer.write_u64(static_cast<std::uint64_t>(*active));
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  status = writer.begin_element(1);
  if (status) {
    return status;
  }

  any_ref value = view.value_ref();
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = write_serialized_value(writer, value, context);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  return writer.end_array();
}

[[nodiscard]] error read_sequence_into_view(serialization_reader& reader, const sequence_view& view,
                                            const serialization_context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (!view.clear() || !view.resize(size)) {
    return {error_domain::serialization, errc::unsupported};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid() || !element.writable()) {
      return {error_domain::serialization, errc::invalid_field};
    }

    status = read_serialized_value(reader, element, context);
    if (status) {
      return status;
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] error read_sequence_value(serialization_reader& reader, const any_ref& target,
                                        const serialization_context& context) {
  const meta_sequence_data* sequence =
      get_meta_registry().find_ptr<meta_sequence_data>(target.type());
  if (sequence != nullptr && !sequence->can_read) {
    return {error_domain::serialization, errc::unsupported};
  }

  const std::optional<meta_type> meta = find_type(target.type());
  if (sequence != nullptr && !sequence->read_in_place && meta.has_value() &&
      meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_sequence_into_view(reader, decoded.create_sequence_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_sequence_into_view(reader, target.create_sequence_view(), context);
}

[[nodiscard]] error read_set_into_view(serialization_reader& reader, const set_view& view,
                                       const serialization_context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (!view.clear()) {
    return {error_domain::serialization, errc::unsupported};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    any element = any::default_construct(view.value_type());
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = read_serialized_value(reader, element.ref(), context);
    if (status) {
      return status;
    }

    if (!view.insert_value(element)) {
      return {error_domain::serialization, errc::already_exists};
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] error read_set_value(serialization_reader& reader, const any_ref& target,
                                   const serialization_context& context) {
  const std::optional<meta_type> meta = find_type(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_set_into_view(reader, decoded.create_set_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_set_into_view(reader, target.create_set_view(), context);
}

[[nodiscard]] error read_associative_into_view(serialization_reader& reader,
                                               const associative_view& view,
                                               const serialization_context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_dictionary(size);
  if (status) {
    return status;
  }

  if (!view.clear()) {
    return {error_domain::serialization, errc::unsupported};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_entry(index);
    if (status) {
      return status;
    }

    any key = any::default_construct(view.key_type());
    any value = any::default_construct(view.value_type());
    if (!key.valid() || !value.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = reader.begin_key();
    if (status) {
      return status;
    }

    status = read_serialized_value(reader, key.ref(), context);
    if (status) {
      return status;
    }

    status = reader.end_key();
    if (status) {
      return status;
    }

    status = reader.begin_value();
    if (status) {
      return status;
    }

    status = read_serialized_value(reader, value.ref(), context);
    if (status) {
      return status;
    }

    status = reader.end_value();
    if (status) {
      return status;
    }

    if (!view.insert_value(key, value)) {
      return {error_domain::serialization, errc::already_exists};
    }

    status = reader.end_entry();
    if (status) {
      return status;
    }
  }

  return reader.end_dictionary();
}

[[nodiscard]] error read_associative_value(serialization_reader& reader, const any_ref& target,
                                           const serialization_context& context) {
  const std::optional<meta_type> meta = find_type(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_associative_into_view(reader, decoded.create_associative_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_associative_into_view(reader, target.create_associative_view(), context);
}

[[nodiscard]] error read_pair_into_view(serialization_reader& reader, const pair_view& view,
                                        const serialization_context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != 2) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.begin_element(0);
  if (status) {
    return status;
  }

  any_ref first = view.first_ref();
  if (!first.valid() || !first.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = read_serialized_value(reader, first, context);
  if (status) {
    return status;
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  status = reader.begin_element(1);
  if (status) {
    return status;
  }

  any_ref second = view.second_ref();
  if (!second.valid() || !second.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = read_serialized_value(reader, second, context);
  if (status) {
    return status;
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  return reader.end_array();
}

[[nodiscard]] error read_pair_value(serialization_reader& reader, const any_ref& target,
                                    const serialization_context& context) {
  const std::optional<meta_type> meta = find_type(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_pair_into_view(reader, decoded.create_pair_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_pair_into_view(reader, target.create_pair_view(), context);
}

[[nodiscard]] error read_tuple_into_view(serialization_reader& reader, const tuple_view& view,
                                         const serialization_context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != view.size()) {
    return {error_domain::serialization, errc::unsupported};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid() || !element.writable()) {
      return {error_domain::serialization, errc::invalid_field};
    }

    status = read_serialized_value(reader, element, context);
    if (status) {
      return status;
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] error read_tuple_value(serialization_reader& reader, const any_ref& target,
                                     const serialization_context& context) {
  const std::optional<meta_type> meta = find_type(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_tuple_into_view(reader, decoded.create_tuple_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_tuple_into_view(reader, target.create_tuple_view(), context);
}

[[nodiscard]] error read_variant_into_view(serialization_reader& reader, const variant_view& view,
                                           const serialization_context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != 2) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.begin_element(0);
  if (status) {
    return status;
  }

  std::size_t active = 0;
  status = read_size_value(reader, active);
  if (status) {
    return status;
  }

  if (active >= view.size()) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  if (!view.emplace(active)) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.begin_element(1);
  if (status) {
    return status;
  }

  any_ref value = view.value_ref();
  if (!value.valid() || !value.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = read_serialized_value(reader, value, context);
  if (status) {
    return status;
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  return reader.end_array();
}

[[nodiscard]] error read_variant_value(serialization_reader& reader, const any_ref& target,
                                       const serialization_context& context) {
  const std::optional<meta_type> meta = find_type(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_variant_into_view(reader, decoded.create_variant_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_variant_into_view(reader, target.create_variant_view(), context);
}

[[nodiscard]] variant_result make_sequence_value(const sequence_view& view,
                                                 const serialization_context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  elements.reserve(*size);
  for (std::size_t index = 0; index < *size; ++index) {
    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    variant_result encoded = make_serialized_value(element, context);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_set_value(const set_view& view,
                                            const serialization_context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  elements.reserve(*size);

  struct visitor_context {
    variant::array_type* elements = nullptr;
    const serialization_context* context = nullptr;
    variant_result result{};
  } visitor{&elements, &context, {}};

  const bool completed = view.for_each_ref(
      [](any_ref element, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        variant_result encoded = make_serialized_value(element, *visitor->context);
        if (encoded.failed()) {
          visitor->result = encoded;
          return false;
        }

        visitor->elements->push_back(std::move(encoded.value));
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.result.failed()
               ? visitor.result
               : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_associative_value(const associative_view& view,
                                                    const serialization_context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::dictionary_type entries;
  entries.reserve(*size);

  struct visitor_context {
    variant::dictionary_type* entries = nullptr;
    const serialization_context* context = nullptr;
    variant_result result{};
  } visitor{&entries, &context, {}};

  const bool completed = view.for_each_ref(
      [](any_ref key, any_ref value, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        variant_result encoded_key = make_serialized_value(key, *visitor->context);
        if (encoded_key.failed()) {
          visitor->result = encoded_key;
          return false;
        }

        variant_result encoded_value = make_serialized_value(value, *visitor->context);
        if (encoded_value.failed()) {
          visitor->result = encoded_value;
          return false;
        }

        visitor->entries->push_back(
            variant_entry{std::move(encoded_key.value), std::move(encoded_value.value)});
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.result.failed()
               ? visitor.result
               : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, variant::dictionary(std::move(entries))};
}

[[nodiscard]] variant_result make_pair_value(const pair_view& view,
                                             const serialization_context& context) {
  if (!view.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  any_ref first = view.first_ref();
  any_ref second = view.second_ref();
  if (!first.valid() || !second.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant_result encoded_first = make_serialized_value(first, context);
  if (encoded_first.failed()) {
    return encoded_first;
  }

  variant_result encoded_second = make_serialized_value(second, context);
  if (encoded_second.failed()) {
    return encoded_second;
  }

  variant::array_type elements;
  elements.reserve(2);
  elements.push_back(std::move(encoded_first.value));
  elements.push_back(std::move(encoded_second.value));

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_tuple_value(const tuple_view& view,
                                              const serialization_context& context) {
  if (!view.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  const std::size_t size = view.size();
  elements.reserve(size);
  for (std::size_t index = 0; index < size; ++index) {
    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    variant_result encoded = make_serialized_value(element, context);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_variant_value(const variant_view& view,
                                                const serialization_context& context) {
  const std::optional<std::size_t> active = view.index();
  if (!view.valid() || !active.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  any_ref value = view.value_ref();
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant_result encoded = make_serialized_value(value, context);
  if (encoded.failed()) {
    return encoded;
  }

  variant::array_type elements;
  elements.reserve(2);
  elements.push_back(variant::from(static_cast<std::uint64_t>(*active)));
  elements.push_back(std::move(encoded.value));

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] error write_object_ref_field(serialization_writer& writer,
                                           const reflection_field& field, const void* object,
                                           const serialization_field_data& field_data,
                                           const serialization_context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  error status = validate_field_for_writer(field_data, writer.features());
  if (status) {
    return status;
  }

  status = writer.begin_field(field_data);
  if (status) {
    return status;
  }

  const caors::object* referenced = field.get_object_ref(object);
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {error_domain::serialization, errc::not_found};
  }

  status = writer.write_u64(id.value());
  if (status) {
    return status;
  }

  return writer.end_field();
}

[[nodiscard]] error read_object_ref_field(serialization_reader& reader,
                                          const reflection_field& field, void* object,
                                          const serialization_field_data& field_data,
                                          const serialization_context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_read()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  error status = validate_field_for_reader(field_data, reader.features());
  if (status) {
    return status;
  }

  status = reader.begin_field(field_data);
  if (status) {
    return status;
  }

  std::uint64_t encoded_id = 0;
  status = reader.read_u64(encoded_id);
  if (status) {
    return status;
  }

  caors::object* referenced = nullptr;
  if (encoded_id != 0) {
    referenced = context.reference_resolver->id_to_object(object_id{encoded_id},
                                                          context.reference_resolver->user_data);
    if (referenced == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }
  }

  if (!field.try_set_object_ref(object, referenced)) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return reader.end_field();
}

} // namespace

string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

bool field_visible_in_context(const serialization_field_data& field,
                              const serialization_context& context) noexcept {
  if (context.schema_version == 0) {
    return true;
  }

  if (field.version_added != 0 && context.schema_version < field.version_added) {
    return false;
  }

  if (field.version_removed != 0 && context.schema_version >= field.version_removed) {
    return false;
  }

  return true;
}

error validate_field_for_writer(const serialization_field_data& field,
                                serialization_format_features features) {
  if (has_feature(features, serialization_format_features::named_fields) &&
      serialized_field_name(field).empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (has_feature(features, serialization_format_features::numeric_fields) && field.field_id == 0) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

error validate_field_for_reader(const serialization_field_data& field,
                                serialization_format_features features) {
  const bool can_use_name = has_feature(features, serialization_format_features::named_fields) &&
                            !serialized_field_name(field).empty();
  const bool can_use_id =
      has_feature(features, serialization_format_features::numeric_fields) && field.field_id != 0;

  if (!can_use_name && !can_use_id) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

bool try_write_serialized_value(serialization_writer& writer, const any& data) {
  if (const bool* stored = data.try_get<bool>()) {
    return writer.write_bool(*stored).succeeded();
  }

  if (const char* stored = data.try_get<char>()) {
    return writer.write_string(std::string_view{stored, 1}).succeeded();
  }

  if (try_write_i64<signed char>(writer, data) || try_write_i64<short>(writer, data) ||
      try_write_i64<int>(writer, data) || try_write_i64<long>(writer, data) ||
      try_write_i64<long long>(writer, data) || try_write_u64<unsigned char>(writer, data) ||
      try_write_u64<unsigned short>(writer, data) || try_write_u64<unsigned int>(writer, data) ||
      try_write_u64<unsigned long>(writer, data) ||
      try_write_u64<unsigned long long>(writer, data) || try_write_f64<float>(writer, data) ||
      try_write_f64<double>(writer, data) || try_write_f64<long double>(writer, data)) {
    return true;
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return writer.write_string(*stored).succeeded();
  }

  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return writer.write_string(*stored).succeeded();
  }

  return false;
}

variant_result make_serialized_value(const any& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const meta_type_semantics_data* semantics = type_semantics(data.type());
  if (semantics != nullptr &&
      (semantics->kind == type_kind::raw_pointer || semantics->kind == type_kind::wrapper ||
       semantics->kind == type_kind::enumeration || semantics->kind == type_kind::sequence ||
       semantics->kind == type_kind::set || semantics->kind == type_kind::associative ||
       semantics->kind == type_kind::pair || semantics->kind == type_kind::tuple ||
       semantics->kind == type_kind::variant)) {
    return make_serialized_value(data.ref(), serialization_context{});
  }

  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(data.type());
  if (codec.has_value() && codec->make_value != nullptr) {
    return codec->make_value(data);
  }

  if (const bool* stored = data.try_get<bool>()) {
    return {{}, variant::from(*stored)};
  }

  if (const char* stored = data.try_get<char>()) {
    return {{}, variant::from(std::string_view{stored, 1})};
  }

  if (const signed char* stored = data.try_get<signed char>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const short* stored = data.try_get<short>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const int* stored = data.try_get<int>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long* stored = data.try_get<long>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long long* stored = data.try_get<long long>()) {
    return {{}, variant::from(static_cast<std::int64_t>(*stored))};
  }

  if (const unsigned char* stored = data.try_get<unsigned char>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned short* stored = data.try_get<unsigned short>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned int* stored = data.try_get<unsigned int>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long* stored = data.try_get<unsigned long>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long long* stored = data.try_get<unsigned long long>()) {
    return {{}, variant::from(static_cast<std::uint64_t>(*stored))};
  }

  if (const float* stored = data.try_get<float>()) {
    return {{}, variant::from(static_cast<double>(*stored))};
  }
  if (const double* stored = data.try_get<double>()) {
    return {{}, variant::from(*stored)};
  }
  if (const long double* stored = data.try_get<long double>()) {
    return {{}, variant::from(static_cast<double>(*stored))};
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return {{}, variant::from(*stored)};
  }
  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return {{}, variant::from(*stored)};
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

variant_result make_serialized_value(const any_ref& value, const serialization_context& context) {
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (type_uses_inline_indirect_value(value.type())) {
    const indirect_view indirect = value.create_indirect_view();
    return make_indirect_serialized_value(indirect, context);
  }

  if (type_uses_object_ref(value.type())) {
    return make_object_ref_value(value, context);
  }

  if (type_is_enumeration(value.type())) {
    return make_enum_value(value);
  }

  sequence_view sequence = value.create_sequence_view();
  if (sequence.valid()) {
    return make_sequence_value(sequence, context);
  }

  set_view set = value.create_set_view();
  if (set.valid()) {
    return make_set_value(set, context);
  }

  associative_view associative = value.create_associative_view();
  if (associative.valid()) {
    return make_associative_value(associative, context);
  }

  pair_view pair = value.create_pair_view();
  if (pair.valid()) {
    return make_pair_value(pair, context);
  }

  tuple_view tuple = value.create_tuple_view();
  if (tuple.valid()) {
    return make_tuple_value(tuple, context);
  }

  variant_view variant = value.create_variant_view();
  if (variant.valid()) {
    return make_variant_value(variant, context);
  }

  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(value.type());
  if (codec.has_value() && codec->make_value != nullptr) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return codec->make_value(copied);
  }

  const std::optional<reflection_type> nested_type = find_reflection(value.type());
  if (nested_type.has_value() && get_serialization_registry().contains(value.type())) {
    return to_serialization_value(*nested_type, value.data(), context);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return make_serialized_value(copied);
}

error write_serialized_value(serialization_writer& writer, const any_ref& value,
                             const serialization_context& context) {
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (type_uses_inline_indirect_value(value.type())) {
    const indirect_view indirect = value.create_indirect_view();
    return write_indirect_serialized_value(writer, indirect, context);
  }

  if (type_uses_object_ref(value.type())) {
    return write_object_ref_value(writer, value, context);
  }

  if (type_is_enumeration(value.type())) {
    return write_enum_value(writer, value);
  }

  sequence_view sequence = value.create_sequence_view();
  if (sequence.valid()) {
    return write_sequence_value(writer, sequence, context);
  }

  set_view set = value.create_set_view();
  if (set.valid()) {
    return write_set_value(writer, set, context);
  }

  associative_view associative = value.create_associative_view();
  if (associative.valid()) {
    return write_associative_value(writer, associative, context);
  }

  pair_view pair = value.create_pair_view();
  if (pair.valid()) {
    return write_pair_value(writer, pair, context);
  }

  tuple_view tuple = value.create_tuple_view();
  if (tuple.valid()) {
    return write_tuple_value(writer, tuple, context);
  }

  variant_view variant = value.create_variant_view();
  if (variant.valid()) {
    return write_variant_value(writer, variant, context);
  }

  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(value.type());
  if (codec.has_value() && codec->write != nullptr) {
    any copied = value.to_any();
    if (!copied.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return codec->write(writer, copied);
  }

  const std::optional<reflection_type> nested_type = find_reflection(value.type());
  if (nested_type.has_value() && get_serialization_registry().contains(value.type())) {
    return serialize(*nested_type, value.data(), writer, context);
  }

  any copied = value.to_any();
  if (!copied.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!try_write_serialized_value(writer, copied)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

error read_serialized_value(serialization_reader& reader, const any_ref& target,
                            const serialization_context& context) {
  if (!target.valid() || !target.writable() || target.mutable_data() == nullptr) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (type_uses_inline_indirect_value(target.type())) {
    const indirect_view indirect = target.create_indirect_view();
    return read_indirect_serialized_value(reader, indirect, context);
  }

  if (type_uses_object_ref(target.type())) {
    return read_object_ref_value(reader, target, context);
  }

  if (type_is_enumeration(target.type())) {
    return read_enum_value(reader, target);
  }

  if (target.create_sequence_view().valid()) {
    return read_sequence_value(reader, target, context);
  }

  if (target.create_set_view().valid()) {
    return read_set_value(reader, target, context);
  }

  if (target.create_associative_view().valid()) {
    return read_associative_value(reader, target, context);
  }

  if (target.create_pair_view().valid()) {
    return read_pair_value(reader, target, context);
  }

  if (target.create_tuple_view().valid()) {
    return read_tuple_value(reader, target, context);
  }

  if (target.create_variant_view().valid()) {
    return read_variant_value(reader, target, context);
  }

  const std::optional<serialization_codec_data> codec =
      get_serialization_codec_registry().find_codec(target.type());
  if (codec.has_value() && codec->read_value != nullptr) {
    return codec->read_value(reader, target);
  }

  const std::optional<reflection_type> nested_type = find_reflection(target.type());
  if (nested_type.has_value() && get_serialization_registry().contains(target.type())) {
    return deserialize(*nested_type, target.mutable_data(), reader, context);
  }

  bool bool_value = false;
  bool written = reader.read_bool(bool_value).succeeded() && target.try_set<bool>(bool_value);

  if (!written) {
    written = try_read_char_value(reader, target);
  }

  if (!written) {
    written = try_read_signed_value<signed char>(reader, target) ||
              try_read_signed_value<short>(reader, target) ||
              try_read_signed_value<int>(reader, target) ||
              try_read_signed_value<long>(reader, target) ||
              try_read_signed_value<long long>(reader, target);
  }

  if (!written) {
    written = try_read_unsigned_value<unsigned char>(reader, target) ||
              try_read_unsigned_value<unsigned short>(reader, target) ||
              try_read_unsigned_value<unsigned int>(reader, target) ||
              try_read_unsigned_value<unsigned long>(reader, target) ||
              try_read_unsigned_value<unsigned long long>(reader, target);
  }

  if (!written) {
    written = try_read_float_value<float>(reader, target) ||
              try_read_float_value<double>(reader, target) ||
              try_read_float_value<long double>(reader, target);
  }

  if (!written) {
    string string_value;
    if (reader.read_string(string_value).succeeded()) {
      const std::string standard_string{string_value.data(), string_value.size()};
      written =
          target.try_set<string>(string_value) || target.try_set<std::string>(standard_string);
    }
  }

  if (!written) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

error write_field(serialization_writer& writer, const reflection_field& field, const void* object,
                  const serialization_field_data& field_data,
                  const serialization_context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.wrapper_semantics() == wrapper_semantics::object_ref) {
    return write_object_ref_field(writer, field, object, field_data, context);
  }

  const bool uses_indirect_value = field_uses_inline_indirect_value(field);
  any_ref field_value{};
  if (field_uses_direct_value(field)) {
    field_value = field.get_ref(object);
  } else if (!uses_indirect_value) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!uses_indirect_value && !field_value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = validate_field_for_writer(field_data, writer.features());
  if (status) {
    return status;
  }

  status = writer.begin_field(field_data);
  if (status) {
    return status;
  }

  if (uses_indirect_value) {
    status = write_indirect_serialized_value(writer, field.create_indirect_view(object), context);
  } else {
    status = write_serialized_value(writer, field_value, context);
  }

  if (status) {
    return status;
  }

  return writer.end_field();
}

error read_field(serialization_reader& reader, const reflection_field& field, void* object,
                 const serialization_field_data& field_data, const serialization_context& context) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (!field.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.wrapper_semantics() == wrapper_semantics::object_ref) {
    return read_object_ref_field(reader, field, object, field_data, context);
  }

  error status = validate_field_for_reader(field_data, reader.features());
  if (status) {
    return status;
  }

  const bool uses_indirect_value = field_uses_inline_indirect_value(field);
  any_ref target{};
  bool use_legacy_codec_read = false;
  if (field_uses_direct_value(field)) {
    const std::optional<serialization_codec_data> codec =
        get_serialization_codec_registry().find_codec(field.type());
    use_legacy_codec_read =
        codec.has_value() && codec->read != nullptr && codec->read_value == nullptr;
    target = field.get_ref(object);
  } else if (!uses_indirect_value) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!uses_indirect_value && !use_legacy_codec_read &&
      (!target.valid() || !target.writable() || target.mutable_data() == nullptr)) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = reader.begin_field(field_data);
  if (status) {
    return status;
  }

  if (uses_indirect_value) {
    status = read_indirect_serialized_value(reader, field.create_indirect_view(object), context);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  if (use_legacy_codec_read) {
    const std::optional<serialization_codec_data> codec =
        get_serialization_codec_registry().find_codec(field.type());
    if (!codec.has_value() || codec->read == nullptr) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = codec->read(reader, field, object);
    if (status) {
      return status;
    }

    return reader.end_field();
  }

  status = read_serialized_value(reader, target, context);
  if (status) {
    return status;
  }

  return reader.end_field();
}

error read_configured_field(serialization_reader& reader, const reflection_field& field,
                            void* object, const serialization_field_data& field_data,
                            const serialization_context& context) {
  const error status = read_field(reader, field, object, field_data, context);
  if (status.value() == errc::not_found && !field_data.required) {
    return {};
  }

  return status;
}

} // namespace caors::detail
