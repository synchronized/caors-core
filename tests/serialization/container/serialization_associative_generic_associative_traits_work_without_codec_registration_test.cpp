// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - generic associative traits work without codec registration",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_map_actor>(
                                   "caors.test.serialization.custom_codec_map_actor")
                                   .field("labels", &custom_codec_map_actor::labels)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<custom_codec_map_actor>()));

  custom_codec_map_actor actor{};
  actor.labels.values.emplace(6, caors::string{"six"});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[6,"six"]]})");

  caors::variant::dictionary_type entries;
  entries.push_back(caors::value::variant_entry{
      caors::variant::from(std::int64_t{10}),
      caors::variant::from(std::string_view{"ten"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"labels"},
      0,
      caors::variant::dictionary(std::move(entries)),
  });

  custom_codec_map_actor decoded{};
  const caors::error result =
      caors::deserialize(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.labels.values.size() == 1);
  CHECK(decoded.labels.values.at(10) == "ten");
}
