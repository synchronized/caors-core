// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPES_H_
#define CAORS_SERIALIZATION_TYPES_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/meta.h"
#include "caors/result.h"
#include "caors/serialization/external_ref.h"
#include "caors/serialization/type_schema.h"
#include "caors/value/variant.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace caors::serialization {

using type_create_fn = void* (*)(type_id type, memory::memory_resource* resource,
                                 void* user_data);
using type_destroy_fn = void (*)(void* instance, void* user_data) noexcept;

/** @brief 按运行期类型构造和销毁实例的函数表。 */
struct type_factory {
  type_create_fn create = nullptr;
  type_destroy_fn destroy = nullptr;
  void* user_data = nullptr;

  [[nodiscard]] constexpr bool can_create() const noexcept { return create != nullptr; }
  [[nodiscard]] constexpr bool can_destroy() const noexcept { return destroy != nullptr; }
  [[nodiscard]] constexpr bool valid() const noexcept {
    return can_create() && can_destroy();
  }

  [[nodiscard]] void* create_type(type_id type, memory::memory_resource* resource) const {
    return create != nullptr ? create(type, resource, user_data) : nullptr;
  }

  void destroy_type(void* instance) const noexcept {
    if (destroy != nullptr && instance != nullptr) {
      destroy(instance, user_data);
    }
  }
};

/**
 * @brief 多态对象和值 wrapper 的 envelope 输出配置。
 *
 * 字段名保持空值时使用默认 `$type`、`$value` 和 `$type_name`。`$type` 始终写入稳定
 * `type_guid`，`$type_name` 只用于调试输出，默认不写出，读入时也不作为类型身份依据。
 */
struct polymorphic_options {
  string_name type_field{};
  string_name value_field{};
  string_name type_name_field{};
  bool write_type_name = false;
};

/** @brief JSON writer / reader 的格式层选项。 */
struct json_options {
  /** @brief 是否输出换行和缩进；默认输出紧凑 JSON。 */
  bool pretty = false;
  /** @brief `pretty` 开启时每一层使用的空格数。 */
  std::uint32_t indent = 2;
};

/** @brief 字段和 schema 相关的序列化语义选项。 */
struct field_options {
  /**
   * @brief 当前读写使用的 schema version。
   *
   * `0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前版本可见的字段。
   */
  std::uint32_t schema_version = 0;
};

/**
 * @brief `std::string_view` 反序列化使用的多段字符串存储。
 *
 * `std::string_view` 不拥有字符数据。反序列化读回 view 字段时，调用方应让该 storage 的生命周期
 * 长于目标对象中所有读回的 view。每次 `store()` 都会追加一段独立字符串，避免多个 view
 * 字段互相覆盖。
 */
struct string_storage {
  explicit string_storage(memory::memory_resource* resource = nullptr)
      : values_(core::make_list<string>(resource)) {}

  [[nodiscard]] std::string_view store(string value) {
    values_.push_back(std::move(value));
    const string& stored = values_.back();
    return {stored.data(), stored.size()};
  }

  void clear() noexcept { values_.clear(); }
  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

private:
  list<string> values_;
};

/** @brief 运行期工厂相关的序列化语义选项。 */
struct factory_options {
  const type_factory* type_factory = nullptr;
};

/** @brief 运行期构造和销毁相关的序列化语义选项。 */
struct memory_options {
  memory::memory_resource* resource = nullptr;
};

/** @brief 借用视图反序列化相关的外部存储选项。 */
struct storage_options {
  string_storage* string_view_storage = nullptr;
};

/**
 * @brief 单次序列化调度上下文。
 *
 * `schema_version == 0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前版本可见的
 * 字段。
 */
struct context {
  field_options fields{};
  external_ref_options external_refs{};
  factory_options factories{};
  memory_options memory{};
  storage_options storage{};
  polymorphic_options polymorphic{};

  [[nodiscard]] std::uint32_t effective_schema_version() const noexcept {
    return fields.schema_version;
  }

  [[nodiscard]] const serialization::external_ref_resolver*
  effective_external_ref_resolver() const noexcept {
    return external_refs.resolver;
  }

  [[nodiscard]] const serialization::external_ref_collector*
  effective_external_ref_collector() const noexcept {
    return external_refs.collector;
  }

  [[nodiscard]] external_ref_resolution effective_external_ref_resolution() const noexcept {
    return external_refs.resolution;
  }

  [[nodiscard]] string_storage* effective_string_view_storage() const noexcept {
    return storage.string_view_storage;
  }

  [[nodiscard]] const serialization::type_factory* effective_type_factory() const noexcept {
    return factories.type_factory;
  }

  [[nodiscard]] memory::memory_resource* effective_memory_resource() const noexcept {
    return memory.resource;
  }
};

/** @brief 文本序列化结果。 */
using serialization_text_result = result<string>;

/** @brief 序列化中间值构造结果。 */
using variant_result = result<variant>;

} // namespace caors::serialization

namespace caors {

using type_factory = serialization::type_factory;

} // namespace caors

#endif
