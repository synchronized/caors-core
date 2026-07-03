// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_TYPE_H_
#define CAORS_OBJECT_TYPE_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/meta/type.h"
#include "caors/object/id.h"

namespace caors {

class object;

namespace objects {

class registry;

using try_create_fn = object* (*)(memory_resource * resource) noexcept;
using create_fn = object* (*)(memory_resource * resource);
using destroy_fn = void (*)(object* instance, memory_resource* resource) noexcept;

/** @brief 对象类型底层数据。 */
struct type_data {
  try_create_fn try_create = nullptr;
  create_fn create = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool constructible() const noexcept {
    return try_create != nullptr && create != nullptr;
  }
};

/** @brief 对象类型只读视图。 */
class CAORS_API type final {
public:
  type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type_view info() const noexcept;
  [[nodiscard]] object_type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] object_type_id parent_id() const noexcept;
  [[nodiscard]] bool constructible() const noexcept;

private:
  friend class registry;

  type(meta::type_view info, const type_data* data) noexcept;
  [[nodiscard]] const type_data* data() const noexcept;

  meta::type_view type_{};
  const type_data* data_ = nullptr;
};

} // namespace objects

} // namespace caors

#endif
