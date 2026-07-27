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
    return identity;
  }

  std::mutex mutex_;
  std::list<type_identity> identities_;
  std::unordered_map<std::string_view, type_identity*> identities_by_name_;
  std::uint64_t next_id_ = 1;
};

class identity_state final {
public:
  [[nodiscard]] std::uint64_t get_or_create(identity_domain domain,
                                            std::string_view canonical_name) {
    return table(domain).get_or_create(canonical_name);
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

} // namespace caors::meta::detail
