// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string>
#include <string_view>

using namespace caors::tests::serialization;

namespace {

struct object_ptr_graph_actor {
  caors::object_ptr<object_ptr_target> primary{};
  caors::object_ptr<object_ptr_target> secondary{};
  caors::vector<caors::object_ptr<object_ptr_target>> related{};
};

void require_object_ptr_target_class_metadata() {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
}

void require_object_ptr_graph_metadata() {
  require_registered_type<caors::object_ptr<object_ptr_target>>(
      "caors.object_ptr<caors.test.serialization.object_ptr_target>");
  require_registered_type<caors::vector<caors::object_ptr<object_ptr_target>>>(
      "caors.vector<caors.object_ptr<caors.test.serialization.object_ptr_target>>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ptr_graph_actor>(
                                   "caors.test.serialization.object_ptr_graph_actor")
                                   .field("primary", &object_ptr_graph_actor::primary)
                                   .field("secondary", &object_ptr_graph_actor::secondary)
                                   .field("related", &object_ptr_graph_actor::related)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_graph_actor>()));
}

} // namespace

TEST_CASE("serialization - roundtrips object pointer graphs through runtime ids",
          "[serialization]") {
  require_object_ptr_target_class_metadata();
  require_object_ptr_graph_metadata();

  caors::objects::manager manager;
  object_ptr_target* primary = manager.create<object_ptr_target>();
  object_ptr_target* secondary = manager.create<object_ptr_target>();
  REQUIRE(primary != nullptr);
  REQUIRE(secondary != nullptr);

  const caors::object_id primary_id = primary->id();
  const caors::object_id secondary_id = secondary->id();
  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};

  object_ptr_graph_actor graph{};
  graph.primary.reset(primary);
  graph.secondary.reset(secondary);
  graph.related.push_back(caors::object_ptr<object_ptr_target>{primary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{secondary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{primary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{});

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(graph, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"primary\":" + std::to_string(primary_id.value()) +
                                    ",\"secondary\":" + std::to_string(secondary_id.value()) +
                                    ",\"related\":[" + std::to_string(primary_id.value()) + "," +
                                    std::to_string(secondary_id.value()) + "," +
                                    std::to_string(primary_id.value()) + ",0]}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_graph_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.primary.get() == primary);
  CHECK(decoded.secondary.get() == secondary);
  REQUIRE(decoded.related.size() == 4);
  CHECK(decoded.related[0].get() == primary);
  CHECK(decoded.related[1].get() == secondary);
  CHECK(decoded.related[2].get() == decoded.primary.get());
  CHECK(decoded.related[3].get() == nullptr);

  REQUIRE(manager.destroy(secondary_id));

  object_ptr_graph_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - roundtrips object pointer graphs through persistent guids",
          "[serialization]") {
  require_object_ptr_target_class_metadata();
  require_object_ptr_graph_metadata();

  const caors::object_guid primary_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x1c, 0x04, 0x1d, 0x36, 0x9e, 0x5e, 0x45, 0xe8, 0x93, 0x5c, 0x27,
                              0x28, 0x81, 0x41, 0x59, 0x44})};
  const caors::object_guid secondary_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x94, 0xa3, 0xf6, 0x77, 0xa9, 0x83, 0x4b, 0x1d, 0x81, 0x1f, 0x76,
                              0x8f, 0x5f, 0x73, 0x58, 0x0e})};
  const std::string primary_guid_text = primary_guid.string();
  const std::string secondary_guid_text = secondary_guid.string();

  caors::objects::object_index index;
  caors::objects::manager manager(index);
  object_ptr_target* primary = manager.create_with_guid<object_ptr_target>(primary_guid);
  object_ptr_target* secondary = manager.create_with_guid<object_ptr_target>(secondary_guid);
  REQUIRE(primary != nullptr);
  REQUIRE(secondary != nullptr);

  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_persistent_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};

  object_ptr_graph_actor graph{};
  graph.primary.reset(primary);
  graph.secondary.reset(secondary);
  graph.related.push_back(caors::object_ptr<object_ptr_target>{secondary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{primary});

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(graph, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"primary\":\"" + primary_guid_text + "\",\"secondary\":\"" +
                                    secondary_guid_text + "\",\"related\":[\"" +
                                    secondary_guid_text + "\",null,\"" + primary_guid_text + "\"]}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_graph_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.primary.get() == primary);
  CHECK(decoded.secondary.get() == secondary);
  REQUIRE(decoded.related.size() == 3);
  CHECK(decoded.related[0].get() == secondary);
  CHECK(decoded.related[1].get() == nullptr);
  CHECK(decoded.related[2].get() == decoded.primary.get());

  REQUIRE(manager.destroy(secondary->id()));

  object_ptr_graph_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}
