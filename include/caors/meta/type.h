// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/core/string_name.h"
#include "caors/export.h"
#include "caors/meta/capability.h"
#include "caors/meta/metadata.h"
#include "caors/meta/ops/scalar_ops.h"
#include "caors/meta/type_ops.h"
#include "caors/meta/type_id.h"
#include "caors/meta/value_ops.h"
#include "caors/traits/wrapper.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

namespace caors::memory {
class memory_resource;
} // namespace caors::memory

namespace caors::meta {

class any;
class argument;
class context;
class registry;
class type;
class property;
class method;
class method_set;
struct associative_ops;
struct enum_ops;
struct indirect_ops;
struct pair_ops;
struct sequence_ops;
struct set_ops;
struct tuple_ops;
struct variant_ops;

[[nodiscard]] CAORS_API registry& get_registry() noexcept;
[[nodiscard]] CAORS_API context& default_context() noexcept;
template <typename T> [[nodiscard]] type resolve();
template <typename T> [[nodiscard]] type find();

namespace detail {

class type_data;
struct type_descriptor;
class type_access;

enum class builtin_ops_kind {
  value_storage,
  scalar,
  enumeration,
  indirect,
  sequence,
  set,
  associative,
  pair,
  tuple,
  variant,
};

[[nodiscard]] CAORS_API const void* find_type_builtin_ops(const type& source,
                                                          builtin_ops_kind kind) noexcept;

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

/** @brief 类型成员查询范围。 */
enum class member_search {
  local_only,
  include_bases,
};

using wrapper_semantics = traits::wrapper_semantics;

/**
 * @brief 元类型只读视图。
 *
 * `meta::type` 不拥有类型数据。它可以表示未注册 C++ 类型，也可以在类型已注册时引用
 * `meta::registry` 中 append-only 保存的注册数据。视图复制成本低，适合作为对外查询结果。
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
  /** @brief 查询类型是否是已注册的 `meta::object` 类型或其派生类型。 */
  [[nodiscard]] bool is_object() const noexcept;
  [[nodiscard]] scalar_kind scalar() const noexcept;
  [[nodiscard]] bool is_scalar() const noexcept;
  [[nodiscard]] wrapper_semantics wrapper() const noexcept;
  [[nodiscard]] bool indirect() const noexcept;
  [[nodiscard]] bool nullable() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  /**
   * @brief 查询该类型是否能通过高层创建入口构造运行期值。
   *
   * 调用创建 API 时应包含 `caors/meta/type_creation.h` 或 `caors/meta.h`，以获得
   * `meta::any` 的完整定义。
   */
  [[nodiscard]] bool constructible() const noexcept;
  /** @brief 查询该类型是否能使用指定参数数量通过高层创建入口构造运行期值。 */
  [[nodiscard]] bool constructible(std::size_t argument_count) const noexcept;
  /** @brief 查询类型是否支持默认值构造能力。 */
  [[nodiscard]] bool value_constructible() const noexcept;
  /** @brief 创建运行期值，优先 factory，再匹配无参 constructor，最后回退默认值构造能力。 */
  [[nodiscard]] any create(memory::memory_resource* resource = nullptr) const;
  /** @brief 使用运行期参数列表创建运行期值；非空参数列表只匹配 constructor capability。 */
  [[nodiscard]] any create(std::span<const argument> arguments,
                           memory::memory_resource* resource = nullptr) const;
  /** @brief 使用类型默认值构造能力创建运行期值，不查询 factory capability。 */
  [[nodiscard]] any create_value(memory::memory_resource* resource = nullptr) const;
  [[nodiscard]] const metadata_map& metadata() const noexcept;
  [[nodiscard]] const metadata_value* find_metadata(metadata_key key) const noexcept;
  [[nodiscard]] type get_value_type() const noexcept;
  [[nodiscard]] type get_key_type() const noexcept;
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] property property_at(std::size_t index) const;
  [[nodiscard]] property find_property(string_name name,
                                       member_search search = member_search::local_only) const;
  [[nodiscard]] property find_property(std::string_view name,
                                       member_search search = member_search::local_only) const;
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method_set method_at(std::size_t index) const;
  [[nodiscard]] method_set find_method(string_name name,
                                       member_search search = member_search::local_only) const;
  [[nodiscard]] method_set find_method(std::string_view name,
                                       member_search search = member_search::local_only) const;
  /** @brief 查询本类型声明的 nested 类型关系数量；不包含父类型声明的 nested 关系。 */
  [[nodiscard]] std::size_t nested_count() const noexcept;
  /** @brief 查询指定 nested 关系在 owner 内部使用的短名；索引越界时返回空名。 */
  [[nodiscard]] string_name nested_name_at(std::size_t index) const noexcept;
  /** @brief 按索引查询 nested 类型；目标类型未注册或索引越界时返回空视图。 */
  [[nodiscard]] type nested_at(std::size_t index) const;
  /** @brief 按 owner 内部短名查询 nested 类型；未命中时返回空视图。 */
  [[nodiscard]] type find_nested(string_name name) const;
  /** @brief 按 owner 内部短名查询 nested 类型；未命中时返回空视图。 */
  [[nodiscard]] type find_nested(std::string_view name) const;

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
   * `*_ops` 是 CAORS 内部和高级扩展使用的类型擦除函数表，不是普通业务 API。
   * 普通实例访问应优先通过 `any_ref::get_capability<Capability>()` 获取绑定后的 view；
   * 只有实现新的 capability facade、跨模块桥接或性能敏感适配层时才应直接使用本入口。
   */
  template <typename Ops> [[nodiscard]] const Ops* get_raw_ops() const noexcept {
    using ops_type = std::remove_cv_t<std::remove_reference_t<Ops>>;
    if constexpr (std::is_same_v<ops_type, value_storage_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::value_storage));
    } else if constexpr (std::is_same_v<ops_type, scalar_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::scalar));
    } else if constexpr (std::is_same_v<ops_type, enum_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::enumeration));
    } else if constexpr (std::is_same_v<ops_type, indirect_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::indirect));
    } else if constexpr (std::is_same_v<ops_type, sequence_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::sequence));
    } else if constexpr (std::is_same_v<ops_type, set_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::set));
    } else if constexpr (std::is_same_v<ops_type, associative_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::associative));
    } else if constexpr (std::is_same_v<ops_type, pair_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::pair));
    } else if constexpr (std::is_same_v<ops_type, tuple_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::tuple));
    } else if constexpr (std::is_same_v<ops_type, variant_ops>) {
      return static_cast<const ops_type*>(
          detail::find_type_builtin_ops(*this, detail::builtin_ops_kind::variant));
    } else {
      return type_capability_traits<ops_type>::get_raw(*this);
    }
  }

  /** @brief 判断该类型视图是否具备指定 raw ops 函数表。 */
  template <typename Ops> [[nodiscard]] bool has_raw_ops() const noexcept {
    return get_raw_ops<Ops>() != nullptr;
  }

private:
  friend class detail::type_access;

  constexpr type(const registry* registry, const detail::type_data* data,
                 const detail::type_descriptor* descriptor) noexcept
      : registry_(registry), data_(data), descriptor_(descriptor) {}

  const registry* registry_ = nullptr;
  const detail::type_data* data_ = nullptr;
  const detail::type_descriptor* descriptor_ = nullptr;
};

} // namespace caors::meta

#endif
