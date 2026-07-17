// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_INSTANCE_H_
#define CAORS_META_INSTANCE_H_

#include "caors/core/containers.h"
#include "caors/meta/method.h"
#include "caors/meta/property.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <array>
#include <span>
#include <string_view>
#include <type_traits>

namespace caors::meta {

/**
 * @brief 运行期实例反射访问视图。
 *
 * `meta::instance` 不拥有对象，只保存外部实例的 `meta::any_ref`。调用方必须保证被引用对象在
 * instance 使用期间仍然存活。它用于把对象实例、meta 类型、属性和方法调用组合成统一入口。
 */
class CAORS_API instance final {
public:
  instance() noexcept = default;
  explicit instance(meta::any_ref value) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::any_ref value() const noexcept;
  [[nodiscard]] meta::type type() const noexcept;
  [[nodiscard]] const void* data() const noexcept;
  [[nodiscard]] void* mutable_data() const noexcept;
  [[nodiscard]] bool writable() const noexcept;

  /** @brief 查询并绑定当前实例的运行期能力视图。 */
  template <typename Capability> [[nodiscard]] auto get_capability() const {
    return value_.template get_capability<Capability>();
  }

  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] property property_at(std::size_t index) const;
  [[nodiscard]] property find_property(string_name name) const;
  [[nodiscard]] property find_property(std::string_view name) const;
  /** @brief 返回属性视图快照；属性视图仍引用全局 meta 注册表。 */
  [[nodiscard]] core::vector<property> properties(memory_resource* resource = nullptr) const;

  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method_set method_at(std::size_t index) const;
  [[nodiscard]] method_set find_method(string_name name) const;
  [[nodiscard]] method_set find_method(std::string_view name) const;
  /** @brief 返回方法视图快照；方法视图仍引用全局 meta 注册表。 */
  [[nodiscard]] core::vector<method_set> methods(memory_resource* resource = nullptr) const;

  /** @brief 读取属性值的运行期拷贝；属性不存在时返回无效 `any`。 */
  [[nodiscard]] any get(std::string_view name) const;

  /** @brief 读取属性值的运行期引用；只读实例返回只读引用。 */
  [[nodiscard]] any_ref get_ref(std::string_view name) const;

  /** @brief 从运行期值写入属性。 */
  bool set(std::string_view name, const any& source) const;

  /** @brief 从运行期引用写入属性。 */
  bool set(std::string_view name, const any_ref& source) const;

  /** @brief 从 C++ 值写入属性。 */
  template <typename T> bool set(std::string_view name, const T& source) const {
    const property reflected_property = find_property(name);
    if (!reflected_property || !value_.writable()) {
      return false;
    }

    const any_ref target = reflected_property.get_ref(*this);
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
  meta::any_ref value_{};
};

/** @brief 从运行期引用创建实例视图。 */
[[nodiscard]] inline instance view(meta::any_ref value) noexcept { return instance{value}; }

/** @brief 从 C++ 对象创建实例视图。 */
template <typename T> [[nodiscard]] instance view(T& value) {
  return instance{meta::any_ref::from(value)};
}

/** @brief 从只读 C++ 对象创建实例视图。 */
template <typename T> [[nodiscard]] instance view_const(const T& value) {
  return instance{meta::any_ref::from_const(value)};
}

} // namespace caors::meta

namespace caors {

using meta::instance;

} // namespace caors

#endif
