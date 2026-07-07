// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include <algorithm>

using namespace caors::tests::resources;

namespace {

constexpr caors::resource_guid test_guid_c{caors::guid::from_bytes(
    caors::guid::byte_array{0x9d, 0xe1, 0x08, 0x75, 0xa1, 0x63, 0x4b, 0x8d, 0x9c, 0x28, 0x5b, 0x28,
                            0x57, 0x49, 0x86, 0x92})};

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
