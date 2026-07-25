// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_ENUM_VIEW_H_
#define CAORS_META_VALUE_ENUM_VIEW_H_

#include "caors/meta/capability_bind.h"
#include "caors/meta/type_ops.h"

#include <cstdint>
#include <optional>

namespace caors::meta {

/**
 * @brief 绑定实际枚举实例的运行时访问视图。
 *
 * 视图不拥有枚举值。只读 source 可读取底层整数值；只有可写 source 才能写入。
 */
class CAORS_API enum_view final : public capability_view_base<enum_ops> {
public:
  using base_type = capability_view_base<enum_ops>;
  using base_type::base_type;

  enum_view() noexcept = default;

  [[nodiscard]] std::optional<std::int64_t> to_i64() const noexcept {
    const enum_ops* operations = ops();
    return operations != nullptr && operations->to_i64 != nullptr
               ? std::optional<std::int64_t>{operations->to_i64(data())}
               : std::nullopt;
  }

  [[nodiscard]] std::optional<std::uint64_t> to_u64() const noexcept {
    const enum_ops* operations = ops();
    return operations != nullptr && operations->to_u64 != nullptr
               ? std::optional<std::uint64_t>{operations->to_u64(data())}
               : std::nullopt;
  }

  bool from_i64(std::int64_t value) const noexcept {
    const enum_ops* operations = ops();
    void* target = mutable_data();
    return operations != nullptr && operations->from_i64 != nullptr && target != nullptr &&
           operations->from_i64(target, value);
  }

  bool from_u64(std::uint64_t value) const noexcept {
    const enum_ops* operations = ops();
    void* target = mutable_data();
    return operations != nullptr && operations->from_u64 != nullptr && target != nullptr &&
           operations->from_u64(target, value);
  }
};

} // namespace caors::meta

#endif
