// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_VIEW_H_
#define CAORS_OBJECT_VIEW_H_

#include "caors/core/containers.h"
#include "caors/meta/argument.h"
#include "caors/meta/instance.h"
#include "caors/meta/method.h"
#include "caors/meta/property.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/value.h"
#include "caors/object/base.h"

#include <array>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::objects {

/**
 * @brief 对象系统反射访问门面。
 *
 * `objects::object_view` 不拥有对象，只保存外部实例的 `meta::any_ref` 和可选的
 * `objects::object*`。它用于对象系统场景下的属性读写、方法调用和对象指针属性访问。
 */
class CAORS_API object_view final {
public:
  object_view() noexcept = default;
  explicit object_view(meta::any_ref value) noexcept;
  object_view(meta::any_ref value, object* pointer) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::any_ref value() const noexcept;
  [[nodiscard]] meta::type type() const noexcept;

  /** @brief 查询并绑定当前对象实例的运行期能力视图。 */
  template <typename View> [[nodiscard]] View get_capability() const {
    return value_.template get_capability<View>();
  }

  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] meta::property property_at(std::size_t index) const;
  [[nodiscard]] meta::property find_property(string_name name) const;
  [[nodiscard]] meta::property find_property(std::string_view name) const;
  [[nodiscard]] core::vector<meta::property> properties(memory_resource* resource = nullptr) const;

  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] meta::method_set method_at(std::size_t index) const;
  [[nodiscard]] meta::method_set find_method(string_name name) const;
  [[nodiscard]] meta::method_set find_method(std::string_view name) const;
  [[nodiscard]] core::vector<meta::method_set> methods(memory_resource* resource = nullptr) const;

  /** @brief 读取属性值的高层运行期值；属性不存在时返回 `null`。 */
  [[nodiscard]] meta::value get(std::string_view name) const;

  /** @brief 读取属性值的精确 C++ 拷贝；属性不存在时返回无效 `any`。 */
  [[nodiscard]] any get_any(std::string_view name) const;

  /** @brief 读取属性值的运行期引用；只读实例返回只读引用。 */
  [[nodiscard]] any_ref get_ref(std::string_view name) const;

  /** @brief 从高层运行期值写入属性。 */
  bool set(std::string_view name, const meta::value& source) const;

  /** @brief 从运行期引用写入属性。 */
  bool set_ref(std::string_view name, const any_ref& source) const;

  /** @brief 从精确 C++ 运行期值写入属性。 */
  bool set_any(std::string_view name, const any& source) const;

  /** @brief 从 C++ 值写入属性。 */
  template <typename T> bool set(std::string_view name, const T& source) const {
    const meta::property reflected_property = find_property(name);
    if (!reflected_property || !value_.writable()) {
      return false;
    }

    const any_ref target = reflected_property.get_ref(meta::instance{value_});
    return target.try_set(source);
  }

  /** @brief 使用高层运行期值参数调用反射方法。 */
  [[nodiscard]] meta::invoke_value_result call(std::string_view name,
                                               std::span<const meta::value> arguments = {}) const;

  /** @brief 调用反射方法并返回精确 C++ 拷贝结果；只读实例只能调用 const 成员函数。 */
  [[nodiscard]] any call_any(std::string_view name,
                             std::span<const meta::argument> arguments = {}) const;

  /** @brief 使用 C++ 参数临时创建左值 `argument` 后调用反射方法并返回精确 C++ 拷贝结果。 */
  template <typename... Args>
  [[nodiscard]] any call_any(std::string_view name, Args&&... args) const {
    std::array<meta::argument, sizeof...(Args)> argument_refs{meta::argument::ref(args)...};
    return call_any(name,
                    std::span<const meta::argument>{argument_refs.data(), argument_refs.size()});
  }

  /** @brief 使用 C++ 参数创建高层运行期参数后调用反射方法。 */
  template <typename... Args>
  [[nodiscard]] meta::invoke_value_result call(std::string_view name, Args&&... args) const {
    std::array<meta::value, sizeof...(Args)> values{make_call_value(std::forward<Args>(args))...};
    return call(name, std::span<const meta::value>{values.data(), values.size()});
  }

  /** @brief 返回当前视图显式绑定的对象指针；没有显式绑定时可能返回 `nullptr`。 */
  [[nodiscard]] object* ptr() const noexcept;

  /** @brief 读取属性中的对象指针。 */
  [[nodiscard]] object* get_ptr(std::string_view name) const;

  /** @brief 写入属性中的对象指针。 */
  bool set_ptr(std::string_view name, object* pointer) const;

private:
  template <typename T> [[nodiscard]] static meta::value make_call_value(T&& source) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<value_type, meta::value>) {
      return std::forward<T>(source);
    } else {
      return meta::value::from_any(static_cast<const value_type&>(source));
    }
  }

  meta::any_ref value_{};
  object* pointer_ = nullptr;
};

/** @brief 从运行期引用创建对象视图。 */
[[nodiscard]] inline object_view view(meta::any_ref value) noexcept { return object_view{value}; }

/** @brief 从 C++ 实例创建对象视图；对象派生类型会额外保存对象指针。 */
template <typename T>
  requires(!std::is_same_v<std::remove_cvref_t<T>, meta::any_ref>)
[[nodiscard]] object_view view(T& value) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(!std::is_const_v<std::remove_reference_t<T>>,
                "objects::view requires a mutable value");
  if constexpr (std::is_base_of_v<object, value_type>) {
    return object_view{meta::any_ref::from(value), &value};
  } else {
    return object_view{meta::any_ref::from(value)};
  }
}

} // namespace caors::objects

#endif
