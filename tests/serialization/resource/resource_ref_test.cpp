// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_resource_ref_serialization_collects_resource_dependencies_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;

TEST_CASE("serialization - resource refs are collected as dependencies", "[serialization]") {
  require_resource_ref_actor_metadata();

  caors::resources::resource_dependency_list dependencies;
  caors::resources::resource_dependency_collector_ops collector =
      caors::resources::make_resource_dependency_collector(dependencies);

  caors::serialization::context context{};
  context.resolvers.external_refs.collector = &collector;

  const resource_ref_actor actor{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::serialization::serialization_text_result json = caors::to_string(context, actor);

  REQUIRE(json.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == texture_guid);
  CHECK(dependencies.entries()[0].type == caors::meta::id_of<serialized_texture_resource>());

  dependencies.clear();
  resource_ref_actor decoded{};
  const caors::error status =
      caors::deserialize(context, decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == texture_guid);
  CHECK(dependencies.entries()[0].type == caors::meta::id_of<serialized_texture_resource>());
}

// 原测试文件：serialization_resource_ref_serialization_deserializes_resource_refs_as_unresolved_guids_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - deserializes resource refs as unresolved guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  resource_ref_actor decoded{};
  const caors::error status =
      caors::deserialize(decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  CHECK(decoded.texture.guid() == texture_guid);
  CHECK_FALSE(decoded.texture.resolved());
  CHECK(decoded.texture.get() == nullptr);

  caors::resource_cache<serialized_texture_resource> cache;
  serialized_texture_resource* texture = cache.create(texture_guid);
  REQUIRE(texture != nullptr);

  REQUIRE(cache.resolve(decoded.texture));
  CHECK(decoded.texture.resolved());
  CHECK(decoded.texture.get() == texture);
}

// 原测试文件：serialization_resource_ref_serialization_rejects_invalid_resource_ref_guids_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - rejects invalid resource ref guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize(decoded, R"({"texture":"not-a-guid"})");

  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::invalid_argument);
}

// 原测试文件：serialization_resource_ref_serialization_reports_required_missing_resource_refs_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;

