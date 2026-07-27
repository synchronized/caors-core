// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_identity.h"

#include "caors/meta/capability_traits.h"
#include "caors/meta/identity_id.h"

#include <exception>
#include <limits>
#include <list>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace caors::meta::detail {
namespace {

struct type_identity final {
  std::uint64_t id = 0;
  std::string canonical_name{};
};

class identity_table final {
public:
  [[nodiscard]] std::uint64_t get_or_create(std::string_view canonical_name) {
    return get_or_create_identity(canonical_name).id;
  }

  [[nodiscard]] vector<identity_debug_record> snapshot(identity_debug_domain domain) const {
    std::scoped_lock lock{mutex_};
    auto records = make_vector<identity_debug_record>(&memory::library_resource());
    records.reserve(identities_.size());
    for (const type_identity& identity : identities_) {
      auto canonical_name = make_string(&memory::library_resource());
      canonical_name.assign(identity.canonical_name.data(), identity.canonical_name.size());
      records.push_back(identity_debug_record{domain, identity.id, std::move(canonical_name)});
    }
    return records;
  }

  [[nodiscard]] std::string_view find_name(std::uint64_t id) const noexcept {
    std::scoped_lock lock{mutex_};
    const auto existing = identities_by_id_.find(id);
    return existing != identities_by_id_.end() ? existing->second->canonical_name
                                               : std::string_view{};
  }

  [[nodiscard]] std::uint64_t find_id(std::string_view canonical_name) const noexcept {
    std::scoped_lock lock{mutex_};
    const auto existing = identities_by_name_.find(canonical_name);
    return existing != identities_by_name_.end() ? existing->second->id : 0;
  }

private:
  [[nodiscard]] type_identity& get_or_create_identity(std::string_view canonical_name) {
    if (canonical_name.empty()) {
      std::terminate();
    }

    std::scoped_lock lock{mutex_};
    const auto existing = identities_by_name_.find(canonical_name);
    if (existing != identities_by_name_.end()) {
      return *existing->second;
    }

    if (next_id_ == std::numeric_limits<std::uint64_t>::max()) {
      std::terminate();
    }

    identities_.push_back(type_identity{next_id_++, std::string{canonical_name}});
    type_identity& identity = identities_.back();
    identities_by_name_.emplace(identity.canonical_name, &identity);
    identities_by_id_.emplace(identity.id, &identity);
    return identity;
  }

  mutable std::mutex mutex_;
  std::list<type_identity> identities_;
  std::unordered_map<std::string_view, type_identity*> identities_by_name_;
  std::unordered_map<std::uint64_t, type_identity*> identities_by_id_;
  std::uint64_t next_id_ = 1;
};

class identity_state final {
public:
  [[nodiscard]] std::uint64_t get_or_create(identity_domain domain,
                                            std::string_view canonical_name) {
    return table(domain).get_or_create(canonical_name);
  }

  [[nodiscard]] vector<identity_debug_record> snapshot(identity_debug_domain domain) const {
    return table(domain).snapshot(domain);
  }

  [[nodiscard]] vector<identity_debug_record> snapshot() const {
    auto records = make_vector<identity_debug_record>(&memory::library_resource());
    auto type_records = snapshot(identity_debug_domain::type);
    auto capability_records = snapshot(identity_debug_domain::capability);
    records.reserve(type_records.size() + capability_records.size());
    for (auto& record : type_records) {
      records.push_back(std::move(record));
    }
    for (auto& record : capability_records) {
      records.push_back(std::move(record));
    }
    return records;
  }

  [[nodiscard]] std::string_view find_name(identity_debug_domain domain,
                                           std::uint64_t id) const noexcept {
    return table(domain).find_name(id);
  }

  [[nodiscard]] std::uint64_t find_id(identity_debug_domain domain,
                                      std::string_view canonical_name) const noexcept {
    return table(domain).find_id(canonical_name);
  }

private:
  [[nodiscard]] identity_table& table(identity_domain domain) noexcept {
    switch (domain) {
    case identity_domain::type:
      return type_;
    case identity_domain::capability:
      return capability_;
    default:
      return type_;
    }
  }

  [[nodiscard]] const identity_table& table(identity_debug_domain domain) const noexcept {
    switch (domain) {
    case identity_debug_domain::type:
      return type_;
    case identity_debug_domain::capability:
      return capability_;
    default:
      return type_;
    }
  }

  identity_table type_{};
  identity_table capability_{};
};

[[nodiscard]] identity_state& global_identity_state() {
  static identity_state state;
  return state;
}

} // namespace

std::uint64_t get_or_create_identity_id(identity_domain domain, const char* canonical_name,
                                        std::size_t length) noexcept {
  if (canonical_name == nullptr) {
    std::terminate();
  }
  return global_identity_state().get_or_create(domain, std::string_view{canonical_name, length});
}

type_id get_or_create_type_id(const char* canonical_name, std::size_t length) noexcept {
  return type_id{get_or_create_identity_id(identity_domain::type, canonical_name, length)};
}

capability_id get_or_create_capability_id(const char* canonical_name, std::size_t length) noexcept {
  return capability_id{
      get_or_create_identity_id(identity_domain::capability, canonical_name, length)};
}

vector<identity_debug_record> identity_debug_snapshot_impl(identity_debug_domain domain) {
  return global_identity_state().snapshot(domain);
}

vector<identity_debug_record> identity_debug_snapshot_impl() {
  return global_identity_state().snapshot();
}

std::string_view find_identity_debug_name_impl(identity_debug_domain domain,
                                               std::uint64_t id) noexcept {
  return global_identity_state().find_name(domain, id);
}

std::uint64_t find_identity_debug_id_impl(identity_debug_domain domain,
                                          std::string_view canonical_name) noexcept {
  return global_identity_state().find_id(domain, canonical_name);
}

} // namespace caors::meta::detail

namespace caors::meta {

vector<identity_debug_record> identity_debug_snapshot(identity_debug_domain domain) {
  return detail::identity_debug_snapshot_impl(domain);
}

vector<identity_debug_record> identity_debug_snapshot() {
  return detail::identity_debug_snapshot_impl();
}

std::string_view find_identity_debug_name(identity_debug_domain domain, std::uint64_t id) noexcept {
  return detail::find_identity_debug_name_impl(domain, id);
}

std::uint64_t find_identity_debug_id(identity_debug_domain domain,
                                     std::string_view canonical_name) noexcept {
  return detail::find_identity_debug_id_impl(domain, canonical_name);
}

} // namespace caors::meta
