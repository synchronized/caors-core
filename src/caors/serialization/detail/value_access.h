// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_
#define CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_

#include "caors/meta/argument.h"
#include "caors/meta/value/any_ref.h"
#include "caors/serialization/types.h"

#include <span>
#include <utility>

namespace caors::serialization::detail {

/** @brief 反序列化内部持有的运行期构造结果，负责失败路径回滚销毁。 */
struct owned_constructed_value {
  meta::released_owned_value_guard value{};
  bool transfer_destroy_context = false;

  owned_constructed_value() = default;

  owned_constructed_value(meta::owned_value constructed, bool transfer_context = false) noexcept
      : value(std::move(constructed).release()), transfer_destroy_context(transfer_context) {}

  owned_constructed_value(const owned_constructed_value&) = delete;
  owned_constructed_value& operator=(const owned_constructed_value&) = delete;
  owned_constructed_value(owned_constructed_value&& other) noexcept = default;
  owned_constructed_value& operator=(owned_constructed_value&& other) noexcept = default;

  [[nodiscard]] bool valid() const noexcept { return value.valid(); }
  [[nodiscard]] void* data() noexcept { return value.data(); }
  [[nodiscard]] const void* data() const noexcept { return value.data(); }
  [[nodiscard]] memory::memory_resource* resource() const noexcept { return value.resource(); }
  [[nodiscard]] meta::resource_destroy_fn resource_destroy() const noexcept {
    return value.resource_destroy();
  }
  [[nodiscard]] bool uses_resource_destroy() const noexcept {
    return value.valid() && resource_destroy() != nullptr && transfer_destroy_context;
  }

  void release() noexcept {
    value.dismiss();
    transfer_destroy_context = false;
  }

  void reset() noexcept {
    value.reset();
    transfer_destroy_context = false;
  }

  /** @brief 把当前构造结果转成拥有式 any；失败时仍由 guard 负责回滚销毁。 */
  [[nodiscard]] any into_any() noexcept {
    if (!valid()) {
      return {};
    }

    const meta::released_owned_value released = value.release();
    transfer_destroy_context = false;
    meta::owned_value owned =
        released.resource_destroy != nullptr
            ? meta::make_owned_value(released.type, released.value, released.resource,
                                     released.resource_destroy)
            : meta::make_owned_value(released.type, released.value, released.destroy);
    return std::move(owned).into_any();
  }
};

/** @brief 按序列化构造策略创建一个由调用方接管的运行期对象。 */
[[nodiscard]] inline owned_constructed_value
construct_owned_value(meta::type type, const serialization::context& context,
                      construction_site site = construction_site::value,
                      std::size_t index = construction_request_no_index,
                      bool include_static_default = true,
                      bool transfer_destroy_context = true) {
  const serialization::construction_options& options = context.effective_construction_options();
  const construction_request request = context.make_construction_request(type, site, index);
  memory::memory_resource* resource = context.effective_memory_resource();

  if (options.has_custom_construct()) {
    const construction_result result = options.create(request, context);
    if (result.valid()) {
      return {meta::make_owned_value(type, result.value, resource, result.destroy),
              transfer_destroy_context};
    }
  }

  if (options.use_registered_factory) {
    const meta::factory_capability factory = type.get_capability<meta::factory_capability>();
    if (factory) {
      meta::owned_value value = factory.construct_owned(resource);
      if (value.valid()) {
        return {std::move(value), transfer_destroy_context};
      }
    }
  }

  if (options.use_registered_constructor) {
    const meta::constructor_capability constructor =
        type.get_capability<meta::constructor_capability>();
    const std::span<const meta::argument> arguments{};
    if (constructor.matches(arguments.size())) {
      meta::owned_value value = constructor.construct_owned(arguments, resource);
      if (value.valid()) {
        return {std::move(value), transfer_destroy_context};
      }
    }
  }

  if (!include_static_default || !options.use_static_default) {
    return {};
  }

  const meta::value_storage_ops* storage = type.get_raw_ops<meta::value_storage_ops>();
  if (storage == nullptr || storage->ops.destroy == nullptr) {
    return {};
  }

  if (resource != nullptr && storage->ops.resource_construct != nullptr &&
      storage->ops.resource_destroy != nullptr) {
    return {meta::make_owned_value(type, storage->ops.resource_construct(resource), resource,
                                   storage->ops.resource_destroy),
            transfer_destroy_context};
  }

  return {meta::make_owned_value(
      type, storage->ops.default_construct != nullptr ? storage->ops.default_construct() : nullptr,
      storage->ops.destroy)};
}

/** @brief 按类型视图和序列化上下文构造临时运行期值。 */
[[nodiscard]] inline any construct_value(meta::type type, const serialization::context& context,
                                         construction_site site = construction_site::value,
                                         std::size_t index = construction_request_no_index) {
  owned_constructed_value constructed =
      construct_owned_value(type, context, site, index, false, true);
  if (constructed.valid()) {
    any decoded = constructed.into_any();
    if (decoded.valid()) {
      return decoded;
    }
  }

  if (!context.effective_construction_options().use_static_default) {
    return {};
  }

  memory::memory_resource* resource = context.effective_memory_resource();
  any decoded = any::default_construct(type, resource);
  if (!decoded.valid()) {
    decoded = any::default_construct(type.id(), resource);
  }

  return decoded;
}

} // namespace caors::serialization::detail

#endif