TEST_CASE("serialization - required resource ref resolution reports missing resources",
          "[serialization]") {
  require_resource_ref_actor_metadata();

  caors::resource_cache<serialized_texture_resource> cache;
  const caors::resources::resource_ref_resolver_ops resolver =
      caors::resources::make_resource_resolver(cache);

  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;
  context.resolvers.external_refs.resolution =
      caors::resources::resource_ref_resolution::require_resolved;

  resource_ref_actor decoded{};
  const caors::error status =
      caors::deserialize(context, decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  CHECK(status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK_FALSE(decoded.texture.has_guid());
  CHECK_FALSE(decoded.texture.resolved());
}

// 原测试文件：serialization_resource_ref_serialization_resolves_resource_refs_with_context_resolver_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;

TEST_CASE("serialization - resource ref can resolve through context resolver", "[serialization]") {
  require_resource_ref_actor_metadata();

  caors::resource_cache<serialized_texture_resource> cache;
  serialized_texture_resource* texture = cache.create(texture_guid);
  REQUIRE(texture != nullptr);
  caors::resource_cache<serialized_texture_resource> missing_cache;

  const caors::resources::resource_ref_resolver_ops resolver =
      caors::resources::make_resource_resolver(cache);
  const caors::resources::resource_ref_resolver_ops missing_resolver =
      caors::resources::make_resource_resolver(missing_cache);

  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;
  context.resolvers.external_refs.resolution =
      caors::resources::resource_ref_resolution::resolve_if_present;

  resource_ref_actor decoded{};
  const caors::error status =
      caors::deserialize(context, decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  CHECK(decoded.texture.guid() == texture_guid);
  CHECK(decoded.texture.resolved());
  CHECK(decoded.texture.get() == texture);

  caors::serialization::context unresolved_context{};
  unresolved_context.resolvers.external_refs.resolver = &missing_resolver;
  unresolved_context.resolvers.external_refs.resolution =
      caors::resources::resource_ref_resolution::keep_unresolved;

  resource_ref_actor unresolved{};
  const caors::error unresolved_status = caors::deserialize(
      unresolved_context, unresolved, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(unresolved_status.succeeded());
  CHECK(unresolved.texture.guid() == texture_guid);
  CHECK_FALSE(unresolved.texture.resolved());
}

TEST_CASE("serialization - external ref interfaces adapt resource refs", "[serialization]") {
  require_resource_ref_actor_metadata();

  class test_collector final : public caors::serialization::external_ref_collector_interface {
  public:
    void collect(const caors::serialization::external_ref& ref) override {
      ++count;
      last = ref;
    }

    int count = 0;
    caors::serialization::external_ref last{};
  };

  class test_resolver final : public caors::serialization::external_ref_resolver_interface {
  public:
    explicit test_resolver(caors::resource_cache<serialized_texture_resource>& source)
        : source_(&source) {}

    [[nodiscard]] void* resolve(const caors::serialization::external_ref& ref) override {
      if (ref.domain != caors::resources::resource_ref_domain()) {
        return nullptr;
      }

      return source_->find(caors::resource_guid{ref.id});
    }

  private:
    caors::resource_cache<serialized_texture_resource>* source_ = nullptr;
  };

  test_collector collector_interface;
  const caors::serialization::external_ref_collector_ops collector =
      caors::serialization::make_external_ref_collector(collector_interface);

  caors::serialization::context collect_context{};
  collect_context.resolvers.external_refs.collector = &collector;

  const resource_ref_actor actor{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::serialization::serialization_text_result json =
      caors::to_string(collect_context, actor);

  REQUIRE(json.succeeded());
  CHECK(collector_interface.count == 1);
  CHECK(collector_interface.last.domain == caors::resources::resource_ref_domain());
  CHECK(collector_interface.last.id == texture_guid.value);
  CHECK(collector_interface.last.type == caors::meta::id_of<serialized_texture_resource>());

  caors::resource_cache<serialized_texture_resource> cache;
  serialized_texture_resource* texture = cache.create(texture_guid);
  REQUIRE(texture != nullptr);

  test_resolver resolver_interface{cache};
  const caors::serialization::external_ref_resolver_ops resolver =
      caors::serialization::make_external_ref_resolver(resolver_interface);

  caors::serialization::context resolve_context{};
  resolve_context.resolvers.external_refs.resolver = &resolver;
  resolve_context.resolvers.external_refs.resolution =
      caors::resources::resource_ref_resolution::require_resolved;

  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize(resolve_context, decoded, json.value);

  REQUIRE(status.succeeded());
  CHECK(decoded.texture.resolved());
  CHECK(decoded.texture.get() == texture);
}

// 原测试文件：serialization_resource_ref_serialization_serializes_empty_resource_refs_as_null_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - serializes empty resource refs as null", "[serialization]") {
  require_resource_ref_actor_metadata();

  const resource_ref_actor actor{};
  const caors::serialization::serialization_text_result json = caors::to_string(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"texture":null})");

  resource_ref_actor decoded{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::error status = caors::deserialize(decoded, R"({"texture":null})");

  REQUIRE(status.succeeded());
  CHECK_FALSE(decoded.texture);
  CHECK(decoded.texture.guid() == caors::resource_guid::nil());
  CHECK_FALSE(decoded.texture.resolved());
}

// 原测试文件：serialization_resource_ref_serialization_serializes_resource_refs_as_stable_guids_test.cpp

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - serializes resource refs as stable guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  const resource_ref_actor actor{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::serialization::serialization_text_result json = caors::to_string(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* texture =
      value.value.find_member(caors::string_name{"texture"});
  REQUIRE(texture != nullptr);
  REQUIRE(texture->value.is_string());
  CHECK(texture->value.as_string() == "6cd2da3d-b549-4756-8b8b-0c84293af410");
}
