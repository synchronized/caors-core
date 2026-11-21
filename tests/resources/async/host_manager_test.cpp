// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/memory/tracking.h"
#include "caors/resources/resource_manager.h"
#include <catch2/catch_all.hpp>
#include <cctype>
#include <optional>
#include <string>

namespace {
struct host_image {
  int value = 1;
};
struct host_material {
  int value = 2;
};
struct unregistered {};
struct collision_hash {
  std::size_t operator()(const std::string&) const noexcept { return 0; }
};
struct host_equal {
  bool fold = false;
  bool operator()(const std::string& a, const std::string& b) const {
    if (!fold)
      return a == b;
    if (a.size() != b.size())
      return false;
    for (std::size_t i = 0; i < a.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(a[i])) !=
          std::tolower(static_cast<unsigned char>(b[i])))
        return false;
    return true;
  }
};
template <typename T>
class deferred_loader final : public caors::resources::loader<T, std::string> {
public:
  caors::error start(caors::resources::basic_load_context<std::string> context,
                     caors::resources::load_completion<T> receipt) override {
    ++calls;
    resource = context.resource;
    pending.emplace(std::move(receipt));
    return {};
  }
  bool finish() {
    auto receipt = std::move(*pending);
    pending.reset();
    return receipt.complete({{}, caors::memory::make_unique<T>(resource)});
  }
  int calls = 0;
  caors::memory_resource* resource = nullptr;
  std::optional<caors::resources::load_completion<T>> pending;
};
}

TEST_CASE("resources - host manager isolates resource types and closes bindings",
          "[resources][host-key]") {
  deferred_loader<host_image> images;
  deferred_loader<host_material> materials;
  caors::tasks::execution_context worker, completion;
  caors::resources::resource_manager<std::string> manager{
      {worker.get_executor(), completion.get_executor()}};
  REQUIRE_FALSE(manager.register_loader(images));
  REQUIRE_FALSE(manager.register_loader(materials));
  CHECK(manager.register_loader(images).value() == caors::errc::already_exists);
  CHECK(manager.request_load<unregistered>("same").error().value() == caors::errc::not_found);
  auto image = manager.request_load<host_image>("same");
  auto material = manager.request_load<host_material>("same");
  (void)worker.drain();
  REQUIRE(images.finish());
  REQUIRE(materials.finish());
  (void)completion.drain();
  REQUIRE(image.result());
  REQUIRE(material.result());
  CHECK(manager.find<host_image>("same")->value == 1);
  CHECK(manager.find<host_material>("same")->value == 2);
  caors::resources::ref<host_image, std::string> reference{std::string{"same"}};
  REQUIRE(manager.resolve(reference));
  CHECK(manager.is_current(reference));
  manager.close();
  CHECK(manager.closed());
  CHECK_FALSE(manager.find<host_image>("same"));
  CHECK_FALSE(manager.is_current(reference));
  CHECK_FALSE(manager.resolve(reference));
  CHECK_FALSE(reference.resolved());
  REQUIRE(reference.key());
  CHECK(*reference.key() == "same");
  CHECK(manager.request_load<host_image>("same").error().value() == caors::errc::cancelled);
  CHECK(image.result().value->value == 1);
}

TEST_CASE("resources - host manager retains stores through late completion without leaks",
          "[resources][host-key]") {
  const bool start_before_close = GENERATE(false, true);
  caors::tracking_resource memory(&caors::get_system_memory_resource());
  {
    deferred_loader<host_image> loader;
    caors::tasks::execution_context worker, completion;
    caors::resources::load_request<host_image> request;
    {
      caors::resources::resource_manager<std::string> manager{
          {worker.get_executor(), completion.get_executor(), &memory}};
      REQUIRE_FALSE(manager.register_loader(loader));
      request = manager.request_load<host_image>("late");
      if (start_before_close)
        (void)worker.drain();
    }
    CHECK_FALSE(request.done());
    if (!start_before_close)
      (void)worker.drain();
    REQUIRE(loader.finish());
    (void)completion.drain();
    REQUIRE(request.result());
    CHECK(request.result().value->value == 1);
  }
  CHECK(memory.statistics().bytes_in_use == 0);
}

TEST_CASE("resources - accepted load retains other registered stores after close",
          "[resources][host-key]") {
  caors::tracking_resource memory(&caors::get_system_memory_resource());
  {
    deferred_loader<host_image> images;
    deferred_loader<host_material> materials;
    caors::tasks::execution_context worker, completion;
    caors::resources::weak_handle<host_image> cached;
    caors::resources::load_request<host_material> pending;
    {
      caors::resources::resource_manager<std::string> manager{
          {worker.get_executor(), completion.get_executor(), &memory}};
      REQUIRE_FALSE(manager.register_loader(images));
      REQUIRE_FALSE(manager.register_loader(materials));
      {
        auto image = manager.request_load<host_image>("cached");
        (void)worker.drain();
        REQUIRE(images.finish());
        (void)completion.drain();
        cached = image.result().value;
      }
      pending = manager.request_load<host_material>("later");
      manager.close();
      CHECK_FALSE(manager.find<host_image>("cached"));
      CHECK(manager.request_load<host_image>("new").error().value() == caors::errc::cancelled);
    }
    CHECK_FALSE(cached.expired());
    (void)worker.drain();
    REQUIRE(materials.finish());
    (void)completion.drain();
    REQUIRE(pending.result());
    CHECK(cached.expired());
  }
  CHECK(memory.statistics().bytes_in_use == 0);
}

TEST_CASE("resources - host manager copies key policy into cache and request sharing",
          "[resources][host-key]") {
  deferred_loader<host_image> loader;
  caors::tasks::execution_context worker, completion;
  caors::resources::resource_manager<std::string, collision_hash, host_equal> manager{
      {worker.get_executor(), completion.get_executor()}, {}, {true}};
  REQUIRE_FALSE(manager.register_loader(loader));
  auto first = manager.request_load<host_image>("IMAGE");
  auto second = manager.request_load<host_image>("image");
  (void)worker.drain();
  CHECK(loader.calls == 1);
  REQUIRE(loader.finish());
  (void)completion.drain();
  REQUIRE(first.result());
  REQUIRE(second.result());
  CHECK(manager.find<host_image>("Image")->value == 1);
}

TEST_CASE("resources - plain objects can use GUID loading without embedded identity",
          "[resources][host-key]") {
  class guid_loader final : public caors::resources::loader<host_image> {
  public:
    caors::error start(caors::resources::async_load_context context,
                       caors::resources::load_completion<host_image> completion) override {
      observed = context.guid;
      (void)completion.complete({{}, caors::memory::make_unique<host_image>(context.resource)});
      return {};
    }
    caors::resource_guid observed;
  } loader;
  caors::resources::resource_manager<caors::resource_guid> manager{
      {caors::tasks::inline_executor(), caors::tasks::inline_executor()}};
  REQUIRE_FALSE(manager.register_loader(loader));
  auto key = caors::resource_guid::parse("11223344-5566-7788-99aa-bbccddeeff00");
  REQUIRE(key);
  auto loaded = manager.request_load<host_image>(*key);
  REQUIRE(loaded.result());
  CHECK(loader.observed == *key);
  CHECK(manager.request_load<host_image>({}).error().value() == caors::errc::invalid_argument);
}
