// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/core/string_name.h"
#include "caors/export.h"
#include "caors/meta/capability.h"
#include "caors/meta/metadata.h"
#include "caors/meta/type_id.h"
#include "caors/traits/wrapper.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

namespace caors::meta {

class registry;
class type;
class field;
class method;
class type_registration_data;

[[nodiscard]] CAORS_API registry& get_registry() noexcept;
template <typename T> [[nodiscard]] type resolve();
template <typename T> [[nodiscard]] type find();

namespace detail {

class type_data;
struct type_descriptor;

[[nodiscard]] CAORS_API type make_type_view(type_id id, const type_descriptor* descriptor) noexcept;

} // namespace detail

/** @brief 类型运行期形态。 */
enum class type_kind {
  value,
  enumeration,
  raw_pointer,
  wrapper,
  sequence,
  set,
  associative,
  pair,
  tuple,
  variant,
};

using wrapper_semantics = traits::wrapper_semantics;

/**
 * @brief 元类型只读视图。
 *
 * `meta::type` 不拥有类型数据，只引用编译期生成的 type descriptor，并在类型已注册时额外引用
 * 全局 `meta::registry` 中 append-only 保存的注册数据。视图复制成本低，适合作为对外查询结果。
 */
class CAORS_API type {
public:
  constexpr type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool registered() const noexcept;

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] type parent() const noexcept;
  [[nodiscard]] type_kind kind() const noexcept;
  [[nodiscard]] wrapper_semantics wrapper() const noexcept;
  [[nodiscard]] bool indirect() const noexcept;
  [[nodiscard]] bool nullable() const noexcept;
  [[nodiscard]] const metadata_map& metadata() const noexcept;
  [[nodiscard]] const metadata_value* find_metadata(metadata_key key) const noexcept;
  [[nodiscard]] type get_value_type() const noexcept;
  [[nodiscard]] type get_key_type() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] field field_at(std::size_t index) const;
  [[nodiscard]] field find_field(string_name name) const;
  [[nodiscard]] field find_field(std::string_view name) const;
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method method_at(std::size_t index) const;
  [[nodiscard]] method find_method(string_name name) const;
  [[nodiscard]] method find_method(std::string_view name) const;

  /**
   * @brief 查询该类型视图具备的能力。
   *
   * 默认返回 `const Capability*`；公开 facade 可通过 `type_capability_traits`
   * 特化返回轻量能力对象。
   */
  template <typename Capability> [[nodiscard]] auto get_capability() const noexcept {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    return type_capability_traits<capability_type>::get(*this);
  }

  /** @brief 判断该类型视图是否具备指定能力。 */
  template <typename Capability> [[nodiscard]] bool has_capability() const noexcept {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    return type_capability_traits<capability_type>::has(*this);
  }

  /**
   * @brief 查询该类型视图具备的 raw ops 函数表。
   *
   * `*_ops` 是 CAORS 内部和高级扩展使用的类型擦除函数表。普通实例访问应优先通过
   * `any_ref::get_capability<Capability>()` 获取绑定后的 view。
   */
  template <typename Ops> [[nodiscard]] const Ops* get_raw_ops() const noexcept {
    using ops_type = std::remove_cv_t<std::remove_reference_t<Ops>>;
    return type_capability_traits<ops_type>::get_raw(*this);
  }

  /** @brief 判断该类型视图是否具备指定 raw ops 函数表。 */
  template <typename Ops> [[nodiscard]] bool has_raw_ops() const noexcept {
    return get_raw_ops<Ops>() != nullptr;
  }

private:
  friend class registry;
  friend type detail::make_type_view(type_id, const detail::type_descriptor*) noexcept;
  friend const void* detail::find_type_capability(const type&, capability_key) noexcept;

  constexpr type(const registry* registry, const detail::type_data* data,
                 const detail::type_descriptor* descriptor) noexcept
      : registry_(registry), data_(data), descriptor_(descriptor) {}

  const registry* registry_ = nullptr;
  const detail::type_data* data_ = nullptr;
  const detail::type_descriptor* descriptor_ = nullptr;
};

} // namespace caors::meta

#endif
