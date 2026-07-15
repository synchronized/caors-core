// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_
#define CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_

#include "caors/meta/value/any_ref.h"
#include "caors/serialization/types.h"

#include <span>

namespace caors::serialization::detail {

/** @brief 反序列化内部持有的运行期构造结果，负责失败路径回滚销毁。 */
struct owned_constructed_value {
  void* value = nullptr;
  memory::memory_resource* resource = nullptr;
  meta::value_ops::resource_destroy_fn resource_destroy = nullptr;
  meta::value_ops::destroy_fn default_destroy = nullptr;
  bool transfer_destroy_context = false;

  owned_constructed_value() = default;

  owned_constructed_value(void* constructed, memory::memory_resource* memory,
                          meta::value_ops::resource_destroy_fn destroy,
                          bool transfer_context) noexcept
      : value(constructed), resource(memory), resource_destroy(destroy),
        transfer_destroy_context(transfer_context) {}

  owned_constructed_value(void* constructed, meta::value_ops::destroy_fn destroy) noexcept
      : value(constructed), default_destroy(destroy) {}

  owned_constructed_value(const owned_constructed_value&) = delete;
  owned_constructed_value& operator=(const owned_constructed_value&) = delete;

  owned_constructed_value(owned_constructed_value&& other) noexcept
      : value(other.value), resource(other.resource), resource_destroy(other.resource_destroy),
        default_destroy(other.default_destroy),
        transfer_destroy_context(other.transfer_destroy_context) {
    other.release();
  }

  owned_constructed_value& operator=(owned_constructed_value&& other) noexcept {
    if (this != &other) {
      reset();
      value = other.value;
      resource = other.resource;
      resource_destroy = other.resource_destroy;
      default_destroy = other.default_destroy;
      transfer_destroy_context = other.transfer_destroy_context;
      other.release();
    }

    return *this;
  }

  ~owned_constructed_value() { reset(); }

  [[nodiscard]] bool valid() const noexcept { return value != nullptr; }
  [[nodiscard]] bool uses_resource_destroy() const noexcept {
    return value != nullptr && resource_destroy != nullptr && transfer_destroy_context;
  }

  void release() noexcept {
    value = nullptr;
    resource = nullptr;
    resource_destroy = nullptr;
    default_destroy = nullptr;
    transfer_destroy_context = false;
  }

  void reset() noexcept {
    if (value == nullptr) {
      return;
    }

    if (resource_destroy != nullptr) {
      resource_destroy(resource, value);
    } else if (default_destroy != nullptr) {
      default_destroy(value);
    }

    release();
  }
};

/** @brief 按序列化构造策略创建一个由调用方接管的运行期对象。 */
[[nodiscard]] inline owned_constructed_value
construct_owned_value(meta::type type, const serialization::context& context,
                      construction_site site = construction_site::value,
                      std::size_t index = construction_request_no_index,
                      bool include_descriptor_default = true,
                      bool transfer_destroy_context = true) {
  const serialization::construction_options& options = context.effective_construction_options();
  const construction_request request = context.make_construction_request(type, site, index);
  memory::memory_resource* resource = context.effective_memory_resource();

  if (options.has_custom_construct()) {
    const construction_result result = options.create(request, context);
    if (result.valid()) {
      return {result.value, resource, result.destroy, transfer_destroy_context};
    }
  }

  if (options.use_registered_factory) {
    const meta::factory_capability factory = type.get_capability<meta::factory_capability>();
    if (factory) {
      void* value = factory.construct(resource);
      if (value != nullptr) {
        return {value, resource, factory.destroy_fn(), transfer_destroy_context};
      }
    }
  }

  if (options.use_registered_constructor) {
    const meta::constructor_capability constructor =
        type.get_capability<meta::constructor_capability>();
    const std::span<const meta::any_ref> arguments{};
    if (constructor.matches(arguments.size())) {
      void* value = constructor.construct(resource, arguments);
      if (value != nullptr) {
        return {value, resource, constructor.destroy_fn(), transfer_destroy_context};
      }
    }
  }

  if (!include_descriptor_default || !options.use_descriptor_default) {
    return {};
  }

  const meta::value_storage_ops* storage = type.get_raw_ops<meta::value_storage_ops>();
  if (storage == nullptr || storage->ops.destroy == nullptr) {
    return {};
  }

  if (resource != nullptr && storage->ops.resource_construct != nullptr &&
      storage->ops.resource_destroy != nullptr) {
    return {storage->ops.resource_construct(resource), resource, storage->ops.resource_destroy,
            transfer_destroy_context};
  }

  return {storage->ops.default_construct != nullptr ? storage->ops.default_construct() : nullptr,
          storage->ops.destroy};
}

/** @brief 按类型视图和序列化上下文构造临时运行期值。 */
[[nodiscard]] inline any construct_value(meta::type type, const serialization::context& context,
                                         construction_site site = construction_site::value,
                                         std::size_t index = construction_request_no_index) {
  owned_constructed_value constructed =
      construct_owned_value(type, context, site, index, false, true);
  if (constructed.valid() && constructed.resource_destroy != nullptr) {
    any decoded = any::take_ownership(type, constructed.value, constructed.resource,
                                      constructed.resource_destroy);
    if (decoded.valid()) {
      constructed.release();
      return decoded;
    }
  }

  if (!context.effective_construction_options().use_descriptor_default) {
    return {};
  }

  memory::memory_resource* resource = context.effective_memory_resource();
  any decoded = any::default_construct(type, resource);
  if (!decoded.valid()) {
    decoded = any::default_construct(type.id(), resource);
  }

  return decoded;
}

/** @brief 兼容旧内部命名；语义已包含 resolver、factory 和 constructor。 */
[[nodiscard]] inline any default_construct_value(meta::type type,
                                                 const serialization::context& context) {
  return construct_value(type, context);
}

} // namespace caors::serialization::detail

#endif
