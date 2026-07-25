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
  constexpr std::string_view first_name = "caors.test.identity.first";
  constexpr std::string_view second_name = "caors.test.identity.second";
  const caors::type_id first =
      caors::meta::detail::get_or_create_type_id(first_name.data(), first_name.size());
  const caors::type_id repeated =
      caors::meta::detail::get_or_create_type_id(first_name.data(), first_name.size());
  const caors::type_id second =
      caors::meta::detail::get_or_create_type_id(second_name.data(), second_name.size());

  CHECK(first == repeated);
  CHECK(first != second);
}

TEST_CASE("type identity state unifies concurrent first lookup", "[meta][type_identity]") {
  constexpr std::size_t thread_count = 8;
  std::vector<caors::type_id> identities(thread_count);
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  for (std::size_t index = 0; index < thread_count; ++index) {
    threads.emplace_back([&, index] {
      constexpr std::string_view name = "caors.test.identity.concurrent";
      identities[index] = caors::meta::detail::get_or_create_type_id(name.data(), name.size());
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  for (const caors::type_id identity : identities) {
    CHECK(identity == identities.front());
  }
}

TEST_CASE("C++ type id uses the runtime identity state", "[meta][type_identity]") {
  constexpr std::string_view name =
      caors::meta::detail::canonical_cpp_type_name<canonical_test_type>();
  const caors::type_id identity =
      caors::meta::detail::get_or_create_type_id(name.data(), name.size());

  CHECK(caors::meta::detail::type_id_of_cpp_type<canonical_test_type>() == identity);
  CHECK(caors::meta::detail::type_id_of_cpp_type<const canonical_test_type&>() == identity);
}
