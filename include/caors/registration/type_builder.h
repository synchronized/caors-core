// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REGISTRATION_TYPE_BUILDER_H_
#define CAORS_REGISTRATION_TYPE_BUILDER_H_

#include "caors/meta/builder.h"
#include "caors/object/registry.h"
#include "caors/serialization/registry.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::registration {

template <typename T> class type_builder;
template <typename T> class meta_section;
template <typename T> class serialization_section;
template <typename T> class object_section;

/**
 * @brief 跨模块类型注册构建器。
 *
 * 该入口把 meta、serialization 和 object 的注册动作集中到同一个链式 API。当前实现复用各模块已有
 * builder；类型数据的“注册后不可变”语义会在后续 registry pending model 中继续收口。
 */
template <typename T> class type_builder {
public:
  type_builder(meta::context& ctx, std::string_view name)
      : name_(name), meta_builder_(meta::class_<T>(ctx, name_)) {}

  /** @brief 进入 meta 属性、方法和基础能力注册段。 */
  [[nodiscard]] meta_section<T> meta() noexcept { return meta_section<T>{*this}; }

  /** @brief 进入序列化 schema 注册段。 */
  [[nodiscard]] serialization_section<T> serialization() noexcept {
    serialization_enabled_ = true;
    return serialization_section<T>{*this};
  }

  /** @brief 进入对象类型注册段。 */
  template <typename U = T>
    requires(std::is_base_of_v<objects::object, U>)
  [[nodiscard]] object_section<T> object() noexcept {
    object_enabled_ = true;
    return object_section<T>{*this};
  }

  /** @brief 提交所有已配置模块注册动作。 */
  [[nodiscard]] error commit() {
    error status = meta_builder_.commit();
    if (failed(status)) {
      status_ = status;
      return status_;
    }

    if (serialization_enabled_) {
      status =
          serialization::class_builder{meta_builder_.get(), serialization_schema_}.commit();
      if (status) {
        status_ = status;
        return status_;
      }
    }

    if (object_enabled_) {
      status = commit_object();
      if (status) {
        status_ = status;
        return status_;
      }
    }

    status_ = {};
    return status_;
  }

  [[nodiscard]] meta::type get() const noexcept { return meta_builder_.get(); }
  [[nodiscard]] error result() const noexcept { return status_ ? status_ : meta_builder_.result(); }

private:
  friend class meta_section<T>;
  friend class serialization_section<T>;
  friend class object_section<T>;

  [[nodiscard]] static bool failed(error status) noexcept {
    return status && status.value() != errc::duplicate_id;
  }

  [[nodiscard]] error commit_object() {
    if constexpr (std::is_base_of_v<objects::object, T>) {
      auto builder = objects::class_<T>(name_);
      if (object_parent_id_) {
        builder.parent(object_parent_id_);
      }
      return builder.commit();
    } else {
      return {error_domain::object, errc::invalid_type};
    }
  }

  std::string name_{};
  meta::class_builder<T> meta_builder_;
  serialization::type_schema serialization_schema_{};
  objects::object_type_id object_parent_id_{};
  bool serialization_enabled_ = false;
  bool object_enabled_ = false;
  error status_{};
};

/** @brief `registration::type_builder` 的 meta 注册段。 */
template <typename T> class meta_section {
public:
  explicit meta_section(type_builder<T>& owner) noexcept : owner_(&owner) {}

  template <typename Member> meta_section& property(std::string_view name, Member member) {
    owner_->meta_builder_.property(name, member);
    return *this;
  }

  template <typename Member>
  meta_section& property(std::string_view name, Member member, std::string_view type_name) {
    owner_->meta_builder_.property(name, member, type_name);
    return *this;
  }

  template <typename Getter, typename Setter>
  meta_section& property(std::string_view name, Getter getter, Setter setter)
    requires(!std::is_convertible_v<Setter, std::string_view>)
  {
    owner_->meta_builder_.property(name, getter, setter);
    return *this;
  }

  template <typename Getter>
  meta_section& property_readonly(std::string_view name, Getter getter) {
    owner_->meta_builder_.property_readonly(name, getter);
    return *this;
  }

  template <typename Member> meta_section& method(std::string_view name, Member member) {
    owner_->meta_builder_.method(name, member);
    return *this;
  }

  template <typename Capability> meta_section& capability(const Capability* capability) {
    owner_->meta_builder_.capability(capability);
    return *this;
  }

  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  meta_section& capability(Capability&& capability) {
    owner_->meta_builder_.capability(std::forward<Capability>(capability));
    return *this;
  }

  template <typename Capability> meta_section& capability() {
    owner_->meta_builder_.template capability<Capability>();
    return *this;
  }

  template <typename Value> meta_section& metadata(std::string_view key, Value&& value) {
    owner_->meta_builder_.metadata(key, std::forward<Value>(value));
    return *this;
  }

  [[nodiscard]] serialization_section<T> serialization() noexcept {
    return owner_->serialization();
  }

  template <typename U = T>
    requires(std::is_base_of_v<objects::object, U>)
  [[nodiscard]] object_section<T> object() noexcept {
    return owner_->object();
  }

  [[nodiscard]] error commit() { return owner_->commit(); }

private:
  type_builder<T>* owner_ = nullptr;
};

/** @brief `registration::type_builder` 的序列化 schema 注册段。 */
template <typename T> class serialization_section {
public:
  explicit serialization_section(type_builder<T>& owner) noexcept : owner_(&owner) {}

  serialization_section& field(std::string_view field_name, std::string_view serialized_name = {},
                               bool skipped = false, std::uint32_t field_id = 0) {
    owner_->serialization_schema_.fields.push_back(
        serialization::make_field(field_name, serialized_name, skipped, field_id));
    return *this;
  }

  [[nodiscard]] meta_section<T> meta() noexcept { return owner_->meta(); }

  template <typename U = T>
    requires(std::is_base_of_v<objects::object, U>)
  [[nodiscard]] object_section<T> object() noexcept {
    return owner_->object();
  }

  [[nodiscard]] error commit() { return owner_->commit(); }

private:
  type_builder<T>* owner_ = nullptr;
};

/** @brief `registration::type_builder` 的对象类型注册段。 */
template <typename T> class object_section {
public:
  explicit object_section(type_builder<T>& owner) noexcept : owner_(&owner) {}

  object_section& parent(objects::object_type_id parent_id) noexcept {
    owner_->object_parent_id_ = parent_id;
    return *this;
  }

  [[nodiscard]] meta_section<T> meta() noexcept { return owner_->meta(); }
  [[nodiscard]] serialization_section<T> serialization() noexcept { return owner_->serialization(); }
  [[nodiscard]] error commit() { return owner_->commit(); }

private:
  type_builder<T>* owner_ = nullptr;
};

/** @brief 创建跨模块类型注册构建器。 */
template <typename T>
[[nodiscard]] type_builder<T> type_(meta::context& ctx, std::string_view name) {
  return type_builder<T>{ctx, name};
}

/** @brief 使用默认上下文创建跨模块类型注册构建器。 */
template <typename T> [[nodiscard]] type_builder<T> type_(std::string_view name) {
  return type_<T>(meta::default_context(), name);
}

} // namespace caors::registration

#endif
