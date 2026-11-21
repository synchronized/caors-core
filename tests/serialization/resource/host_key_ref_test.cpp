// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/serialization.h"
#include "caors/serialization/json_reader.h"
#include "caors/serialization/json_writer.h"
#include <catch2/catch_all.hpp>
#include <string>

namespace {
struct host_texture {};
using path_ref = caors::resources::ref<host_texture, std::string>;
using path_codec = caors::serialization::codec_traits<path_ref>;
template <typename Codec, typename Ref> caors::error read_key(std::string_view text, Ref& value) {
  // reader 的标量读取发生在字段或数组元素中，不能把裸 DOM 根当作当前元素。
  std::string array{"["};
  array.append(text);
  array += ']';
  caors::serialization::json_reader input(array);
  std::size_t size = 0;
  auto status = input.begin_array(size);
  if (!status)
    status = input.begin_element(0);
  return status ? status : Codec::read(input, value);
}
}
TEST_CASE("serialization - host resource references persist paths without GUID conversion",
          "[serialization][resources][host-key]") {
  path_ref source{std::string{"textures/wood.png"}};
  host_texture texture;
  source.resolve(&texture, 7);
  caors::serialization::json_writer output;
  REQUIRE_FALSE(path_codec::write(output, source));
  CHECK(output.text() == "\"textures/wood.png\"");
  auto value = path_codec::make_value(source);
  REQUIRE(value);
  path_ref decoded;
  REQUIRE_FALSE(read_key<path_codec>(output.text(), decoded));
  REQUIRE(decoded.key());
  CHECK(*decoded.key() == "textures/wood.png");
  CHECK_FALSE(decoded.resolved());
  CHECK_FALSE(decoded.has_generation());
  CHECK(read_key<path_codec>("123", decoded));
  CHECK(*decoded.key() == "textures/wood.png");
  REQUIRE_FALSE(read_key<path_codec>("null", decoded));
  CHECK_FALSE(decoded.key());
  caors::serialization::context context;
  context.resolvers.external_refs.resolution =
      caors::serialization::external_ref_resolution::require_resolved;
  caors::serialization::json_reader required("[\"textures/wood.png\"]");
  std::size_t size = 0;
  REQUIRE_FALSE(required.begin_array(size));
  REQUIRE_FALSE(required.begin_element(0));
  CHECK(path_codec::read(context, required, decoded).value() == caors::errc::unsupported);
  CHECK_FALSE(decoded.key());
}
TEST_CASE("serialization - plain GUID object reference serializes only its host key",
          "[serialization][resources][host-key]") {
  using guid_ref = caors::resources::ref<host_texture>;
  using codec = caors::serialization::codec_traits<guid_ref>;
  auto guid = caors::resource_guid::parse("11223344-5566-7788-99aa-bbccddeeff00");
  REQUIRE(guid);
  guid_ref source{*guid};
  auto value = codec::make_value(source);
  REQUIRE(value);
  caors::serialization::json_writer output;
  REQUIRE_FALSE(codec::write(output, source));
  guid_ref decoded;
  REQUIRE_FALSE(read_key<codec>(output.text(), decoded));
  REQUIRE(decoded.key());
  CHECK(*decoded.key() == *guid);
}
