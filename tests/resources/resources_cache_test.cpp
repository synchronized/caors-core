// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache creates and indexes resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  texture_resource* texture = cache.create(test_guid_a);
  REQUIRE(texture != nullptr);

  CHECK(cache.size() == 1);
  CHECK(cache.contains(test_guid_a));
  CHECK(texture->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == texture);
  CHECK(cache.find_as<texture_resource>(test_guid_a) == texture);
  CHECK(cache.index().find(test_guid_a) == texture);
}

TEST_CASE("resources - cache rejects invalid duplicate and mismatched resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  CHECK(cache.create(caors::resource_guid::nil()) == nullptr);
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.create(test_guid_a) == nullptr);
  CHECK(cache.create(test_guid_b, test_guid_a) == nullptr);
  CHECK_FALSE(cache.contains(test_guid_b));
}

TEST_CASE("resources - cache resolves refs through internal manager", "[resources]") {
  caors::resource_cache<image_texture_resource> cache;
  image_texture_resource* image = cache.create(test_guid_a);
  REQUIRE(image != nullptr);

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(cache.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == image);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(cache.resolve(derived_ref));
  CHECK(derived_ref.get() == image);
}

TEST_CASE("resources - cache inserts owned resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  auto texture = caors::memory::make_unique<texture_resource>(nullptr, test_guid_a);
  texture_resource* stored = texture.get();

  REQUIRE(cache.insert(std::move(texture)));

  CHECK(cache.size() == 1);
  CHECK(cache.find(test_guid_a) == stored);
  CHECK(cache.index().find(test_guid_a) == stored);
  CHECK_FALSE(cache.insert({}));
}

TEST_CASE("resources - cache erase destroys owned resource", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.erase(test_guid_a));

  CHECK(observed_resource::destroyed == 1);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK_FALSE(cache.erase(test_guid_a));
}

TEST_CASE("resources - cache release transfers ownership", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  auto released = cache.release(test_guid_a);

  REQUIRE(released != nullptr);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK(released->guid() == test_guid_a);
  CHECK(observed_resource::destroyed == 0);

  released.reset();
  CHECK(observed_resource::destroyed == 1);
}

TEST_CASE("resources - loader can populate cache", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::result<texture_resource*> loaded = caors::load_into(cache, loader, test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);

  CHECK(loader.load_count == 1);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == loaded.value);

  caors::result<texture_resource*> cached = caors::load_into(cache, loader, test_guid_a);
  REQUIRE(cached.succeeded());
  CHECK(cached.value == loaded.value);
  CHECK(loader.load_count == 1);
}

TEST_CASE("resources - saver interface receives resources", "[resources]") {
  texture_resource texture{test_guid_a};
  counting_texture_saver saver;

  const caors::error status = saver.save(texture);

  REQUIRE(status.succeeded());
  CHECK(saver.save_count == 1);
  CHECK(saver.saved_guid == test_guid_a);
}

TEST_CASE("resources - saver can persist cache entries", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  counting_texture_saver saver;

  caors::error saved = caors::save_from(cache, saver, test_guid_a);
  REQUIRE(saved.succeeded());
  CHECK(saver.save_count == 1);
  CHECK(saver.saved_guid == test_guid_a);

  caors::error invalid = caors::save_from(cache, saver, caors::resource_guid::nil());
  REQUIRE(invalid.failed());
  CHECK(invalid.domain() == caors::error_domain::resources);
  CHECK(invalid.value() == caors::errc::invalid_argument);

  caors::error missing = caors::save_from(cache, saver, test_guid_b);
  REQUIRE(missing.failed());
  CHECK(missing.domain() == caors::error_domain::resources);
  CHECK(missing.value() == caors::errc::not_found);
  CHECK(saver.save_count == 1);
}

TEST_CASE("resources - serialized helpers load and save resources", "[resources]") {
  require_serialized_resource_metadata();

  const caors::serialization::variant_result parsed = caors::parse_json(R"({"width":256})");
  REQUIRE(parsed.succeeded());

  caors::serialization::variant_reader reader{parsed.value};
  caors::resource_load_result<serialized_resource> loaded =
      caors::load_serialized<serialized_resource>(test_guid_a, reader);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 256);

  caors::string output;
  caors::serialization::json_writer writer{output};
  const caors::error save_status = caors::save_serialized(*loaded.value, writer);

  REQUIRE(save_status.succeeded());
  CHECK(output == R"({"width":256})");
}

TEST_CASE("resources - serialized helpers reject invalid resource identity", "[resources]") {
  require_serialized_resource_metadata();

  const caors::serialization::variant_result parsed = caors::parse_json(R"({"width":256})");
  REQUIRE(parsed.succeeded());

  caors::serialization::variant_reader reader{parsed.value};
  caors::resource_load_result<serialized_resource> loaded =
      caors::load_serialized<serialized_resource>(caors::resource_guid::nil(), reader);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::resources);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  serialized_resource value{};
  caors::serialization::json_writer writer;
  const caors::error save_status = caors::save_serialized(value, writer);

  REQUIRE(save_status.failed());
  CHECK(save_status.domain() == caors::error_domain::resources);
  CHECK(save_status.value() == caors::errc::invalid_argument);
}

TEST_CASE("resources - cache uses requested memory resource", "[resources]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::resource_cache<texture_resource> cache{&tracking};

  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.resource() == &tracking);
  CHECK(cache.index().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
}
