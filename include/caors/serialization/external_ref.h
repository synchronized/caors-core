// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_EXTERNAL_REF_H_
#define CAORS_SERIALIZATION_EXTERNAL_REF_H_

#include "caors/core/guid.h"
#include "caors/core/string_name.h"
#include "caors/meta/type_id.h"

#include <cstdint>

namespace caors::serialization {

/** @brief 外部引用使用的身份值种类。 */
enum class external_ref_identity {
  guid,
  integer,
};

/**
 * @brief 外部引用。
 *
 * 序列化核心只理解“某个引用域中的身份值”。具体域由上层模块解释，例如资源模块使用
 * `domain == "resource"` 和稳定 GUID，对象系统可以使用 `domain == "object"` 并按上下文选择
 * 运行期整数 id 或持久 GUID。
 */
struct external_ref {
  string_name domain{};
  guid id{};
  type_id type{};
  std::uint64_t integer_id = 0;
  external_ref_identity identity = external_ref_identity::guid;
};

/** @brief 外部引用读回后的解析策略。 */
enum class external_ref_resolution {
  keep_unresolved,
  resolve_if_present,
  require_resolved,
};

/** @brief 外部引用解析函数表。 */
struct external_ref_resolver_ops {
  using resolve_fn = void* (*)(const external_ref& ref, void* user_data);
  using encode_fn = bool (*)(const void* instance, type_id type, external_ref& ref,
                             void* user_data);

  resolve_fn resolve = nullptr;
  void* user_data = nullptr;
  external_ref_identity identity = external_ref_identity::guid;
  encode_fn encode = nullptr;

  [[nodiscard]] constexpr bool can_resolve() const noexcept { return resolve != nullptr; }
  [[nodiscard]] constexpr bool can_encode() const noexcept { return encode != nullptr; }

  [[nodiscard]] void* operator()(const external_ref& ref) const {
    return resolve != nullptr ? resolve(ref, user_data) : nullptr;
  }

  [[nodiscard]] bool encode_ref(const void* instance, type_id type, external_ref& ref) const {
    return encode != nullptr && encode(instance, type, ref, user_data);
  }
};

/** @brief 外部引用收集函数表。 */
struct external_ref_collector_ops {
  using collect_fn = void (*)(const external_ref& ref, void* user_data);

  collect_fn collect = nullptr;
  void* user_data = nullptr;

  [[nodiscard]] constexpr bool can_collect() const noexcept { return collect != nullptr; }

  void operator()(const external_ref& ref) const {
    if (collect != nullptr) {
      collect(ref, user_data);
    }
  }
};

/** @brief 外部引用相关的序列化语义选项。 */
struct external_ref_options {
  const external_ref_resolver_ops* resolver = nullptr;
  const external_ref_collector_ops* collector = nullptr;
  external_ref_resolution resolution = external_ref_resolution::keep_unresolved;
};

/**
 * @brief 面向对象风格的外部引用解析接口。
 *
 * `serialization::context` 底层仍保存函数表。该接口只作为上层适配器，方便已有服务对象通过继承多态
 * 接入。
 */
class external_ref_resolver_interface {
public:
  external_ref_resolver_interface() = default;
  virtual ~external_ref_resolver_interface() = default;

  external_ref_resolver_interface(const external_ref_resolver_interface&) = delete;
  external_ref_resolver_interface& operator=(const external_ref_resolver_interface&) = delete;
  external_ref_resolver_interface(external_ref_resolver_interface&&) = delete;
  external_ref_resolver_interface& operator=(external_ref_resolver_interface&&) = delete;

  [[nodiscard]] virtual void* resolve(const external_ref& ref) = 0;
};

/**
 * @brief 面向对象风格的外部引用收集接口。
 */
class external_ref_collector_interface {
public:
  external_ref_collector_interface() = default;
  virtual ~external_ref_collector_interface() = default;

  external_ref_collector_interface(const external_ref_collector_interface&) = delete;
  external_ref_collector_interface& operator=(const external_ref_collector_interface&) = delete;
  external_ref_collector_interface(external_ref_collector_interface&&) = delete;
  external_ref_collector_interface& operator=(external_ref_collector_interface&&) = delete;

  virtual void collect(const external_ref& ref) = 0;
};

/** @brief 把继承式 resolver 适配为 context 使用的函数表。 */
[[nodiscard]] inline external_ref_resolver_ops
make_external_ref_resolver(external_ref_resolver_interface& resolver) noexcept {
  return external_ref_resolver_ops{
      [](const external_ref& ref, void* user_data) -> void* {
        auto* target = static_cast<external_ref_resolver_interface*>(user_data);
        return target != nullptr ? target->resolve(ref) : nullptr;
      },
      &resolver,
  };
}

/** @brief 把继承式 collector 适配为 context 使用的函数表。 */
[[nodiscard]] inline external_ref_collector_ops
make_external_ref_collector(external_ref_collector_interface& collector) noexcept {
  return external_ref_collector_ops{
      [](const external_ref& ref, void* user_data) {
        auto* target = static_cast<external_ref_collector_interface*>(user_data);
        if (target != nullptr) {
          target->collect(ref);
        }
      },
      &collector,
  };
}

} // namespace caors::serialization

#endif
