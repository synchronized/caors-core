// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ptr_resolver_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_resolver;
TEST_CASE("serialization - object pointer resolver failures are reported", "[serialization]") {
  require_object_ptr_actor_metadata();

  object_ptr_target target;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{&target}};
  const caors::serialization::serialization_text_result missing_resolver =
      caors::serialize_text(actor);
  CHECK(missing_resolver.failed());
  CHECK(missing_resolver.status.value() == caors::errc::invalid_argument);

  object_ptr_resolver_data resolver_data{caors::object_id{99}, nullptr};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{99}),
  });

  object_ptr_actor decoded{};
  const caors::error missing_object =
      caors::deserialize(decoded, caors::variant::object(std::move(members)), context);
  CHECK(missing_object.failed());
  CHECK(missing_object.value() == caors::errc::not_found);

  other_object_ptr_target other;
  object_ptr_resolver_data wrong_type_data{caors::object_id{100}, &other};
  const caors::serialization::reference_resolver wrong_type_resolver{
      &resolve_object_id,
      &resolve_object,
      &wrong_type_data,
  };
  caors::serialization::context wrong_type_context{};
  wrong_type_context.references.object_resolver = &wrong_type_resolver;

  caors::variant::object_type wrong_type_members;
  wrong_type_members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{100}),
  });

  const caors::error wrong_type = caors::deserialize(
      decoded, caors::variant::object(std::move(wrong_type_members)), wrong_type_context);
  CHECK(wrong_type.failed());
  CHECK(wrong_type.value() == caors::errc::invalid_type);
}
