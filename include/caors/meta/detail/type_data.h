// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/error.h"
#include "caors/meta/detail/reflection_data.h"
#include "caors/meta/type_registration.h"

#include <optional>
#include <utility>

namespace caors::meta::detail {

class registry_state;

/** @brief 注册表内部保存的完整 meta 类型数据。 */
class type_data {
public:
  type_data() = default;
  type_data(meta::type_registration_data registration) noexcept
      : registration_(registration) {}

  [[nodiscard]] type_id id() const noexcept { return registration_.id(); }
  [[nodiscard]] string_name name() const noexcept { return registration_.name(); }
  [[nodiscard]] type_guid guid() const noexcept { return registration_.guid(); }
  [[nodiscard]] type_id parent_id() const noexcept { return registration_.parent_id(); }

  [[nodiscard]] bool valid() const noexcept { return registration_.valid(); }
  [[nodiscard]] bool registered() const noexcept { return registration_.registered(); }

  void set_id(type_id id) noexcept { registration_.set_id(id); }
  void set_name(string_name name) noexcept { registration_.set_name(name); }
  void set_guid(type_guid guid) noexcept { registration_.set_guid(guid); }
  void set_parent_id(type_id parent_id) noexcept { registration_.set_parent_id(parent_id); }

  [[nodiscard]] const reflection_data* reflection() const noexcept {
    return reflection_.has_value() ? &*reflection_ : nullptr;
  }

  [[nodiscard]] reflection_data* mutable_reflection() noexcept {
    return reflection_.has_value() ? &*reflection_ : nullptr;
  }

  [[nodiscard]] error attach_reflection(reflection_data reflection) {
    if (reflection_.has_value()) {
      return {error_domain::reflection, errc::duplicate_reflection};
    }

    reflection_.emplace(std::move(reflection));
    return {};
  }

  [[nodiscard]] reflection_data* ensure_reflection(reflection_data reflection) {
    if (!reflection_.has_value()) {
      reflection_.emplace(std::move(reflection));
    }

    return &*reflection_;
  }

private:
  meta::type_registration_data registration_{};
  std::optional<reflection_data> reflection_{};
};

[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return meta::make_type_registration_data<T>(name, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return meta::make_type_registration_data<T>(name, guid, parent_id);
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(std::string_view name,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_data<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(std::string_view name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_data<T>(string_name{name}, guid, parent_id);
}

} // namespace caors::meta::detail

#endif
