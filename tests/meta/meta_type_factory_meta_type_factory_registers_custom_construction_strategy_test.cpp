// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers custom construction strategy", "[meta]") {
  auto builder = caors::meta::type_<custom_factory_type>("caors.test.type.factory_custom")
                     .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(builder.succeeded());

  const caors::meta::detail::value_factory_ops factory =
      caors::meta::get_registry().find_type_factory(builder.get().id());
  REQUIRE(factory.valid());

  auto* value = static_cast<custom_factory_type*>(factory.construct(nullptr));
  REQUIRE(value != nullptr);
  CHECK(value->value == 23);
  factory.destroy(nullptr, value);

  auto duplicate = caors::meta::type_<custom_factory_type>("caors.test.type.factory_custom")
                       .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(duplicate.result() == meta_error(caors::errc::duplicate_id));
  CHECK(duplicate.succeeded());
}
