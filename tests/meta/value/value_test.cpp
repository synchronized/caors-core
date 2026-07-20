// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_value_test_common.h"

#include "caors/meta/object.h"

using namespace caors::tests::meta_value;

namespace {

class runtime_value_object : public caors::meta::object_base<runtime_value_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.meta.runtime_value_object";
};

} // namespace

TEST_CASE("meta value - stores scalar and text fast path values", "[meta]") {
  caors::meta::value empty;
  CHECK(empty.is_null());

  caors::meta::value boolean = true;
  CHECK(boolean.kind() == caors::meta::value_kind::boolean);
  CHECK(boolean.as_bool());

  caors::meta::value signed_integer = -42;
  CHECK(signed_integer.is_i64());
  CHECK(signed_integer.as_i64() == -42);

  caors::meta::value unsigned_integer = 42U;
  CHECK(unsigned_integer.is_u64());
  CHECK(unsigned_integer.as_u64() == 42U);

  caors::meta::value floating = 3.5F;
  CHECK(floating.is_f64());
  CHECK(floating.as_f64() == 3.5);

  const caors::string_name name{"caors.test.meta.value.name"};
  caors::meta::value named = name;
  CHECK(named.is_string_name());
  REQUIRE(named.try_as_string_view().has_value());
  CHECK(*named.try_as_string_view() == name.view());

  caors::meta::value text = "runtime text";
  CHECK(text.is_string());
  REQUIRE(text.try_as_string_view().has_value());
  CHECK(*text.try_as_string_view() == "runtime text");
}

TEST_CASE("meta value - stores object pointer separately from C++ any", "[meta]") {
  runtime_value_object object;

  caors::meta::value stored = caors::meta::value::from_object(&object);
  CHECK(stored.kind() == caors::meta::value_kind::object);
  CHECK(stored.as_object() == &object);

  const caors::meta::any_ref ref = stored.to_ref();
  REQUIRE(ref);
  CHECK(ref.try_get<caors::meta::object>() != nullptr);
}

TEST_CASE("meta value - can wrap any_ref and any fallback values", "[meta]") {
  require_value_test_meta();

  int number = 7;
  caors::meta::value ref_value = caors::meta::value::from_ref(number);
  CHECK(ref_value.kind() == caors::meta::value_kind::reference);
  REQUIRE(ref_value.as_ref().try_get<int>() != nullptr);
  CHECK(*ref_value.as_ref().try_get<int>() == 7);
  CHECK(ref_value.as_ref().try_set(9));
  CHECK(number == 9);

  caors::meta::value any_value = caors::meta::value::from_any(caors::any::from(number));
  CHECK(any_value.kind() == caors::meta::value_kind::any);
  caors::any copied = any_value.to_any();
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 9);
}
