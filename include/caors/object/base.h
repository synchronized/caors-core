// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_BASE_H_
#define CAORS_OBJECT_BASE_H_

#include "caors/config.h"
#include "caors/meta/query.h"
#include "caors/object/id.h"

#include <string_view>
#include <type_traits>

namespace caors {

/**
 * @brief CAORS 对象基类。
 *
 * 该类型只提供对象身份和运行期类型入口，不负责引用计数、GC、脚本或属性反射。
 */
class CAORS_API object {
public:
  object() noexcept;
  virtual ~object();

  object(const object&) = delete;
  object& operator=(const object&) = delete;
  object(object&&) = delete;
  object& operator=(object&&) = delete;

  [[nodiscard]] object_id id() const noexcept;
  [[nodiscard]] virtual object_type_id type_id() const noexcept;
  [[nodiscard]] virtual std::string_view type_name() const noexcept;

  [[nodiscard]] static object_type_id static_type_id() noexcept;
  [[nodiscard]] static std::string_view static_type_name() noexcept;

private:
  object_id id_;
};

namespace objects::detail {

template <typename T, typename = void> struct object_type_guid_traits {
  [[nodiscard]] static constexpr type_guid get() noexcept { return type_guid::nil(); }
};

template <typename T>
struct object_type_guid_traits<T, std::void_t<decltype(T::object_type_guid)>> {
  static_assert(
      std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(T::object_type_guid)>>,
                     type_guid>,
      "object_type_guid must be caors::type_guid");

  [[nodiscard]] static constexpr type_guid get() noexcept { return T::object_type_guid; }
};

template <typename T> [[nodiscard]] constexpr type_guid object_type_guid_of() noexcept {
  return object_type_guid_traits<T>::get();
}

template <typename T, typename = void> struct object_base_type_traits {
  using type = object;
};

template <typename T> struct object_base_type_traits<T, std::void_t<typename T::base_type>> {
  using type = typename T::base_type;
};

} // namespace objects::detail

/**
 * @brief 为派生对象提供默认运行期类型名实现。
 *
 * `Derived` 需要提供 `static constexpr std::string_view object_type_name`。可序列化、可反射或跨插件
 * 暴露的类型建议额外提供 `static constexpr caors::type_guid object_type_guid`。
 */
template <typename Derived, typename Base = object> class object_base : public Base {
  static_assert(std::is_base_of_v<object, Base>, "Base must derive from caors::object");

public:
  using base_type = Base;

  [[nodiscard]] static object_type_id static_type_id() noexcept {
#if CAORS_HAS_EXCEPTIONS
    try {
      return caors::meta::get<Derived>().id();
    } catch (...) {
      return object_type_id::invalid();
    }
#else
    return caors::meta::get<Derived>().id();
#endif
  }

  [[nodiscard]] static std::string_view static_type_name() noexcept {
    return Derived::object_type_name;
  }

  [[nodiscard]] static constexpr type_guid static_type_guid() noexcept {
    return objects::detail::object_type_guid_of<Derived>();
  }

  [[nodiscard]] object_type_id type_id() const noexcept override { return static_type_id(); }

  [[nodiscard]] std::string_view type_name() const noexcept override { return static_type_name(); }
};

} // namespace caors

#endif
