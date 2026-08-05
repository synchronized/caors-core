// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_STATE_H_
#define CAORS_META_DETAIL_REGISTRY_STATE_H_

#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/detail/call_guard.h"
#include "caors/meta/detail/type_record.h"
#include "caors/meta/registration.h"
#include "caors/meta/registry.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <typeindex>
#include <utility>

namespace caors::meta::detail {

class registry_state {
public:
  explicit registry_state(memory::memory_resource* internal_resource = nullptr)
      : resource(internal_resource != nullptr ? internal_resource : &memory::process_resource()),
        registrations(core::vector<registration_fn>::allocator_type{resource}),
        types(core::hash_map<std::uint64_t, memory::unique_ptr<type_record>>::allocator_type{
            resource}),
        cpp_types(core::hash_map<std::type_index, type_record*>::allocator_type{resource}),
        names(core::hash_map<core::string_name, type_record*>::allocator_type{resource}),
        guids(core::hash_map<core::guid, type_record*, core::guid_hash>::allocator_type{resource}) {
  }

  mutable std::mutex mutex;
  memory::memory_resource* resource = nullptr;
  bool frozen = false;
  bool registrations_ran = false;
  core::vector<registration_fn> registrations;
  core::hash_map<std::uint64_t, memory::unique_ptr<type_record>> types;
  core::hash_map<std::type_index, type_record*> cpp_types;
  core::hash_map<core::string_name, type_record*> names;
  core::hash_map<core::guid, type_record*, core::guid_hash> guids;
};

/** @brief 单个 registry layer 的稳定状态所有者。 */
class registry_layer_state final {
public:
  explicit registry_layer_state(
      memory::memory_resource* resource,
      registry_layer_lifetime lifetime = registry_layer_lifetime::detachable);
  ~registry_layer_state();

  registry_layer_state(const registry_layer_state&) = delete;
  registry_layer_state& operator=(const registry_layer_state&) = delete;

  [[nodiscard]] registry_state& state() noexcept { return *state_; }
  [[nodiscard]] const registry_state& state() const noexcept { return *state_; }
  [[nodiscard]] registry_layer_lifetime lifetime() const noexcept { return lifetime_; }
  [[nodiscard]] bool frozen() const noexcept { return frozen_; }
  [[nodiscard]] bool active() const noexcept { return active_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  void freeze() noexcept { frozen_ = true; }
  void deactivate() noexcept { active_.store(false, std::memory_order_release); }
  [[nodiscard]] bool try_enter_call() noexcept;
  void leave_call() noexcept;
  [[nodiscard]] std::size_t in_flight_calls() const noexcept;
  [[nodiscard]] error attach_lifetime(std::shared_ptr<void> owner);

private:
  memory::memory_resource* resource_ = nullptr;
  registry_state* state_ = nullptr;
  registry_layer_lifetime lifetime_ = registry_layer_lifetime::detachable;
  std::atomic<bool> active_{true};
  std::atomic<std::size_t> in_flight_calls_{0};
  /** @brief 保持注册代码所属模块存活；必须晚于 state_ 所有 record 和 wrapper 释放。 */
  std::shared_ptr<void> lifetime_owner_{};
  std::uint64_t generation_ = 0;
  bool frozen_ = false;
};

/** @brief 一次 registry 查询固定持有的有序 layer 集合。 */
class registry_snapshot final {
public:
  explicit registry_snapshot(std::shared_ptr<registry_layer_state> primary);
  explicit registry_snapshot(core::vector<std::shared_ptr<registry_layer_state>> layers);

  [[nodiscard]] std::span<const std::shared_ptr<registry_layer_state>> layers() const noexcept {
    return layers_;
  }
  [[nodiscard]] const std::shared_ptr<registry_layer_state>& primary() const noexcept {
    return layers_.front();
  }

private:
  core::vector<std::shared_ptr<registry_layer_state>> layers_;
};

[[nodiscard]] type_record* find_type_record(registry_state& state, type_id id);
[[nodiscard]] const type_record* find_type_record(const registry_state& state, type_id id);
[[nodiscard]] error validate_registry(const registry_state& state,
                                      const registry_snapshot* snapshot = nullptr);

template <typename Callback>
decltype(auto) read_layer_state(const registry_layer_state& layer, Callback&& callback) {
  const registry_state& state = layer.state();
  if (layer.frozen() || state.frozen) {
    return std::invoke(std::forward<Callback>(callback), state);
  }

  std::lock_guard lock{state.mutex};
  return std::invoke(std::forward<Callback>(callback), state);
}

} // namespace caors::meta::detail

#endif
