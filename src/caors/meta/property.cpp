// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/property.h"
#include "caors/meta/detail/argument_access.h"
#include "caors/meta/detail/property_record.h"
#include "caors/meta/detail/type_access.h"
#include "caors/meta/instance.h"
#include "caors/meta/metadata.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/capabilities/enum_capability.h"
#include "caors/meta/value/value.h"
#include "caors/meta/value_ops.h"

#include "detail/registry_state.h"

#include <optional>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] const meta::metadata_map& empty_metadata_map() noexcept {
  static const meta::metadata_map empty{
      typename meta::metadata_map::allocator_type{&memory::process_resource()}};
  return empty;
}

[[nodiscard]] detail::layer_call_guard
call_guard_for(const std::shared_ptr<detail::registry_layer_state>& layer) noexcept {
  return detail::layer_call_guard{layer};
}

[[nodiscard]] any_ref
bind_ref_to_layer(any_ref source,
                  const std::shared_ptr<detail::registry_layer_state>& layer) noexcept {
  if (!source || layer == nullptr) {
    return source;
  }

  const meta::type source_type = source.get_type();
  const meta::type bound_type =
      detail::type_access::make(detail::type_access::registry_of(source_type),
                                detail::type_access::data_of(source_type), layer);
  return detail::any_ref_access::from_erased(bound_type, source.data(), source.writable());
}

[[nodiscard]] const meta::property_schema& empty_property_schema() noexcept {
  static const meta::property_schema empty{};
  return empty;
}

[[nodiscard]] bool should_try_property_conversion(property_write_options options) noexcept {
  return options.conversion_policy != argument_conversion_policy::exact_only ||
         options.allow_custom_conversions;
}

template <typename Target>
[[nodiscard]] std::optional<any>
try_make_converted_property_any_for_type(meta::type expected, const meta::value& source,
                                         property_write_options options) {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  const meta::type actual = meta::resolve<target_type>();
  if (!expected || !actual || expected.id() != actual.id()) {
    return std::nullopt;
  }

  argument argument_value = source.to_argument();
  if (!argument_value) {
    return std::nullopt;
  }

  std::optional<argument> converted = detail::try_convert_argument<target_type>(
      argument_value, options.conversion_policy, options.allow_custom_conversions);
  if (!converted.has_value()) {
    return std::nullopt;
  }

  const target_type* value = converted->try_get<target_type>();
  return value != nullptr ? std::optional<any>{any::from(*value)} : std::nullopt;
}

template <typename... Targets>
[[nodiscard]] std::optional<any>
try_make_converted_property_any_from_known(meta::type expected, const meta::value& source,
                                           property_write_options options) {
  std::optional<any> result{};
  (([&] {
     if (!result.has_value()) {
       result = try_make_converted_property_any_for_type<Targets>(expected, source, options);
     }
   }()),
   ...);
  return result;
}

[[nodiscard]] bool try_set_enum_from_argument(const meta::enum_capability& capability,
                                              const meta::enum_view& target,
                                              const argument& argument_value) {
  const std::optional<std::string_view> text = detail::try_get_argument_string_view(argument_value);
  if (text.has_value()) {
    if (const meta::enum_value value = capability.find_value(*text)) {
      return capability.is_signed() ? target.from_i64(value.to_i64())
                                    : target.from_u64(value.to_u64());
    }

    if (capability.is_signed()) {
      std::int64_t parsed = 0;
      return detail::parse_integral_argument(*text, parsed) && target.from_i64(parsed);
    }

    std::uint64_t parsed = 0;
    return detail::parse_integral_argument(*text, parsed) && target.from_u64(parsed);
  }

  if (capability.is_signed()) {
    if (auto converted = detail::try_convert_numeric_argument<std::int64_t>(argument_value)) {
      const std::int64_t* value = converted->try_get<std::int64_t>();
      return value != nullptr && target.from_i64(*value);
    }
  } else if (auto converted = detail::try_convert_numeric_argument<std::uint64_t>(argument_value)) {
    const std::uint64_t* value = converted->try_get<std::uint64_t>();
    return value != nullptr && target.from_u64(*value);
  }

  return false;
}

[[nodiscard]] std::optional<any>
try_make_converted_enum_property_any(meta::type expected, const meta::value& source,
                                     property_write_options options) {
  if (!expected || expected.kind() != meta::type_kind::enumeration ||
      !should_try_property_conversion(options)) {
    return std::nullopt;
  }

  argument argument_value = source.to_argument();
  if (!argument_value) {
    return std::nullopt;
  }

  any target = any::default_construct(expected);
  const meta::enum_capability capability = expected.get_capability<meta::enum_capability>();
  const meta::enum_view view = capability.bind(target.ref());
  if (!view || !try_set_enum_from_argument(capability, view, argument_value)) {
    return std::nullopt;
  }

  return target;
}

[[nodiscard]] std::optional<any> try_make_converted_property_any(meta::type expected,
                                                                 const meta::value& source,
                                                                 property_write_options options) {
  if (std::optional<any> enum_value =
          try_make_converted_enum_property_any(expected, source, options)) {
    return enum_value;
  }

  return try_make_converted_property_any_from_known<
      bool, char, signed char, unsigned char, short, unsigned short, int, unsigned int, long,
      unsigned long, long long, unsigned long long, float, double, long double, core::string_name,
      core::string, std::string, std::string_view, core::guid, core::type_guid, meta::object*>(
      expected, source, options);
}

[[nodiscard]] bool try_set_converted_runtime_value(const detail::property_record& property,
                                                   void* object, const meta::value& source,
                                                   property_write_options options) {
  if (!should_try_property_conversion(options)) {
    return false;
  }

  std::optional<any> converted =
      try_make_converted_property_any(property.get_type(), source, options);
  return converted.has_value() && property.try_set_value(object, *converted);
}

} // namespace

