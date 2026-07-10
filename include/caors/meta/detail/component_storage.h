// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_COMPONENT_STORAGE_H_
#define CAORS_META_DETAIL_COMPONENT_STORAGE_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/type.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

using meta_component_key = const void*;

template <typename Component> [[nodiscard]] meta_component_key meta_component_key_of() noexcept {
  static const int key = 0;
  return &key;
}

class meta_component_storage_base {
public:
  virtual ~meta_component_storage_base() = default;

  [[nodiscard]] virtual bool contains(type_id id) const = 0;
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  [[nodiscard]] virtual vector<type_id> ids() const = 0;
};

template <typename Component>
class meta_component_storage final : public meta_component_storage_base {
public:
  static_assert(std::is_copy_constructible_v<Component>,
                "Meta components must be copy constructible");

  explicit meta_component_storage(memory::memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : &memory::library_resource()),
        components_(typename hash_map<std::uint64_t, memory::unique_ptr<Component>>::allocator_type{
            resource_}) {}

  template <typename... Args> [[nodiscard]] bool emplace(type_id id, Args&&... args) {
    std::lock_guard lock(mutex_);
    auto component = memory::make_unique<Component>(resource_, std::forward<Args>(args)...);
    return components_.try_emplace(id.value(), std::move(component)).second;
  }

  [[nodiscard]] std::optional<Component> find(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return std::nullopt;
    }

    return *position->second;
  }

  [[nodiscard]] const Component* find_ptr(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return nullptr;
    }

    return position->second.get();
  }

  [[nodiscard]] bool contains(type_id id) const override {
    std::lock_guard lock(mutex_);
    return components_.contains(id.value());
  }

  [[nodiscard]] std::size_t size() const noexcept override {
    std::lock_guard lock(mutex_);
    return components_.size();
  }

  [[nodiscard]] vector<type_id> ids() const override {
    std::lock_guard lock(mutex_);
    vector<type_id> result{allocator<type_id>{resource_}};
    result.reserve(components_.size());
    for (const auto& [id, component] : components_) {
      (void)component;
      result.emplace_back(id);
    }

    return result;
  }

private:
  mutable std::mutex mutex_;
  memory::memory_resource* resource_ = nullptr;
  hash_map<std::uint64_t, memory::unique_ptr<Component>> components_;
};

template <typename Component>
[[nodiscard]] memory::unique_ptr<meta_component_storage_base>
make_meta_component_storage(memory::memory_resource* resource = nullptr) {
  memory::memory_resource* resolved = resource != nullptr ? resource : &memory::library_resource();
  using storage_type = meta_component_storage<Component>;
  auto* storage = memory::make<storage_type>(resolved, resolved);
  memory::unique_ptr<meta_component_storage_base> result{storage};
  result.get_deleter().reset(resolved, [](void* user_data, void* value) noexcept {
    memory::destroy(static_cast<storage_type*>(value),
                    static_cast<memory::memory_resource*>(user_data));
  });
  return result;
}

} // namespace caors::meta::detail

#endif
