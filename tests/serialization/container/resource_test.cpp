// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_container_resource_test.cpp

#include "caors/memory.h"
#include "serialization_test_support.h"

using namespace caors::tests::serialization;

namespace {

struct resource_propagation_holder {
  using allocator_type = caors::allocator<resource_propagation_holder>;

  resource_propagation_holder() = default;

  explicit resource_propagation_holder(const allocator_type& allocator)
      : names(caors::allocator<caors::string>{allocator.resource()}),
        groups(caors::allocator<caors::vector<int>>{allocator.resource()}),
        labels(typename caors::hash_map<caors::string, caors::string>::allocator_type{
            allocator.resource()}) {}

  caors::vector<caors::string> names{};
  caors::vector<caors::vector<int>> groups{};
  caors::hash_map<caors::string, caors::string> labels{};
  std::optional<caors::string> maybe{};
  std::variant<int, caors::string, caors::vector<int>> choice{0};
};

void require_resource_propagation_holder_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<resource_propagation_holder>(
                                     "caors.test.serialization.resource_propagation_holder")
                                     .meta()
                                     .property("names", &resource_propagation_holder::names)
                                     .property("groups", &resource_propagation_holder::groups)
                                     .property("labels", &resource_propagation_holder::labels)
                                     .property("maybe", &resource_propagation_holder::maybe)
                                     .property("choice", &resource_propagation_holder::choice)
                                     .serialization()
                                     .commit()));
}

} // namespace

TEST_CASE("serialization - propagates context resource through allocator aware values",
          "[serialization]") {
  require_resource_propagation_holder_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  struct holder_deleter {
    caors::memory_resource* resource = nullptr;

    void operator()(resource_propagation_holder* value) const noexcept {
      caors::destroy(value, resource);
    }
  };

  std::unique_ptr<resource_propagation_holder, holder_deleter> decoded{
      caors::make<resource_propagation_holder>(&tracking),
      holder_deleter{&tracking},
  };
  REQUIRE(decoded != nullptr);

  const caors::error result = caors::deserialize(
      context, *decoded,
      R"({"names":["Ada","Grace"],"groups":[[1,2],[3]],"labels":[["role","engineer"]],"maybe":"present","choice":[1,"variant"]})");

  REQUIRE(result.succeeded());

  CHECK(decoded->names.get_allocator().resource() == &tracking);
  REQUIRE(decoded->names.size() == 2);
  CHECK(decoded->names[0] == "Ada");
  CHECK(decoded->names[0].get_allocator().resource() == &tracking);
  CHECK(decoded->names[1].get_allocator().resource() == &tracking);

  CHECK(decoded->groups.get_allocator().resource() == &tracking);
  REQUIRE(decoded->groups.size() == 2);
  REQUIRE(decoded->groups[0].size() == 2);
  CHECK(decoded->groups[0].get_allocator().resource() == &tracking);
  CHECK(decoded->groups[0][0] == 1);
  CHECK(decoded->groups[0][1] == 2);
  CHECK(decoded->groups[1].get_allocator().resource() == &tracking);
  CHECK(decoded->groups[1][0] == 3);

  CHECK(decoded->labels.get_allocator().resource() == &tracking);
  {
    caors::string role_key{caors::allocator<char>{&tracking}};
    role_key = "role";
    const auto label = decoded->labels.find(role_key);
    REQUIRE(label != decoded->labels.end());
    CHECK(label->first.get_allocator().resource() == &tracking);
    CHECK(label->second == "engineer");
    CHECK(label->second.get_allocator().resource() == &tracking);
  }

  REQUIRE(decoded->maybe.has_value());
  CHECK(*decoded->maybe == "present");
  CHECK(decoded->maybe->get_allocator().resource() == &tracking);

  REQUIRE(std::holds_alternative<caors::string>(decoded->choice));
  CHECK(std::get<caors::string>(decoded->choice) == "variant");
  CHECK(std::get<caors::string>(decoded->choice).get_allocator().resource() == &tracking);

  decoded.reset();
  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}
