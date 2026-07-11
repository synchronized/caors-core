// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CAPABILITY_REGISTRY_H_
#define CAORS_META_DETAIL_CAPABILITY_REGISTRY_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta {

class type;
using capability_key = const void*;

namespace detail {

/** @brief 运行期索引拥有的 capability 基类。内部用于保存用户自定义 concept table。 */
class owned_capability_base {
public:
  virtual ~owned_capability_base() = default;

  [[nodiscard]] virtual const void* data() const noexcept = 0;
};

/** @brief 保存一份具体 capability 对象，并向运行期索引暴露稳定只读指针。 */
template <typename Capability> class owned_capability final : public owned_capability_base {
public:
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;

  explicit owned_capability(capability_type capability) : capability_(std::move(capability)) {}

  [[nodiscard]] const void* data() const noexcept override { return &capability_; }

private:
  capability_type capability_;
};

/** @brief 用 CAORS 内存资源构造一份可按基类保存的 owned capability。 */
template <typename Capability>
[[nodiscard]] memory::unique_ptr<owned_capability_base>
make_owned_capability(Capability capability) {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using storage_type = owned_capability<capability_type>;

  memory::memory_resource* resource = memory::get_default_memory_resource();
  auto* storage = memory::make<storage_type>(resource, capability_type{std::move(capability)});
  memory::unique_ptr<owned_capability_base> result{storage};
  result.get_deleter().reset(resource, [](void* user_data, void* value) noexcept {
    memory::destroy(static_cast<storage_type*>(value),
                    static_cast<memory::memory_resource*>(user_data));
  });
  return result;
}

[[nodiscard]] CAORS_API error register_capability(type_id id, capability_key key,
                                                  const void* capability);
[[nodiscard]] CAORS_API error register_owned_capability(
    type_id id, capability_key key, memory::unique_ptr<owned_capability_base> capability);
[[nodiscard]] CAORS_API const void* find_capability(type_id id, capability_key key) noexcept;
[[nodiscard]] CAORS_API const void* find_type_capability(const meta::type& type,
                                                         capability_key key) noexcept;
[[nodiscard]] CAORS_API bool contains_capability(type_id id, capability_key key) noexcept;
[[nodiscard]] CAORS_API std::size_t capability_count(capability_key key) noexcept;

} // namespace detail
} // namespace caors::meta

#endif
