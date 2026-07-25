// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_identity.h"

#include <exception>
#include <limits>
#include <list>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace caors::meta::detail {
namespace {

class type_identity_state final {
public:
  [[nodiscard]] const type_identity& get_or_create(std::string_view canonical_name) {
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

    identities_.push_back(type_identity{type_id{next_id_++}, std::string{canonical_name}});
    type_identity& identity = identities_.back();
    identities_by_name_.emplace(identity.canonical_name, &identity);
    return identity;
  }

private:
  std::mutex mutex_;
  std::list<type_identity> identities_;
  std::unordered_map<std::string_view, type_identity*> identities_by_name_;
  std::uint64_t next_id_ = 1;
};

[[nodiscard]] type_identity_state& identity_state() {
  static type_identity_state state;
  return state;
}

} // namespace

const type_identity& get_or_create_type_identity(std::string_view canonical_name) noexcept {
  return identity_state().get_or_create(canonical_name);
}

} // namespace caors::meta::detail
