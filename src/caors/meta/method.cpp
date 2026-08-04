// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/method.h"

#include "caors/core/containers.h"
#include "caors/meta/detail/method_access.h"
#include "caors/meta/detail/method_record.h"
#include "caors/meta/instance.h"

#include "detail/registry_state.h"

#include <optional>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::process_resource()}};
  return empty;
}

[[nodiscard]] detail::layer_call_guard
call_guard_for(const std::shared_ptr<detail::registry_layer_state>& layer) noexcept {
  return detail::layer_call_guard{layer};
}

} // namespace

invoke_result invoke_result::value(any value) {
  if (!value) {
    return failed();
  }

  invoke_result result{};
  result.kind_ = invoke_result_kind::value;
  result.value_ = std::move(value);
  return result;
}

invoke_result invoke_result::reference(any_ref ref) noexcept {
  if (!ref) {
    return failed();
  }

  invoke_result result{};
  result.kind_ = invoke_result_kind::reference;
  result.ref_ = ref;
  return result;
}

method::method(const detail::method_record* method,
               std::shared_ptr<detail::registry_layer_state> layer) noexcept
    : method_(method), layer_(std::move(layer)) {}

bool method::valid() const noexcept {
  return method_ != nullptr && (layer_ == nullptr || layer_->active()) && method_->valid();
}

core::string_name method::name() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->name() : core::string_name{};
}

meta::type method::get_owner_type() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->get_owner_type() : meta::type{};
}

meta::type method::get_return_type() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->get_return_type() : meta::type{};
}

std::size_t method::parameter_count() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->parameter_count() : 0;
}

std::optional<meta::type> method::get_parameter_type(std::size_t index) const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->get_parameter_type(index) : std::nullopt;
}

core::string_name method::get_parameter_name(std::size_t index) const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->get_parameter_name(index) : core::string_name{};
}

bool method::const_method() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr && method->const_method();
}

const meta::metadata_map& method::metadata() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->metadata() : empty_metadata_map();
}

const meta::metadata_value* method::find_metadata(meta::metadata_key key) const noexcept {
  return meta::find_metadata(metadata(), key);
}

bool method::matches_arguments(std::span<const argument> arguments,
                               argument_match_options options) const noexcept {
  const detail::method_record* method = info();
  return method != nullptr && method->matches_arguments(arguments, options);
}

bool method::matches_arguments(std::span<const named_argument> arguments,
                               argument_match_options options) const noexcept {
  const detail::method_record* method = info();
  return method != nullptr && method->matches_arguments(arguments, options);
}

std::size_t method::default_argument_count() const noexcept {
  const detail::method_record* method = info();
  return method != nullptr ? method->default_argument_count() : 0;
}

invoke_result method::invoke_result_of(const instance& object, std::span<const argument> arguments,
                                       argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_record* method = info();
  if (method == nullptr || !object) {
    return invoke_result::failed(method == nullptr ? invoke_failure_reason::invalid_method
                                                   : invoke_failure_reason::invalid_object);
  }

  return method->invoke_result_of(object.writable() ? object.mutable_data() : object.data(),
                                  object.writable(), arguments, options);
}

invoke_result method::invoke_result_of(const instance& object,
                                       std::span<const named_argument> arguments,
                                       argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_record* method = info();
  if (method == nullptr || !object) {
    return invoke_result::failed(method == nullptr ? invoke_failure_reason::invalid_method
                                                   : invoke_failure_reason::invalid_object);
  }

  return method->invoke_result_of(object.writable() ? object.mutable_data() : object.data(),
                                  object.writable(), arguments, options);
}

invoke_result method::invoke_result_of(std::span<const argument> arguments,
                                       argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_record* method = info();
  if (method == nullptr) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }

  return method->invoke_result_of(nullptr, false, arguments, options);
}

invoke_result method::invoke_result_of(std::span<const named_argument> arguments,
                                       argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_record* method = info();
  if (method == nullptr) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }

  return method->invoke_result_of(nullptr, false, arguments, options);
}

invoke_value_result method::invoke(const instance& object, std::span<const meta::value> arguments,
                                   argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_value_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_record* method = info();
  if (method == nullptr || !object) {
    return invoke_value_result::failed(method == nullptr ? invoke_failure_reason::invalid_method
                                                         : invoke_failure_reason::invalid_object);
  }

  return method->invoke_runtime_value(object.writable() ? object.mutable_data() : object.data(),
                                      object.writable(), arguments, options);
}

invoke_value_result method::invoke(std::span<const meta::value> arguments,
                                   argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_value_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_record* method = info();
  if (method == nullptr) {
    return invoke_value_result::failed(invoke_failure_reason::invalid_method);
  }

  return method->invoke_runtime_value(nullptr, false, arguments, options);
}

