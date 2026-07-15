// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_TYPE_H_
#define CAORS_OBJECT_TYPE_H_

#include "caors/export.h"
#include "caors/meta/type.h"
#include "caors/object/factory_capability.h"
#include "caors/object/id.h"

namespace caors::objects {

class object;

class registry;

/** @brief 对象类型只读视图。 */
class CAORS_API type final {
public:
  type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type info() const noexcept;
  [[nodiscard]] object_type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] object_type_id parent_id() const noexcept;
  [[nodiscard]] bool constructible() const noexcept;

private:
  friend class registry;

  type(meta::type info, factory_capability factory) noexcept;
  [[nodiscard]] factory_capability factory() const noexcept;

  meta::type type_{};
  factory_capability factory_{};
};

} // namespace caors::objects

#endif
