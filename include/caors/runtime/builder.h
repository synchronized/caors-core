// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RUNTIME_BUILDER_H_
#define CAORS_RUNTIME_BUILDER_H_

#include "caors/error.h"
#include "caors/meta/constructor.h"
#include "caors/meta/query.h"
#include "caors/reflection/builder.h"
#include "caors/runtime/query.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::runtime {

/**
 * @brief 组合式运行时类型注册构建器。
 *
 * 该 facade 先注册 `meta` 类型，再把字段和方法挂到反射组件上。底层仍保持 `meta` 与
 * `reflection` 分层，避免 `meta` 反向依赖反射和值系统。
 */
template <typename Class> class class_builder {
public:
  explicit class_builder(std::string_view name)
      : meta_builder_(meta::class_<Class>(name)), reflection_builder_(reflection::class_<Class>()) {
    update_meta_status();
  }

  class_builder& guid(type_guid guid_value) {
    meta_builder_.guid(guid_value);
    update_meta_status();
    return *this;
  }

  class_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  template <typename Parent> class_builder& parent() {
    meta_builder_.template parent<Parent>();
    update_meta_status();
    return *this;
  }

  class_builder& parent(type_id parent_id) {
    meta_builder_.parent(parent_id);
    update_meta_status();
    return *this;
  }

  class_builder& factory() {
    meta_builder_.factory();
    update_meta_status();
    return *this;
  }

  class_builder& factory(meta::factory_ops::construct_fn construct,
                         meta::factory_ops::destroy_fn destroy) {
    meta_builder_.factory(construct, destroy);
    update_meta_status();
    return *this;
  }

  /** @brief 注册一组运行期构造参数，供 `runtime::type::create(arguments)` 使用。 */
  template <typename... Arguments> class_builder& constructor() {
    meta_builder_.template constructor<Arguments...>();
    update_meta_status();
    return *this;
  }

  template <typename Field> class_builder& field(std::string_view name, Field Class::* member) {
    if (!failed()) {
      reflection_builder_.field(name, member);
    }
    return *this;
  }

  template <typename Field>
  class_builder& field(std::string_view name, Field Class::* member, std::string_view type_name) {
    if (!failed()) {
      reflection_builder_.field(name, member, type_name);
    }
    return *this;
  }

  template <typename Return, typename... Arguments>
  class_builder& method(std::string_view name, Return (Class::*member)(Arguments...)) {
    if (!failed()) {
      reflection_builder_.method(name, member);
    }
    return *this;
  }

  template <typename Return, typename... Arguments>
  class_builder& method(std::string_view name, Return (Class::*member)(Arguments...) const) {
    if (!failed()) {
      reflection_builder_.method(name, member);
    }
    return *this;
  }

  /** @brief 提交反射组件注册；meta 类型在构造 builder 时已经注册。 */
  [[nodiscard]] error commit() {
    if (failed()) {
      return status_;
    }

    status_ = reflection_builder_.commit();
    return status_;
  }

  [[nodiscard]] runtime::type get() const noexcept { return runtime::type{meta_builder_.get()}; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept { return !failed(); }

private:
  [[nodiscard]] bool failed() const noexcept { return static_cast<bool>(status_); }

  void update_meta_status() {
    if (!meta_builder_.succeeded()) {
      status_ = meta_builder_.result();
    }
  }

  meta::class_builder<Class> meta_builder_;
  reflection::class_builder<Class> reflection_builder_;
  error status_{};
};

/** @brief 注册并描述一个 C++ 运行时类型。 */
template <typename Class> [[nodiscard]] class_builder<Class> class_(std::string_view name) {
  return class_builder<Class>{name};
}

/** @brief 组合式运行时枚举注册构建器。 */
template <typename Enum> class enum_builder {
  static_assert(std::is_enum_v<std::remove_cv_t<std::remove_reference_t<Enum>>>,
                "Enum must be an enum type");

public:
  explicit enum_builder(std::string_view name) : meta_builder_(meta::enum_<Enum>(name)) {
    update_meta_status();
  }

  enum_builder& guid(type_guid guid_value) {
    meta_builder_.guid(guid_value);
    update_meta_status();
    return *this;
  }

  enum_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  /** @brief 提交枚举注册；meta 类型在构造 builder 时已经注册。 */
  [[nodiscard]] error commit() const noexcept { return status_; }

  [[nodiscard]] runtime::type get() const noexcept { return runtime::type{meta_builder_.get()}; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept { return !status_; }

private:
  void update_meta_status() {
    if (!meta_builder_.succeeded()) {
      status_ = meta_builder_.result();
    }
  }

  meta::enum_builder<Enum> meta_builder_;
  error status_{};
};

/** @brief 注册并描述一个 C++ 运行时枚举类型。 */
template <typename Enum> [[nodiscard]] enum_builder<Enum> enum_(std::string_view name) {
  return enum_builder<Enum>{name};
}

} // namespace caors::runtime

#endif
