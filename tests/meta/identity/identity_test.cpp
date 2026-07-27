// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <string_view>

namespace {

struct identity_debug_type {};

struct identity_debug_capability {};

[[nodiscard]] bool
contains_identity(const caors::vector<caors::meta::identity_debug_record>& records,
                  caors::meta::identity_debug_domain domain, std::uint64_t id) {
  return std::any_of(records.begin(), records.end(), [&](const auto& record) {
    return record.domain == domain && record.id == id && !record.canonical_name.empty();
  });
}

} // namespace

TEST_CASE("meta identity - debug snapshot exposes allocated type and capability ids", "[meta]") {
  const caors::type_id type_id = caors::meta::id_of<identity_debug_type>();
  const caors::meta::capability_key capability_key =
      caors::meta::capability_key_of<identity_debug_capability>();

  REQUIRE(type_id);
  REQUIRE(capability_key);

  const std::string_view type_name = caors::meta::find_identity_debug_name(
      caors::meta::identity_debug_domain::type, type_id.value());
  REQUIRE_FALSE(type_name.empty());
  CHECK(caors::meta::find_identity_debug_id(caors::meta::identity_debug_domain::type, type_name) ==
        type_id.value());

  const std::string_view capability_name = caors::meta::find_identity_debug_name(
      caors::meta::identity_debug_domain::capability, capability_key.value());
  REQUIRE_FALSE(capability_name.empty());
  CHECK(caors::meta::find_identity_debug_id(caors::meta::identity_debug_domain::capability,
                                            capability_name) == capability_key.value());

  const auto type_records =
      caors::meta::identity_debug_snapshot(caors::meta::identity_debug_domain::type);
  CHECK(contains_identity(type_records, caors::meta::identity_debug_domain::type, type_id.value()));

  const auto all_records = caors::meta::identity_debug_snapshot();
  CHECK(contains_identity(all_records, caors::meta::identity_debug_domain::type, type_id.value()));
  CHECK(contains_identity(all_records, caors::meta::identity_debug_domain::capability,
                          capability_key.value()));
}

TEST_CASE("meta identity - debug lookup reports missing identity as empty", "[meta]") {
  CHECK(caors::meta::find_identity_debug_name(caors::meta::identity_debug_domain::type, 0).empty());
  CHECK(caors::meta::find_identity_debug_id(caors::meta::identity_debug_domain::capability,
                                            "caors.test.missing.capability") == 0);
}
