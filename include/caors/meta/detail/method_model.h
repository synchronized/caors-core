// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_METHOD_MODEL_H_
#define CAORS_META_DETAIL_METHOD_MODEL_H_

#include "caors/meta/detail/argument_access.h"
#include "caors/meta/detail/callable_signature.h"
#include "caors/meta/detail/method_record.h"
#include "caors/meta/detail/value_conversion.h"

namespace caors::meta::detail {

template <typename T> using method_value_type = argument_value_type<T>;

template <typename Parameter>
inline constexpr bool method_value_argument_supported_v =
    value_parameter_conversion_supported_v<Parameter>;

template <typename... Arguments>
inline constexpr bool method_value_arguments_supported_v =
    (method_value_argument_supported_v<Arguments> && ...);

template <typename Parameter>
[[nodiscard]] argument_match_rank method_value_argument_rank(const meta::value& source) noexcept {
  using value_type = method_value_type<Parameter>;
  if constexpr (!method_value_argument_supported_v<Parameter>) {
    return argument_match_rank::none;
  } else {
    if (!value_can_convert<value_type>(source)) {
      return argument_match_rank::none;
    }

    if constexpr (std::is_same_v<value_type, bool>) {
      return source.is_bool() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else if constexpr (std::is_same_v<value_type, std::int64_t>) {
      return source.is_i64() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else if constexpr (std::is_same_v<value_type, std::uint64_t>) {
      return source.is_u64() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else if constexpr (std::is_same_v<value_type, double>) {
      return source.is_f64() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else if constexpr (std::is_same_v<value_type, core::string_name>) {
      return source.is_string_name() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else if constexpr (std::is_same_v<value_type, core::string>) {
      return source.is_string() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else if constexpr (std::is_same_v<value_type, meta::object*>) {
      return source.is_object() ? argument_match_rank::exact : argument_match_rank::conversion;
    } else {
      return argument_match_rank::conversion;
    }
  }
}

template <typename... Arguments>
[[nodiscard]] argument_match_rank
method_value_arguments_rank(const method_record&, std::span<const meta::value> arguments,
                            argument_match_options) noexcept {
  if constexpr (!method_value_arguments_supported_v<Arguments...>) {
    return argument_match_rank::none;
  } else {
    if (arguments.size() != sizeof...(Arguments)) {
      return argument_match_rank::none;
    }

    argument_match_rank result = argument_match_rank::exact;
    std::size_t index = 0;
    (([&] {
       const argument_match_rank rank = method_value_argument_rank<Arguments>(arguments[index++]);
       if (rank == argument_match_rank::none) {
         result = argument_match_rank::none;
       } else if (rank == argument_match_rank::conversion && result == argument_match_rank::exact) {
         result = argument_match_rank::conversion;
       }
     }()),
     ...);

    return result;
  }
}

template <typename Parameter>
[[nodiscard]] method_value_type<Parameter> method_value_argument(const meta::value& source) {
  using value_type = method_value_type<Parameter>;
  std::optional<value_type> converted = try_value_convert<value_type>(source);
  return std::move(*converted);
}

template <typename Return, typename Result>
[[nodiscard]] invoke_result make_method_invoke_result(Result&& result) {
  if constexpr (std::is_lvalue_reference_v<Return>) {
    return invoke_result::reference(any_ref::from(std::forward<Result>(result)));
  } else {
    using value_type = method_value_type<Return>;
    if constexpr (std::is_copy_constructible_v<value_type>) {
      return invoke_result::value(any::from(result));
    } else if constexpr (std::is_move_constructible_v<value_type>) {
      auto* value = memory::make<value_type>(&memory::library_resource(), std::move(result));
      return invoke_result::value(any::take_ownership(
          make_owned_value(meta::resolve<value_type>(), value, &memory::library_resource(),
                           [](memory::memory_resource* resource, void* instance) noexcept {
                             memory::destroy(static_cast<value_type*>(instance), resource);
                           })));
    } else {
      return invoke_result::failed();
    }
  }
}

template <typename Return, typename Result>
[[nodiscard]] invoke_value_result make_method_invoke_value_result(Result&& result) {
  if constexpr (std::is_lvalue_reference_v<Return>) {
    return invoke_value_result::from_value(meta::value::from_ref(std::forward<Result>(result)));
  } else {
    using value_type = method_value_type<Return>;
    if constexpr (value_conversion_supported_v<value_type>) {
      if constexpr (std::is_same_v<value_type, meta::object*>) {
        return invoke_value_result::from_value(meta::value::from_object(result));
      } else {
        return invoke_value_result::from_value(meta::value{std::forward<Result>(result)});
      }
    } else if constexpr (std::is_copy_constructible_v<value_type>) {
      return invoke_value_result::from_value(meta::value::from_any(any::from(result)));
    } else if constexpr (std::is_move_constructible_v<value_type>) {
      auto* value = memory::make<value_type>(&memory::library_resource(), std::move(result));
      return invoke_value_result::from_value(meta::value::from_any(any::take_ownership(
          make_owned_value(meta::resolve<value_type>(), value, &memory::library_resource(),
                           [](memory::memory_resource* resource, void* instance) noexcept {
                             memory::destroy(static_cast<value_type*>(instance), resource);
                           }))));
    } else {
      return invoke_value_result::failed();
    }
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result invoke_member_impl(Return (Class::*member)(Arguments...), Class& object,
                                               std::span<const argument> arguments,
                                               std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result
invoke_member_impl(Return (Class::*member)(Arguments...) const, const Class& object,
                   std::span<const argument> arguments, std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_value_result
invoke_member_value_impl(Return (Class::*member)(Arguments...), Class& object,
                         std::span<const meta::value> arguments, std::index_sequence<Index...>) {
  if constexpr (!method_value_arguments_supported_v<Arguments...>) {
    return invoke_value_result::failed();
  } else if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, method_value_argument<Arguments>(arguments[Index])...);
    return invoke_value_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(member, object, method_value_argument<Arguments>(arguments[Index])...);
    return make_method_invoke_value_result<Return>(result);
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_value_result
invoke_member_value_impl(Return (Class::*member)(Arguments...) const, const Class& object,
                         std::span<const meta::value> arguments, std::index_sequence<Index...>) {
  if constexpr (!method_value_arguments_supported_v<Arguments...>) {
    return invoke_value_result::failed();
  } else if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, method_value_argument<Arguments>(arguments[Index])...);
    return invoke_value_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(member, object, method_value_argument<Arguments>(arguments[Index])...);
    return make_method_invoke_value_result<Return>(result);
  }
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_member_method(const method_record& method, const void* object,
                                                 bool mutable_object,
                                                 std::span<const argument> arguments) {
  if (object == nullptr || !mutable_object || !arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...)>();
  auto* instance = const_cast<Class*>(static_cast<const Class*>(object));
  return invoke_member_impl(member, *instance, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_const_member_method(const method_record& method,
                                                       const void* object, bool /*mutable_object*/,
                                                       std::span<const argument> arguments) {
  if (object == nullptr || !arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...) const>();
  const auto* instance = static_cast<const Class*>(object);
  return invoke_member_impl(member, *instance, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] invoke_value_result
invoke_member_method_value(const method_record& method, const void* object, bool mutable_object,
                           std::span<const meta::value> arguments) {
  if (object == nullptr || !mutable_object ||
      method_value_arguments_rank<Arguments...>(method, arguments, {}) ==
          argument_match_rank::none) {
    return invoke_value_result::failed();
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...)>();
  auto* instance = const_cast<Class*>(static_cast<const Class*>(object));
  return invoke_member_value_impl(member, *instance, arguments,
                                  std::index_sequence_for<Arguments...>{});
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] invoke_value_result
invoke_const_member_method_value(const method_record& method, const void* object,
                                 bool /*mutable_object*/, std::span<const meta::value> arguments) {
  if (object == nullptr || method_value_arguments_rank<Arguments...>(method, arguments, {}) ==
                               argument_match_rank::none) {
    return invoke_value_result::failed();
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...) const>();
  const auto* instance = static_cast<const Class*>(object);
  return invoke_member_value_impl(member, *instance, arguments,
                                  std::index_sequence_for<Arguments...>{});
}

template <typename Class> [[nodiscard]] meta::type method_owner_type(const method_record&) noexcept {
  return meta::resolve<Class>();
}

template <typename Return>
[[nodiscard]] meta::type method_return_type(const method_record&) noexcept {
  if constexpr (std::is_void_v<method_value_type<Return>>) {
    return {};
  } else {
    return meta::resolve<method_value_type<Return>>();
  }
}

template <typename... Arguments>
[[nodiscard]] std::size_t method_parameter_count(const method_record&) noexcept {
  return sizeof...(Arguments);
}

template <typename... Arguments>
[[nodiscard]] std::optional<meta::type> method_parameter_type(const method_record&,
                                                              std::size_t index) noexcept {
  if constexpr (sizeof...(Arguments) == 0) {
    return std::nullopt;
  } else {
    if (index >= sizeof...(Arguments)) {
      return std::nullopt;
    }

    std::array<meta::type, sizeof...(Arguments)> types{
        meta::resolve<method_value_type<Arguments>>()...};
    if (!types[index]) {
      return std::nullopt;
    }

    return types[index];
  }
}

template <typename... Arguments>
[[nodiscard]] argument_match_rank method_arguments_rank(const method_record&,
                                                        std::span<const argument> arguments,
                                                        argument_match_options options) noexcept {
  return arguments_rank<Arguments...>(arguments, options);
}

template <typename... Arguments>
[[nodiscard]] vector<argument> method_prepare_arguments(const method_record&,
                                                        std::span<const argument> arguments,
                                                        argument_match_options options) {
  return prepare_arguments<Arguments...>(arguments, options);
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_ops member_method_ops() noexcept {
  return method_ops{
      &method_owner_type<Class>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_rank<Arguments...>,
      &method_prepare_arguments<Arguments...>,
      &invoke_member_method<Class, Return, Arguments...>,
      &method_value_arguments_rank<Arguments...>,
      &invoke_member_method_value<Class, Return, Arguments...>,
  };
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_ops const_member_method_ops() noexcept {
  return method_ops{
      &method_owner_type<Class>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_rank<Arguments...>,
      &method_prepare_arguments<Arguments...>,
      &invoke_const_member_method<Class, Return, Arguments...>,
      &method_value_arguments_rank<Arguments...>,
      &invoke_const_member_method_value<Class, Return, Arguments...>,
  };
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_record make_method_record(std::string_view name,
                                           Return (Class::*member)(Arguments...)) {
  method_record method{
      string_name{name},
      member_method_ops<Class, Return, Arguments...>(),
      false,
  };
  method.store_member_pointer(member);
  return method;
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_record make_method_record(std::string_view name,
                                           Return (Class::*member)(Arguments...) const) {
  method_record method{
      string_name{name},
      const_member_method_ops<Class, Return, Arguments...>(),
      true,
  };
  method.store_member_pointer(member);
  return method;
}

template <typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result invoke_global_impl(Return (*function)(Arguments...),
                                               std::span<const argument> arguments,
                                               std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(function, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result = std::invoke(function, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_value_result invoke_global_value_impl(Return (*function)(Arguments...),
                                                           std::span<const meta::value> arguments,
                                                           std::index_sequence<Index...>) {
  if constexpr (!method_value_arguments_supported_v<Arguments...>) {
    return invoke_value_result::failed();
  } else if constexpr (std::is_void_v<Return>) {
    std::invoke(function, method_value_argument<Arguments>(arguments[Index])...);
    return invoke_value_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(function, method_value_argument<Arguments>(arguments[Index])...);
    return make_method_invoke_value_result<Return>(result);
  }
}

template <typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_global_method(const method_record& method, const void*, bool,
                                                 std::span<const argument> arguments) {
  if (!arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  auto function = method.template load_member_pointer<Return (*)(Arguments...)>();
  if (function == nullptr) {
    return invoke_result::failed();
  }

  return invoke_global_impl(function, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Return, typename... Arguments>
[[nodiscard]] invoke_value_result
invoke_global_method_value(const method_record& method, const void*, bool,
                           std::span<const meta::value> arguments) {
  if (method_value_arguments_rank<Arguments...>(method, arguments, {}) ==
      argument_match_rank::none) {
    return invoke_value_result::failed();
  }

  auto function = method.template load_member_pointer<Return (*)(Arguments...)>();
  if (function == nullptr) {
    return invoke_value_result::failed();
  }

  return invoke_global_value_impl(function, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Callable, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result invoke_global_callable_impl(const Callable& callable,
                                                        std::span<const argument> arguments,
                                                        std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(callable, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result = std::invoke(callable, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Callable, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_value_result
invoke_global_callable_value_impl(const Callable& callable, std::span<const meta::value> arguments,
                                  std::index_sequence<Index...>) {
  if constexpr (!method_value_arguments_supported_v<Arguments...>) {
    return invoke_value_result::failed();
  } else if constexpr (std::is_void_v<Return>) {
    std::invoke(callable, method_value_argument<Arguments>(arguments[Index])...);
    return invoke_value_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(callable, method_value_argument<Arguments>(arguments[Index])...);
    return make_method_invoke_value_result<Return>(result);
  }
}

template <typename Callable, typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_global_callable_method(const method_record& method, const void*,
                                                          bool,
                                                          std::span<const argument> arguments) {
  if (!arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  const auto* callable = method.template callable<Callable>();
  if (callable == nullptr) {
    return invoke_result::failed();
  }

  return invoke_global_callable_impl<Callable, Return, Arguments...>(
      *callable, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Callable, typename Return, typename... Arguments>
[[nodiscard]] invoke_value_result
invoke_global_callable_method_value(const method_record& method, const void*, bool,
                                    std::span<const meta::value> arguments) {
  if (method_value_arguments_rank<Arguments...>(method, arguments, {}) ==
      argument_match_rank::none) {
    return invoke_value_result::failed();
  }

  const auto* callable = method.template callable<Callable>();
  if (callable == nullptr) {
    return invoke_value_result::failed();
  }

  return invoke_global_callable_value_impl<Callable, Return, Arguments...>(
      *callable, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Return, typename... Arguments>
[[nodiscard]] meta::type global_method_owner_type(const method_record&) noexcept {
  return {};
}

template <typename Return, typename... Arguments>
[[nodiscard]] method_ops global_method_ops() noexcept {
  return method_ops{
      &global_method_owner_type<Return, Arguments...>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_rank<Arguments...>,
      &method_prepare_arguments<Arguments...>,
      &invoke_global_method<Return, Arguments...>,
      &method_value_arguments_rank<Arguments...>,
      &invoke_global_method_value<Return, Arguments...>,
  };
}

template <typename Return, typename... Arguments>
[[nodiscard]] method_record make_global_method_record(std::string_view name,
                                                  Return (*function)(Arguments...)) {
  method_record method{
      string_name{name},
      global_method_ops<Return, Arguments...>(),
      true,
  };
  method.store_member_pointer(function);
  return method;
}

template <typename Callable, typename Signature> struct global_callable_method_factory;

template <typename Callable, typename Return, typename... Arguments>
struct global_callable_method_factory<Callable, function_signature<Return, Arguments...>> {
  [[nodiscard]] static method_ops ops() noexcept {
    return method_ops{
        &global_method_owner_type<Return, Arguments...>,
        &method_return_type<Return>,
        &method_parameter_count<Arguments...>,
        &method_parameter_type<Arguments...>,
        &method_arguments_rank<Arguments...>,
        &method_prepare_arguments<Arguments...>,
        &invoke_global_callable_method<Callable, Return, Arguments...>,
        &method_value_arguments_rank<Arguments...>,
        &invoke_global_callable_method_value<Callable, Return, Arguments...>,
    };
  }

  template <typename Value>
  [[nodiscard]] static method_record make(std::string_view name, Value&& callable) {
    method_record method{
        string_name{name},
        ops(),
        true,
    };
    method.store_callable(std::forward<Value>(callable));
    return method;
  }
};

template <typename Callable>
[[nodiscard]] method_record make_global_callable_method_record(std::string_view name,
                                                           Callable&& callable) {
  using value_type = std::decay_t<Callable>;
  using signature = callable_signature_t<value_type>;
  return global_callable_method_factory<value_type, signature>::make(
      name, std::forward<Callable>(callable));
}

} // namespace caors::meta::detail

#endif
