// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/async.h"
#include "caors/resources/resource_manager.h"
#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

namespace {
struct image {
  explicit image(int value) : value(value) {}
  int value;
};
class network_loader final : public caors::resources::loader<image, std::string> {
public:
  caors::error start(caors::resources::basic_load_context<std::string> context,
                     caors::resources::load_completion<image> completion) override {
    keys.push_back(std::move(context.key));
    pending.push_back(std::move(completion));
    return {};
  }
  void finish(std::size_t index, int value) {
    auto completion = std::move(pending[index]);
    REQUIRE(completion.complete({{}, caors::memory::make_unique<image>(nullptr, value)}));
  }
  std::vector<std::string> keys;
  std::vector<caors::resources::load_completion<image>> pending;
};
}

TEST_CASE("resources - path loading uses existing shared request and completion machinery",
          "[resources][host-key]") {
  caors::resources::cache<image, std::string> cache;
  caors::resources::load_request_table<image, std::string> requests;
  network_loader loader;
  caors::tasks::execution_context worker, completion;
  std::string key = "https://example.invalid/texture";
  auto first = caors::resources::request_load_into(cache, loader, key, worker.get_executor(),
                                                   completion.get_executor(), requests);
  auto second = caors::resources::request_load_into(cache, loader, key, worker.get_executor(),
                                                    completion.get_executor(), requests);
  key.assign("changed");
  (void)worker.drain();
  REQUIRE(loader.keys.size() == 1);
  CHECK(loader.keys[0] == "https://example.invalid/texture");
  CHECK_FALSE(first.done());
  CHECK(first.cancel());
  loader.finish(0, 42);
  (void)completion.drain();
  REQUIRE(second.result());
  CHECK(second.result().value->value == 42);
  REQUIRE(cache.find("https://example.invalid/texture"));
  CHECK(cache.find("https://example.invalid/texture")->value == 42);
  CHECK(requests.size() == 0);
  auto hit = caors::resources::request_load_into(cache, loader, "https://example.invalid/texture",
                                                 worker.get_executor(), completion.get_executor(),
                                                 requests);
  REQUIRE(hit.result());
  CHECK(loader.keys.size() == 1);
}

TEST_CASE("resources - path reload publishes latest intent and retains old handles",
          "[resources][host-key]") {
  caors::resources::cache<image, std::string> cache;
  auto old = cache.create_handle("item", 1);
  network_loader loader;
  caors::tasks::execution_context worker, completion;
  auto earlier = caors::resources::request_reload_into(cache, loader, "item", worker.get_executor(),
                                                       completion.get_executor());
  auto latest = caors::resources::request_reload_into(cache, loader, "item", worker.get_executor(),
                                                      completion.get_executor());
  (void)worker.drain();
  REQUIRE(loader.keys.size() == 2);
  loader.finish(1, 3);
  loader.finish(0, 2);
  (void)completion.drain();
  REQUIRE(earlier.result());
  REQUIRE(latest.result());
  CHECK(earlier.result().value->value == 2);
  CHECK(cache.find("item")->value == 3);
  CHECK(old->value == 1);
  auto failed = caors::resources::request_reload_into(cache, loader, "item", worker.get_executor(),
                                                      completion.get_executor());
  (void)worker.drain();
  REQUIRE(loader.pending.back().complete(
      {{caors::error_domain::resources, caors::errc::not_found}, {}}));
  (void)completion.drain();
  CHECK(failed.result().failed());
  CHECK(cache.find("item")->value == 3);
}