property::property(const detail::property_record* property,
                   std::shared_ptr<detail::registry_layer_state> layer) noexcept
    : property_(property), layer_(std::move(layer)) {}

bool property::valid() const noexcept {
  return property_ != nullptr && (layer_ == nullptr || layer_->active()) && property_->valid();
}

core::string_name property::name() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->name() : core::string_name{};
}

meta::type property::get_owner_type() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->get_owner_type() : meta::type{};
}

meta::type property::get_type() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->get_type() : meta::type{};
}

meta::type property::get_value_type() const noexcept { return get_type().get_value_type(); }

meta::type property::get_element_type() const noexcept { return get_value_type(); }

meta::type property::get_key_type() const noexcept { return get_type().get_key_type(); }

meta::type property::get_first_type() const noexcept { return get_key_type(); }

meta::type property::get_mapped_type() const noexcept { return get_value_type(); }

meta::type property::get_second_type() const noexcept { return get_value_type(); }

meta::type_kind property::kind() const noexcept { return get_type().kind(); }

meta::wrapper_semantics property::wrapper_semantics() const noexcept {
  return get_type().wrapper();
}

bool property::indirect() const noexcept { return get_type().indirect(); }

bool property::nullable() const noexcept { return get_type().nullable(); }

bool property::writable() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr && property->writable();
}

const meta::property_schema& property::schema() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->schema() : empty_property_schema();
}

const meta::metadata_map& property::metadata() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->metadata() : empty_metadata_map();
}

const meta::metadata_value* property::find_metadata(meta::metadata_key key) const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->find_metadata(key) : nullptr;
}

std::size_t property::offset() const noexcept {
  const detail::property_record* property = info();
  return property != nullptr ? property->offset() : 0;
}

std::size_t property::size() const noexcept {
  const meta::type meta = get_type();
  const meta::value_storage_ops* value_ops = meta.get_raw_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.size : 0;
}

std::size_t property::alignment() const noexcept {
  const meta::type meta = get_type();
  const meta::value_storage_ops* value_ops = meta.get_raw_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.alignment : 0;
}

const detail::property_record* property::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return property_;
}

any property::get_any(const instance& object) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return {};
  }
  const detail::property_record* property = info();
  if (property == nullptr || !object) {
    return {};
  }

  return property->get_value(object.data());
}

any property::get_any() const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return {};
  }
  const detail::property_record* property = info();
  if (property == nullptr) {
    return {};
  }

  return property->get_value(nullptr);
}

meta::value property::get_value(const instance& object) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return {};
  }
  const detail::property_record* property = info();
  if (property == nullptr || !object) {
    return {};
  }
  return object.writable() ? property->get_runtime_value(object.mutable_data())
                           : property->get_runtime_value(object.data());
}

meta::value property::get_value() const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return {};
  }
  const detail::property_record* property = info();
  return property != nullptr ? property->get_runtime_value(static_cast<const void*>(nullptr))
                             : meta::value{};
}

any_ref property::get_ref(const instance& object) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return {};
  }
  const detail::property_record* property = info();
  if (property == nullptr || !object) {
    return {};
  }

  any_ref result = object.writable() ? property->get_ref(object.mutable_data())
                                     : property->get_ref(object.data());
  return bind_ref_to_layer(result, layer_);
}

any_ref property::get_ref() const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return {};
  }
  const detail::property_record* property = info();
  if (property == nullptr) {
    return {};
  }

  return bind_ref_to_layer(property->get_ref(static_cast<const void*>(nullptr)), layer_);
}

bool property::try_set_any(const instance& object, const any& source) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return false;
  }
  const detail::property_record* property = info();
  if (property == nullptr || !object.writable()) {
    return false;
  }

  return property->try_set_value(object.mutable_data(), source);
}

bool property::try_set_any(const any& source) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return false;
  }
  const detail::property_record* property = info();
  if (property == nullptr) {
    return false;
  }

  return property->try_set_value(nullptr, source);
}

bool property::try_set_value(const instance& object, const meta::value& source) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return false;
  }
  const detail::property_record* property = info();
  if (property == nullptr || !object.writable()) {
    return false;
  }
  return property->try_set_runtime_value(object.mutable_data(), source);
}

bool property::try_set_value(const instance& object, const meta::value& source,
                             property_write_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return false;
  }
  const detail::property_record* property = info();
  if (property == nullptr || !object.writable()) {
    return false;
  }
  void* target = object.mutable_data();
  return property->try_set_runtime_value(target, source) ||
         try_set_converted_runtime_value(*property, target, source, options);
}

bool property::try_set_value(const meta::value& source) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return false;
  }
  const detail::property_record* property = info();
  return property != nullptr && property->try_set_runtime_value(nullptr, source);
}

bool property::try_set_value(const meta::value& source, property_write_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return false;
  }
  const detail::property_record* property = info();
  return property != nullptr &&
         (property->try_set_runtime_value(nullptr, source) ||
          try_set_converted_runtime_value(*property, nullptr, source, options));
}

bool property::is_sequence() const noexcept { return kind() == meta::type_kind::sequence; }

bool property::is_set() const noexcept { return kind() == meta::type_kind::set; }

bool property::is_associative() const noexcept { return kind() == meta::type_kind::associative; }

bool property::is_pair() const noexcept { return kind() == meta::type_kind::pair; }

bool property::is_tuple() const noexcept { return kind() == meta::type_kind::tuple; }

bool property::is_variant() const noexcept { return kind() == meta::type_kind::variant; }

} // namespace caors::meta
