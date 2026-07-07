// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include <algorithm>

using namespace caors::tests::resources;

namespace {

constexpr caors::resource_guid test_guid_c{caors::guid::from_bytes(
    caors::guid::byte_array{0x9d, 0xe1, 0x08, 0x75, 0xa1, 0x63, 0x4b, 0x8d, 0x9c, 0x28, 0x5b, 0x28,
                            0x57, 0x49, 0x86, 0x92})};

constexpr caors::resource_guid test_guid_d{caors::guid::from_bytes(
    caors::guid::byte_array{0x14, 0x58, 0x5f, 0x36, 0x89, 0x9b, 0x4c, 0x34, 0xa9, 0x9d, 0xd8, 0x4e,
                            0x6a, 0x6f, 0xa3, 0x4c})};

constexpr caors::resource_guid test_guid_e{caors::guid::from_bytes(
    caors::guid::byte_array{0xa1, 0x83, 0x0b, 0xf7, 0x2c, 0x2f, 0x43, 0xb2, 0xa0, 0x43, 0x93, 0x7d,
                            0xf6, 0x9d, 0x83, 0x27})};

[[nodiscard]] bool contains_guid(const caors::vector<caors::resource_guid>& values,
                                 caors::resource_guid guid) {
  return std::find(values.begin(), values.end(), guid) != values.end();
}

} // namespace

TEST_CASE("resources - dependency graph tracks resource dependencies", "[resources]") {
  caors::resource_dependency_graph graph;

  CHECK(graph.empty());
  CHECK(graph.add(test_guid_a, test_guid_b));
  CHECK(graph.add(test_guid_a, test_guid_c));
  CHECK(graph.add(test_guid_a, test_guid_b));

  CHECK(graph.edge_count() == 2);
  CHECK(graph.contains(test_guid_a, test_guid_b));
  CHECK(graph.contains(test_guid_a, test_guid_c));

  caors::vector<caors::resource_guid> dependencies = graph.dependencies(test_guid_a);
  CHECK(dependencies.size() == 2);
  CHECK(contains_guid(dependencies, test_guid_b));
  CHECK(contains_guid(dependencies, test_guid_c));

  caors::vector<caors::resource_guid> dependents = graph.dependents(test_guid_b);
  REQUIRE(dependents.size() == 1);
  CHECK(dependents[0] == test_guid_a);

  caors::vector<caors::resource_guid> affected = graph.affected_by(test_guid_b);
  REQUIRE(affected.size() == 1);
  CHECK(affected[0] == test_guid_a);
}

TEST_CASE("resources - dependency graph removes forward and reverse edges", "[resources]") {
  caors::resource_dependency_graph graph;

  REQUIRE(graph.add(test_guid_a, test_guid_b));
  REQUIRE(graph.add(test_guid_c, test_guid_b));

  CHECK(graph.remove(test_guid_a, test_guid_b));
  CHECK_FALSE(graph.contains(test_guid_a, test_guid_b));
  CHECK(graph.contains(test_guid_c, test_guid_b));

  caors::vector<caors::resource_guid> dependents = graph.dependents(test_guid_b);
  REQUIRE(dependents.size() == 1);
  CHECK(dependents[0] == test_guid_c);

  graph.clear_resource(test_guid_b);
  CHECK(graph.edge_count() == 0);
  CHECK(graph.dependencies(test_guid_c).empty());
  CHECK(graph.dependents(test_guid_b).empty());
}

TEST_CASE("resources - dependency graph rejects invalid edges", "[resources]") {
  caors::resource_dependency_graph graph;

  CHECK_FALSE(graph.add(caors::resource_guid::nil(), test_guid_a));
  CHECK_FALSE(graph.add(test_guid_a, caors::resource_guid::nil()));
  CHECK_FALSE(graph.add(test_guid_a, test_guid_a));
  CHECK(graph.empty());
}

TEST_CASE("resources - dependency graph returns transitive affected resources", "[resources]") {
  caors::resource_dependency_graph graph;

  REQUIRE(graph.add(test_guid_b, test_guid_a));
  REQUIRE(graph.add(test_guid_c, test_guid_b));
  REQUIRE(graph.add(test_guid_d, test_guid_c));
  REQUIRE(graph.add(test_guid_e, test_guid_b));

  caors::vector<caors::resource_guid> direct = graph.affected_by(test_guid_a);
  REQUIRE(direct.size() == 1);
  CHECK(direct[0] == test_guid_b);

  caors::vector<caors::resource_guid> affected = graph.affected_closure(test_guid_a);
  CHECK(affected.size() == 4);
  CHECK(contains_guid(affected, test_guid_b));
  CHECK(contains_guid(affected, test_guid_c));
  CHECK(contains_guid(affected, test_guid_d));
  CHECK(contains_guid(affected, test_guid_e));
  CHECK_FALSE(contains_guid(affected, test_guid_a));
}

TEST_CASE("resources - dependency graph de-duplicates diamond affected resources", "[resources]") {
  caors::resource_dependency_graph graph;

  REQUIRE(graph.add(test_guid_b, test_guid_a));
  REQUIRE(graph.add(test_guid_c, test_guid_a));
  REQUIRE(graph.add(test_guid_d, test_guid_b));
  REQUIRE(graph.add(test_guid_d, test_guid_c));

  caors::vector<caors::resource_guid> affected = graph.affected_closure(test_guid_a);
  CHECK(affected.size() == 3);
  CHECK(contains_guid(affected, test_guid_b));
  CHECK(contains_guid(affected, test_guid_c));
  CHECK(contains_guid(affected, test_guid_d));
}

TEST_CASE("resources - dependency graph handles cycles in affected closure", "[resources]") {
  caors::resource_dependency_graph graph;

  REQUIRE(graph.add(test_guid_b, test_guid_a));
  REQUIRE(graph.add(test_guid_c, test_guid_b));
  REQUIRE(graph.add(test_guid_a, test_guid_c));

  caors::vector<caors::resource_guid> affected = graph.affected_closure(test_guid_a);
  CHECK(affected.size() == 2);
  CHECK(contains_guid(affected, test_guid_b));
  CHECK(contains_guid(affected, test_guid_c));
  CHECK_FALSE(contains_guid(affected, test_guid_a));
}

TEST_CASE("resources - dependency graph updates affected closure after clear", "[resources]") {
  caors::resource_dependency_graph graph;

  REQUIRE(graph.add(test_guid_b, test_guid_a));
  REQUIRE(graph.add(test_guid_c, test_guid_b));

  caors::vector<caors::resource_guid> affected = graph.affected_closure(test_guid_a);
  CHECK(affected.size() == 2);

  graph.clear_dependencies(test_guid_b);

  affected = graph.affected_closure(test_guid_a);
  CHECK(affected.empty());
}
