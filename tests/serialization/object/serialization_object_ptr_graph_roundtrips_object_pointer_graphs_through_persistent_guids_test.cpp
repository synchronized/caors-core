// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ptr_graph_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_graph;

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
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;

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
