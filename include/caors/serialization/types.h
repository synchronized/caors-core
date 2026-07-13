// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPES_H_
#define CAORS_SERIALIZATION_TYPES_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/result.h"
#include "caors/serialization/external_ref.h"
#include "caors/serialization/type_schema.h"
#include "caors/value/variant.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace caors::serialization {

enum class reference_identity {
  runtime_id,
  persistent_guid,
};

using object_to_id_fn = object_id (*)(const object* instance, void* user_data);
using id_to_object_fn = object* (*)(object_id id, void* user_data);
using object_to_guid_fn = object_guid (*)(const object* instance, void* user_data);
using guid_to_object_fn = object* (*)(object_guid guid, void* user_data);

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

/** @brief 对象引用解析器。 */
struct reference_resolver {
  object_to_id_fn object_to_id = nullptr;
  id_to_object_fn id_to_object = nullptr;
  void* user_data = nullptr;
  reference_identity identity = reference_identity::runtime_id;
  object_to_guid_fn object_to_guid = nullptr;
  guid_to_object_fn guid_to_object = nullptr;

  [[nodiscard]] constexpr bool uses_runtime_id() const noexcept {
    return identity == reference_identity::runtime_id;
  }

  [[nodiscard]] constexpr bool uses_persistent_guid() const noexcept {
    return identity == reference_identity::persistent_guid;
  }

  [[nodiscard]] constexpr bool can_write() const noexcept {
    return uses_runtime_id() ? object_to_id != nullptr : object_to_guid != nullptr;
  }

  [[nodiscard]] constexpr bool can_read() const noexcept {
    return uses_runtime_id() ? id_to_object != nullptr : guid_to_object != nullptr;
  }
};

/** @brief 对象引用相关的序列化语义选项。 */
struct reference_options {
  const reference_resolver* object_resolver = nullptr;
};

/** @brief 对象生命周期管理相关的序列化语义选项。 */
struct object_options {
  objects::manager* manager = nullptr;
};

/** @brief 运行期构造和销毁相关的序列化语义选项。 */
struct memory_options {
  memory::memory_resource* resource = nullptr;
};

/** @brief 借用视图反序列化相关的外部存储选项。 */
struct storage_options {
  string_storage* string_view_storage = nullptr;
};

namespace detail {

[[nodiscard]] inline object_id manager_object_to_id(const object* instance,
                                                    void* user_data) noexcept {
  auto* manager = static_cast<objects::manager*>(user_data);
  if (manager == nullptr || instance == nullptr || manager->find(instance->id()) != instance) {
    return object_id::invalid();
  }

  return instance->id();
}

[[nodiscard]] inline object* manager_id_to_object(object_id id, void* user_data) noexcept {
  auto* manager = static_cast<objects::manager*>(user_data);
  return manager != nullptr ? manager->find(id) : nullptr;
}

[[nodiscard]] inline object_guid manager_object_to_guid(const object* instance,
                                                        void* user_data) noexcept {
  auto* manager = static_cast<objects::manager*>(user_data);
  if (manager == nullptr || instance == nullptr || manager->find(instance->id()) != instance) {
    return object_guid::nil();
  }

  return manager->guid_of(instance->id());
}

[[nodiscard]] inline object* manager_guid_to_object(object_guid guid, void* user_data) noexcept {
  auto* manager = static_cast<objects::manager*>(user_data);
  return manager != nullptr ? manager->find(guid) : nullptr;
}

} // namespace detail

/** @brief 使用对象生命周期管理器创建对象引用 resolver。 */
[[nodiscard]] inline reference_resolver
make_reference_resolver(objects::manager& manager) noexcept {
  return reference_resolver{
      &detail::manager_object_to_id,
      &detail::manager_id_to_object,
      &manager,
      reference_identity::runtime_id,
      nullptr,
      nullptr,
  };
}

/** @brief 使用对象生命周期管理器和对象索引创建持久对象引用 resolver。 */
[[nodiscard]] inline reference_resolver
make_persistent_reference_resolver(objects::manager& manager) noexcept {
  return reference_resolver{
      nullptr,
      nullptr,
      &manager,
      reference_identity::persistent_guid,
      &detail::manager_object_to_guid,
      &detail::manager_guid_to_object,
  };
}

/**
 * @brief 单次序列化调度上下文。
 *
 * `schema_version == 0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前版本可见的
 * 字段。
 */
struct context {
  field_options fields{};
  reference_options references{};
  external_ref_options external_refs{};
  object_options objects{};
  memory_options memory{};
  storage_options storage{};
  polymorphic_options polymorphic{};

  [[nodiscard]] std::uint32_t effective_schema_version() const noexcept {
    return fields.schema_version;
  }

  [[nodiscard]] const serialization::reference_resolver*
  effective_reference_resolver() const noexcept {
    return references.object_resolver;
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

  [[nodiscard]] objects::manager* effective_object_manager() const noexcept {
    return objects.manager;
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

#endif
