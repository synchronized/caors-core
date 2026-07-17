// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_ENUM_VALUE_H_
#define CAORS_META_ENUM_VALUE_H_

#include "caors/core/string_name.h"
#include "caors/export.h"
#include "caors/meta/metadata.h"

#include <cstdint>
#include <optional>
#include <type_traits>

namespace caors::meta {

class type;
class enum_value_view;

namespace detail {

[[nodiscard]] CAORS_API enum_value_view make_enum_value_view(const void* enum_value) noexcept;

} // namespace detail

/**
 * @brief 枚举项只读视图。
 *
 * `meta::enum_value_view` 不拥有枚举项数据，只引用全局 `meta::registry`
 * 中的内部记录。枚举项按注册顺序保存，可通过名称或枚举值查询。
 */
class CAORS_API enum_value_view final {
public:
  enum_value_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] std::int64_t to_i64() const noexcept;
  [[nodiscard]] std::uint64_t to_u64() const noexcept;
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept;
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept;

  /** @brief 按指定枚举类型读取枚举项值。 */
  template <typename Enum> [[nodiscard]] std::optional<Enum> get() const noexcept {
    using enum_type = std::remove_cv_t<std::remove_reference_t<Enum>>;
    static_assert(std::is_enum_v<enum_type>, "meta::enum_value_view::get requires an enum type");

    if (!valid()) {
      return std::nullopt;
    }

    using underlying_type = std::underlying_type_t<enum_type>;
    if constexpr (std::is_signed_v<underlying_type>) {
      return static_cast<enum_type>(static_cast<underlying_type>(to_i64()));
    } else {
      return static_cast<enum_type>(static_cast<underlying_type>(to_u64()));
    }
  }

private:
  friend class type;
  friend enum_value_view detail::make_enum_value_view(const void*) noexcept;

  explicit enum_value_view(const void* enum_value) noexcept;
  [[nodiscard]] const void* info() const noexcept;

  const void* value_ = nullptr;
};

} // namespace caors::meta

#endif
