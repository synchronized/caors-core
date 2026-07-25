// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_ASSOCIATIVE_ENTRY_REF_H_
#define CAORS_META_VALUE_ASSOCIATIVE_ENTRY_REF_H_

#include "caors/meta/value/any_ref.h"

namespace caors::meta {

/** @brief 关联容器 key/value 引用。 */
class CAORS_API associative_entry_ref final {
public:
  associative_entry_ref() noexcept = default;
  associative_entry_ref(any_ref key, any_ref value) noexcept : key_(key), value_(value) {}

  [[nodiscard]] any_ref key() const noexcept { return key_; }
  [[nodiscard]] any_ref value() const noexcept { return value_; }
  [[nodiscard]] any_ref mapped() const noexcept { return value_; }

private:
  any_ref key_{};
  any_ref value_{};
};

} // namespace caors::meta

#endif
