// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RUNTIME_INSTANCE_H_
#define CAORS_RUNTIME_INSTANCE_H_

#include "caors/core/containers.h"
#include "caors/object/base.h"
#include "caors/runtime/type.h"
#include "caors/value/any_ref.h"

#include <array>
#include <span>
#include <string_view>
#include <type_traits>

namespace caors::runtime {

/**
 * @brief 运行期实例反射访问门面。
 *
 * `runtime::instance` 不拥有对象，只保存外部实例的 `value::any_ref`。它负责把运行期值、类型视图和
 * 反射字段/方法组合起来，适合实现编辑器、调试面板和脚本桥接等高层访问入口。
 */
class CAORS_API instance final {
public:
  instance() noexcept = default;
  explicit instance(value::any_ref value) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] value::any_ref value() const noexcept;
  [[nodiscard]] runtime::type type() const noexcept;
  [[nodiscard]] reflection::type reflection() const noexcept;

  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] field field_at(std::size_t index) const;
  [[nodiscard]] field find_field(string_name name) const;
  [[nodiscard]] field find_field(std::string_view name) const;
  /** @brief 返回字段视图快照；字段视图仍引用全局反射注册表。 */
  [[nodiscard]] core::vector<field> fields(memory_resource* resource = nullptr) const;

  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method method_at(std::size_t index) const;
  [[nodiscard]] method find_method(string_name name) const;
  [[nodiscard]] method find_method(std::string_view name) const;
  /** @brief 返回方法视图快照；方法视图仍引用全局反射注册表。 */
  [[nodiscard]] core::vector<method> methods(memory_resource* resource = nullptr) const;

  /** @brief 读取字段值的运行期拷贝；字段不存在时返回无效 `any`。 */
  [[nodiscard]] any get(std::string_view name) const;

  /** @brief 读取字段值的运行期引用；只读实例返回只读引用。 */
  [[nodiscard]] any_ref get_ref(std::string_view name) const;

  /** @brief 从运行期值写入字段。 */
  bool set(std::string_view name, const any& source) const;

  /** @brief 从运行期引用写入字段。 */
  bool set(std::string_view name, const any_ref& source) const;

  /** @brief 从 C++ 值写入字段。 */
  template <typename T> bool set(std::string_view name, const T& source) const {
    const field reflected_field = find_field(name);
    if (!reflected_field || !value_.writable()) {
      return false;
    }

    const any_ref target = reflected_field.get_ref(value_.mutable_data());
    return target.try_set(source);
  }

  /** @brief 调用反射方法；只读实例只能调用 const 成员函数。 */
  [[nodiscard]] any call(std::string_view name, std::span<const any_ref> arguments = {}) const;

  /** @brief 使用 C++ 参数临时创建 `any_ref` 后调用反射方法。 */
  template <typename... Args> [[nodiscard]] any call(std::string_view name, Args&&... args) const {
    std::array<any_ref, sizeof...(Args)> argument_refs{any_ref::from(args)...};
    return call(name, std::span<const any_ref>{argument_refs.data(), argument_refs.size()});
  }

private:
  value::any_ref value_{};
};

/**
 * @brief 对象系统运行期适配视图。
 *
 * `runtime::object` 是 `runtime::instance` 之上的对象专用门面，用于处理 `objects::ptr<T>` 等对象
 * 指针语义。它不拥有对象生命周期。
 */
class CAORS_API object final {
public:
  object() noexcept = default;
  explicit object(instance value) noexcept;
  object(instance value, objects::object* pointer) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] instance as_instance() const noexcept;
  [[nodiscard]] value::any_ref value() const noexcept;
  [[nodiscard]] runtime::type type() const noexcept;

  [[nodiscard]] any get(std::string_view name) const;
  [[nodiscard]] any_ref get_ref(std::string_view name) const;
  bool set(std::string_view name, const any& source) const;
  bool set(std::string_view name, const any_ref& source) const;

  template <typename T> bool set(std::string_view name, const T& source) const {
    return instance_.set(name, source);
  }

  /** @brief 返回当前视图显式绑定的对象指针；没有显式绑定时可能返回 `nullptr`。 */
  [[nodiscard]] objects::object* ptr() const noexcept;

  /** @brief 读取字段中的对象指针。 */
  [[nodiscard]] objects::object* get_ptr(std::string_view name) const;

  /** @brief 写入字段中的对象指针。 */
  bool set_ptr(std::string_view name, objects::object* pointer) const;

private:
  instance instance_{};
  objects::object* pointer_ = nullptr;
};

/** @brief 从运行期引用创建实例视图。 */
[[nodiscard]] inline instance view(value::any_ref value) noexcept { return instance{value}; }

/** @brief 从 C++ 对象创建实例视图。 */
template <typename T> [[nodiscard]] instance view(T& value) {
  return instance{value::any_ref::from(value)};
}

/** @brief 从只读 C++ 对象创建实例视图。 */
template <typename T> [[nodiscard]] instance view_const(const T& value) {
  return instance{value::any_ref::from_const(value)};
}

/** @brief 从实例视图创建对象适配视图；不会从类型擦除的派生对象自动推导对象指针。 */
[[nodiscard]] object object_view(instance value) noexcept;

/** @brief 从对象派生实例创建对象适配视图，同时保留真实 C++ 类型的反射信息。 */
template <typename T> [[nodiscard]] object object_view(T& value) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(std::is_base_of_v<objects::object, value_type>,
                "T must derive from caors::objects::object");
  static_assert(!std::is_const_v<std::remove_reference_t<T>>,
                "runtime::object_view requires a mutable object");
  return object{view(value), &value};
}

} // namespace caors::runtime

#endif
