// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - writes intermediate arrays and objects through text writer",
          "[serialization]") {
  caors::variant::array_type items;
  items.push_back(caors::variant::from(std::int64_t{1}));
  items.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"items"},
      1,
      caors::variant::array(std::move(items)),
  });

  caors::serialization::text_writer writer;
  const caors::error result =
      caors::serialization::write_variant(writer, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(writer.text() == R"({"items":[1,2]})");
}
