// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/canonical_type_name.h"
#include "caors/meta/detail/type_identity.h"

#include <catch2/catch_all.hpp>

#include <string>
#include <thread>
#include <vector>

namespace {

struct canonical_test_type {};

using function_type = int(double, const char*);
using member_pointer_type = int canonical_test_type::*;

constexpr std::string_view int_name = caors::meta::detail::canonical_cpp_type_name<int>();
constexpr std::string_view const_int_ref_name =
    caors::meta::detail::canonical_cpp_type_name<const int&>();
constexpr std::string_view test_type_name =
    caors::meta::detail::canonical_cpp_type_name<canonical_test_type>();

static_assert(!int_name.empty());
static_assert(int_name == const_int_ref_name);
static_assert(int_name != test_type_name);

} // namespace

TEST_CASE("canonical type name normalizes top-level cv and reference", "[meta][type_identity]") {
  CHECK(caors::meta::detail::canonical_cpp_type_name<int>() ==
        caors::meta::detail::canonical_cpp_type_name<const int&>());
  CHECK(caors::meta::detail::canonical_cpp_type_name<volatile int&&>() ==
        caors::meta::detail::canonical_cpp_type_name<int>());
}

TEST_CASE("canonical type name distinguishes complete C++ types", "[meta][type_identity]") {
  CHECK(caors::meta::detail::canonical_cpp_type_name<int*>() !=
        caors::meta::detail::canonical_cpp_type_name<const int*>());
  CHECK(caors::meta::detail::canonical_cpp_type_name<int[3]>() !=
        caors::meta::detail::canonical_cpp_type_name<int[]>());
  CHECK(caors::meta::detail::canonical_cpp_type_name<std::vector<int>>() !=
        caors::meta::detail::canonical_cpp_type_name<std::vector<std::string>>());
  CHECK(caors::meta::detail::canonical_cpp_type_name<function_type>() !=
        caors::meta::detail::canonical_cpp_type_name<member_pointer_type>());
}

TEST_CASE("type identity state reuses canonical names and allocates distinct ids",
          "[meta][type_identity]") {
  const caors::meta::detail::type_identity& first =
      caors::meta::detail::get_or_create_type_identity("caors.test.identity.first");
  const caors::meta::detail::type_identity& repeated =
      caors::meta::detail::get_or_create_type_identity("caors.test.identity.first");
  const caors::meta::detail::type_identity& second =
      caors::meta::detail::get_or_create_type_identity("caors.test.identity.second");

  CHECK(&first == &repeated);
  CHECK(first.id == repeated.id);
  CHECK(first.id != second.id);
  CHECK(first.canonical_name == "caors.test.identity.first");
}

TEST_CASE("type identity state unifies concurrent first lookup", "[meta][type_identity]") {
  constexpr std::size_t thread_count = 8;
  std::vector<const caors::meta::detail::type_identity*> identities(thread_count);
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  for (std::size_t index = 0; index < thread_count; ++index) {
    threads.emplace_back([&, index] {
      identities[index] =
          &caors::meta::detail::get_or_create_type_identity("caors.test.identity.concurrent");
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  for (const caors::meta::detail::type_identity* identity : identities) {
    CHECK(identity == identities.front());
  }
}
