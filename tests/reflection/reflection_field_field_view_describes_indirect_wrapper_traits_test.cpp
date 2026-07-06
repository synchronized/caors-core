// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes indirect wrapper traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field raw = require_field(reflected, "raw");
  CHECK(raw.kind() == caors::meta::type_kind::raw_pointer);
  CHECK(raw.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(raw.value_type() == registered_type_id("int"));
  CHECK(raw.indirect());
  CHECK(raw.nullable());

  const caors::reflection::field unique = require_field(reflected, "unique");
  CHECK(unique.kind() == caors::meta::type_kind::wrapper);
  CHECK(unique.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(unique.value_type() == registered_type_id("int"));
  CHECK(unique.nullable());

  const caors::reflection::field shared = require_field(reflected, "shared");
  CHECK(shared.kind() == caors::meta::type_kind::wrapper);
  CHECK(shared.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(shared.value_type() == registered_type_id("int"));
  CHECK(shared.nullable());

  const caors::reflection::field weak = require_field(reflected, "weak");
  CHECK(weak.kind() == caors::meta::type_kind::wrapper);
  CHECK(weak.wrapper_semantics() == caors::meta::wrapper_semantics::unsupported);
  CHECK(weak.value_type() == registered_type_id("int"));
  CHECK(weak.nullable());

  const caors::reflection::field optional = require_field(reflected, "optional");
  CHECK(optional.kind() == caors::meta::type_kind::wrapper);
  CHECK(optional.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(optional.value_type() == registered_type_id("int"));
  CHECK(optional.nullable());

  const caors::reflection::field wrapper = require_field(reflected, "wrapper");
  CHECK(wrapper.kind() == caors::meta::type_kind::wrapper);
  CHECK(wrapper.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(wrapper.value_type() == registered_type_id("int"));
  CHECK_FALSE(wrapper.nullable());

  const caors::reflection::field custom_inline = require_field(reflected, "custom_inline");
  CHECK(custom_inline.kind() == caors::meta::type_kind::wrapper);
  CHECK(custom_inline.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(custom_inline.value_type() == registered_type_id("int"));
  CHECK_FALSE(custom_inline.nullable());
}
