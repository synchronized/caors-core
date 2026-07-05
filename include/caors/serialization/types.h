// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPES_H_
#define CAORS_SERIALIZATION_TYPES_H_

#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/result.h"
#include "caors/serialization/type_data.h"
#include "caors/value/variant.h"

#include <cstdint>

namespace caors::serialization {

enum class reference_identity {
  runtime_id,
  persistent_guid,
};

using object_to_id_fn = object_id (*)(const object* instance, void* user_data);
using id_to_object_fn = object* (*)(object_id id, void* user_data);
using object_to_guid_fn = object_guid (*)(const object* instance, void* user_data);
using guid_to_object_fn = object* (*)(object_guid guid, void* user_data);

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
  std::uint32_t schema_version = 0;
  const reference_resolver* reference_resolver = nullptr;
  /**
   * @brief `std::string_view` 反序列化使用的外部字符串存储。
   *
   * 默认保持 `nullptr`，此时读回 `std::string_view` 会返回 `errc::unsupported`，避免指向临时
   * 字符串。调用方如果能保证该存储的生命周期长于目标对象，可以传入一个可写字符串；读入时会把文本
   * 写入该字符串，再让目标 `std::string_view` 指向它。
   */
  string* string_view_storage = nullptr;
  /**
   * @brief 多态对象反序列化使用的对象生命周期管理器。
   *
   * `deserialize_polymorphic_object()` 会通过该 manager 创建动态对象。manager 持有的
   * `memory_resource` 负责实际分配，对象生命周期也由 manager 接管。
   */
  objects::manager* object_manager = nullptr;
  /**
   * @brief 普通 C++ 多态值反序列化时可选的内存资源。
   *
   * 只有目标 wrapper 能保存匹配销毁回调时，反序列化才会用该资源构造动态对象。否则保持标准
   * `new` / `delete` 语义，避免默认 deleter 与自定义资源释放策略不匹配。
   */
  memory::memory_resource* memory_resource = nullptr;
};

/** @brief 文本序列化结果。 */
using serialization_text_result = result<string>;

/** @brief 序列化中间值构造结果。 */
using variant_result = result<variant>;

} // namespace caors::serialization

#endif
