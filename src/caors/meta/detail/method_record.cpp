// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/method_record.h"

#include "caors/meta/value/value.h"

#include <optional>
#include <utility>

namespace caors::meta::detail {
namespace {

[[nodiscard]] invoke_value_result make_value_result(invoke_result result) {
  switch (result.kind()) {
  case invoke_result_kind::void_value:
    return invoke_value_result::void_value();
  case invoke_result_kind::value:
    if (std::optional<meta::value> value = try_make_value_from_any(result.mutable_value());
        value.has_value()) {
      return invoke_value_result::from_value(std::move(*value));
    }
    return invoke_value_result::from_value(
        meta::value::from_any(std::move(result.mutable_value())));
  case invoke_result_kind::reference:
    return invoke_value_result::from_value(meta::value::from_ref(result.ref()));
  case invoke_result_kind::failed:
  default:
    return invoke_value_result::failed(result.failure_reason());
  }
}

[[nodiscard]] vector<argument> make_arguments(std::span<const meta::value> values,
                                              memory::memory_resource* resource) {
  vector<argument> arguments{memory::allocator<argument>{resource}};
  arguments.reserve(values.size());
  for (const meta::value& value : values) {
    argument converted = value.to_argument();
    if (!converted) {
      return {};
    }
    arguments.push_back(std::move(converted));
  }
  return arguments;
}

} // namespace

invoke_value_result method_record::invoke_runtime_value(const void* object, bool mutable_object,
                                                        std::span<const meta::value> arguments,
                                                        argument_match_options options) const {
  temporary_memory_scope scratch{options.scratch_resource};
  options.scratch_resource = scratch.resource();
  if (matches_arguments(arguments, options)) {
    return invoke_value_result_of(object, mutable_object, arguments, options);
  }

  vector<argument> converted = make_arguments(arguments, options.scratch_resource);
  if (converted.empty() && !arguments.empty()) {
    return invoke_value_result::failed(invoke_failure_reason::argument_mismatch);
  }
  return make_value_result(invoke_result_of(object, mutable_object, converted, options));
}

invoke_value_result method_set_record::invoke_runtime_value(const void* object, bool mutable_object,
                                                            std::span<const meta::value> arguments,
                                                            argument_match_options options) const {
  temporary_memory_scope scratch{options.scratch_resource};
  options.scratch_resource = scratch.resource();
  const method_invocation_resolution value_match =
      match_invocable(arguments, options, object, mutable_object);
  if (value_match.kind == method_match_kind::ambiguous) {
    return invoke_value_result::failed(value_match.failure_reason);
  }
  if (value_match.matched()) {
    return value_match.method->invoke_runtime_value(object, mutable_object, arguments, options);
  }
  if (value_match.failure_reason != invoke_failure_reason::argument_mismatch) {
    return invoke_value_result::failed(value_match.failure_reason);
  }

  vector<argument> converted = make_arguments(arguments, options.scratch_resource);
  if (converted.empty() && !arguments.empty()) {
    return invoke_value_result::failed(invoke_failure_reason::argument_mismatch);
  }

  const method_invocation_resolution argument_match =
      match_invocable(std::span<const argument>{converted}, options, object, mutable_object);
  if (argument_match.kind == method_match_kind::ambiguous || !argument_match.matched()) {
    return invoke_value_result::failed(argument_match.failure_reason);
  }
  return make_value_result(
      argument_match.method->invoke_result_of(object, mutable_object, converted, options));
}

invoke_result method_set_record::invoke_result_of(const void* object, bool mutable_object,
                                                  std::span<const argument> arguments,
                                                  argument_match_options options) const {
  const method_invocation_resolution result =
      match_invocable(arguments, options, object, mutable_object);
  if (result.kind == method_match_kind::ambiguous || !result.matched()) {
    return invoke_result::failed(result.failure_reason);
  }
  return result.method->invoke_result_of(object, mutable_object, arguments, options);
}

invoke_result method_set_record::invoke_result_of(const void* object, bool mutable_object,
                                                  std::span<const named_argument> arguments,
                                                  argument_match_options options) const {
  const method_invocation_resolution result =
      match_invocable(arguments, options, object, mutable_object);
  if (result.kind == method_match_kind::ambiguous || !result.matched()) {
    return invoke_result::failed(result.failure_reason);
  }
  return result.method->invoke_result_of(object, mutable_object, arguments, options);
}

} // namespace caors::meta::detail
