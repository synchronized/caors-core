// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_polymorphic_value_factory_polymorphic_value_factory_roundtrips_unique_pointer_test.cpp

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory roundtrips unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 42;
  player->name = "Una";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::to_string(holder);
  REQUIRE(json.succeeded());

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 42);
  CHECK(decoded_owner->name == "Una");
}

// 原测试文件：serialization_polymorphic_value_factory_polymorphic_value_factory_uses_memory_resource_for_unique_pointer_test.cpp

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory uses memory resource for unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 43;
  player->name = "Uma";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::to_string(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize(context, decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 43);
  CHECK(decoded_owner->name == "Uma");
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.owner.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：serialization_polymorphic_value_factory_polymorphic_value_factory_uses_memory_resource_test.cpp

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory uses memory resource", "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_holder holder{};
  auto shared = std::make_shared<polymorphic_factory_player>(1);
  shared->level = 41;
  shared->name = "Rin";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::to_string(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  polymorphic_factory_holder decoded{};
  const caors::error status = caors::deserialize(context, decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_shared = dynamic_cast<polymorphic_factory_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 41);
  CHECK(decoded_shared->name == "Rin");
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.shared.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
