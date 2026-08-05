// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "registry_state.h"

#include "caors/meta/detail/type_access.h"

#include <utility>

namespace caors::meta::detail {
namespace {

std::atomic<std::uint64_t> next_layer_generation{1};

} // namespace

registry_layer_state::registry_layer_state(memory::memory_resource* resource,
                                           registry_layer_lifetime lifetime)
    : resource_(resource != nullptr ? resource : &memory::process_resource()),
      state_(memory::make<registry_state>(resource_, resource_)), lifetime_(lifetime),
      generation_(next_layer_generation.fetch_add(1, std::memory_order_relaxed)) {}

registry_layer_state::~registry_layer_state() {
  // record 和 wrapper 的析构可能执行插件代码，必须在释放插件 lifetime lease 之前完成。
  memory::destroy(state_, resource_);
  state_ = nullptr;
  lifetime_owner_.reset();
}

bool registry_layer_state::try_enter_call() noexcept {
  if (!active()) {
    return false;
  }
  if (lifetime_ == registry_layer_lifetime::pinned_process) {
    return true;
  }

  in_flight_calls_.fetch_add(1, std::memory_order_acq_rel);
  if (active()) {
    return true;
  }

  in_flight_calls_.fetch_sub(1, std::memory_order_acq_rel);
  return false;
}

void registry_layer_state::leave_call() noexcept {
  if (lifetime_ == registry_layer_lifetime::pinned_process) {
    return;
  }
  in_flight_calls_.fetch_sub(1, std::memory_order_acq_rel);
}

std::size_t registry_layer_state::in_flight_calls() const noexcept {
  return in_flight_calls_.load(std::memory_order_acquire);
}

error registry_layer_state::attach_lifetime(std::shared_ptr<void> owner) {
  if (owner == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  std::lock_guard lock{state_->mutex};
  if (frozen_ || state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }
  if (lifetime_owner_ != nullptr) {
    return {error_domain::meta, errc::already_exists};
  }

  lifetime_owner_ = std::move(owner);
  return {};
}

layer_call_guard::layer_call_guard(std::shared_ptr<registry_layer_state> layer) noexcept
    : layer_(std::move(layer)), entered_(layer_ == nullptr || layer_->try_enter_call()) {}

layer_call_guard::layer_call_guard(const meta::type& type) noexcept
    : layer_call_guard(type_access::layer_of(type)) {}

layer_call_guard::~layer_call_guard() {
  if (entered_ && layer_ != nullptr) {
    layer_->leave_call();
  }
}

registry_snapshot::registry_snapshot(std::shared_ptr<registry_layer_state> primary)
    : layers_(core::vector<std::shared_ptr<registry_layer_state>>::allocator_type{
          primary != nullptr ? primary->state().resource : &memory::process_resource()}) {
  layers_.push_back(std::move(primary));
}

registry_snapshot::registry_snapshot(core::vector<std::shared_ptr<registry_layer_state>> layers)
    : layers_(std::move(layers)) {}

type_record* find_type_record(registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

const type_record* find_type_record(const registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

namespace {

[[nodiscard]] bool snapshot_contains(const registry_snapshot* snapshot, type_id id) {
  if (snapshot == nullptr) {
    return false;
  }
  for (const auto& layer : snapshot->layers()) {
    if (layer->state().types.contains(id.value())) {
      return true;
    }
  }
  return false;
}

} // namespace

error validate_registry(const registry_state& state, const registry_snapshot* snapshot) {
  for (const auto& [raw_id, stored] : state.types) {
    if (stored == nullptr || !stored->registered() || stored->id().value() != raw_id) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (stored->parent_id() && !state.types.contains(stored->parent_id().value()) &&
        !snapshot_contains(snapshot, stored->parent_id())) {
      return {error_domain::meta, errc::missing_parent};
    }

    if (stored->name()) {
      const auto name_position = state.names.find(stored->name());
      if (name_position == state.names.end() || name_position->second != stored.get()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }

    if (stored->guid()) {
      const auto guid_position = state.guids.find(stored->guid().value);
      if (guid_position == state.guids.end() || guid_position->second != stored.get()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }

    const parent_cast_ops* cast = stored->parent_cast();
    if (cast != nullptr && (!cast->valid() || cast->parent != stored->parent_id() ||
                            (!state.types.contains(cast->parent.value()) &&
                             !snapshot_contains(snapshot, cast->parent)))) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored->members_valid()) {
      return {error_domain::meta, errc::invalid_type};
    }

    for (std::size_t index = 0; index < stored->nested_type_count(); ++index) {
      const nested_type_data* nested_type = stored->nested_type_at(index);
      if (nested_type == nullptr || !nested_type->valid()) {
        return {error_domain::meta, errc::invalid_type};
      }
      if (!state.types.contains(nested_type->nested_type.value()) &&
          !snapshot_contains(snapshot, nested_type->nested_type)) {
        return {error_domain::meta, errc::missing_type};
      }
    }
  }

  for (const auto& [index, stored] : state.cpp_types) {
    (void)index;
    if (stored == nullptr || find_type_record(state, stored->id()) != stored) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [name, stored] : state.names) {
    if (stored == nullptr || stored->name() != name) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [guid_value, stored] : state.guids) {
    if (stored == nullptr || stored->guid().value != guid_value) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  return {};
}

} // namespace caors::meta::detail