const detail::method_record* method::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_;
}

method_set::method_set(const detail::method_set_record* method_set,
                       std::shared_ptr<detail::registry_layer_state> layer) noexcept
    : method_set_(method_set), layer_(std::move(layer)) {}

bool method_set::valid() const noexcept {
  return method_set_ != nullptr && (layer_ == nullptr || layer_->active()) && method_set_->valid();
}

core::string_name method_set::name() const noexcept {
  const detail::method_set_record* method_set = info();
  return method_set != nullptr ? method_set->name() : core::string_name{};
}

std::size_t method_set::overload_count() const noexcept {
  const detail::method_set_record* method_set = info();
  return method_set != nullptr ? method_set->overload_count() : 0;
}

method method_set::overload_at(std::size_t index) const {
  const detail::method_set_record* method_set = info();
  return method_set != nullptr ? detail::method_access::make(method_set->overload_at(index), layer_)
                               : method{};
}

method method_set::single() const {
  const detail::method_set_record* method_set = info();
  return method_set != nullptr ? detail::method_access::make(method_set->single(), layer_)
                               : method{};
}

method_match_result method_set::match(std::span<const argument> arguments,
                                      argument_match_options options) const {
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr) {
    return method_match_result::none();
  }

  const detail::method_resolution result = method_set->match(arguments, options);
  if (result.kind == method_match_kind::ambiguous) {
    return method_match_result::ambiguous();
  }

  return result.matched() ? method_match_result::matched(
                                detail::method_access::make(result.method, layer_), result.kind)
                          : method_match_result::none();
}

method_match_result method_set::match(std::span<const named_argument> arguments,
                                      argument_match_options options) const {
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr) {
    return method_match_result::none();
  }

  const detail::method_resolution result = method_set->match(arguments, options);
  if (result.kind == method_match_kind::ambiguous) {
    return method_match_result::ambiguous();
  }

  return result.matched() ? method_match_result::matched(
                                detail::method_access::make(result.method, layer_), result.kind)
                          : method_match_result::none();
}

method method_set::resolve(std::span<const argument> arguments,
                           argument_match_options options) const {
  return match(arguments, options).value();
}

method method_set::resolve(std::span<const named_argument> arguments,
                           argument_match_options options) const {
  return match(arguments, options).value();
}

invoke_result method_set::invoke_result_of(const instance& object,
                                           std::span<const argument> arguments,
                                           argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr || !object) {
    return invoke_result::failed(method_set == nullptr ? invoke_failure_reason::invalid_method
                                                       : invoke_failure_reason::invalid_object);
  }

  return method_set->invoke_result_of(object.writable() ? object.mutable_data() : object.data(),
                                      object.writable(), arguments, options);
}

invoke_result method_set::invoke_result_of(const instance& object,
                                           std::span<const named_argument> arguments,
                                           argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr || !object) {
    return invoke_result::failed(method_set == nullptr ? invoke_failure_reason::invalid_method
                                                       : invoke_failure_reason::invalid_object);
  }

  return method_set->invoke_result_of(object.writable() ? object.mutable_data() : object.data(),
                                      object.writable(), arguments, options);
}

invoke_result method_set::invoke_result_of(std::span<const argument> arguments,
                                           argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }

  return method_set->invoke_result_of(nullptr, false, arguments, options);
}

invoke_result method_set::invoke_result_of(std::span<const named_argument> arguments,
                                           argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr) {
    return invoke_result::failed(invoke_failure_reason::invalid_method);
  }

  return method_set->invoke_result_of(nullptr, false, arguments, options);
}

invoke_value_result method_set::invoke(const instance& object,
                                       std::span<const meta::value> arguments,
                                       argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_value_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr || !object) {
    return invoke_value_result::failed(method_set == nullptr
                                           ? invoke_failure_reason::invalid_method
                                           : invoke_failure_reason::invalid_object);
  }

  return method_set->invoke_runtime_value(object.writable() ? object.mutable_data() : object.data(),
                                          object.writable(), arguments, options);
}

invoke_value_result method_set::invoke(std::span<const meta::value> arguments,
                                       argument_match_options options) const {
  const auto guard = call_guard_for(layer_);
  if (!guard) {
    return invoke_value_result::failed(invoke_failure_reason::invalid_method);
  }
  const detail::method_set_record* method_set = info();
  if (method_set == nullptr) {
    return invoke_value_result::failed(invoke_failure_reason::invalid_method);
  }

  return method_set->invoke_runtime_value(nullptr, false, arguments, options);
}

const detail::method_set_record* method_set::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_set_;
}

} // namespace caors::meta
