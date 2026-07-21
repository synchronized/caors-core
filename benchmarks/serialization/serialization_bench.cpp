// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include <ctrack.hpp>

#include <array>
#include <cstdlib>
#include <iostream>

namespace caors::benchmarks {

void benchmark_serialization(std::size_t iterations) {
  const benchmark_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization::type_schema actor_schema = make_benchmark_actor_schema();
  const caors::meta::instance instance = caors::meta::view(actor);
  const caors::meta::type actor_type = caors::meta::find<benchmark_actor>();
  const std::array<caors::meta::property, 4> actor_properties{
      actor_type.find_property("id"),
      actor_type.find_property("health"),
      actor_type.find_property("active"),
      actor_type.find_property("name"),
  };
  for (const caors::meta::property& property : actor_properties) {
    if (!property) {
      std::cerr << "benchmark 序列化属性未注册\n";
      std::exit(EXIT_FAILURE);
    }
  }

  const caors::serialization::variant_result parsed_actor_json =
      caors::serialization::parse_json(benchmark_actor_json);
  if (parsed_actor_json.failed()) {
    std::cerr << "解析 benchmark JSON 输入失败\n";
    std::exit(EXIT_FAILURE);
  }

  {
    CTRACK_NAME("serialization.to_value");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::variant_result result = caors::to_value(actor);
      if (result.failed()) {
        std::cerr << "序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
  }

  {
    CTRACK_NAME("serialization.schema_view_find");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::serialization::schema_view schema =
          caors::serialization::find_schema(actor_type);
      consume(static_cast<std::uintptr_t>(schema ? schema.field_count() : 0U));
    }
  }

  {
    CTRACK_NAME("serialization.bound_schema_fields");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::serialization::schema_view schema =
          actor_type.get_capability<caors::serialization::schema_view>();
      const caors::serialization::type_schema* raw_schema = schema.get();
      if (raw_schema == nullptr) {
        std::cerr << "benchmark 序列化 schema 未注册\n";
        std::exit(EXIT_FAILURE);
      }

      for (const caors::serialization::field_schema& field : raw_schema->fields) {
        consume(static_cast<std::uintptr_t>(field.property ? 1U : 0U));
      }
    }
  }

  {
    CTRACK_NAME("serialization.schema_property_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      for (const caors::serialization::field_schema& field : actor_schema.fields) {
        const caors::meta::property property = actor_type.find_property(field.field_name);
        consume(static_cast<std::uintptr_t>(property ? 1U : 0U));
      }
    }
  }

  {
    CTRACK_NAME("serialization.reflect_properties_get_ref");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::any_ref id = actor_properties[0].get_ref(instance);
      const caors::any_ref health = actor_properties[1].get_ref(instance);
      const caors::any_ref active = actor_properties[2].get_ref(instance);
      const caors::any_ref name = actor_properties[3].get_ref(instance);
      consume_address(id.try_get<int>());
      consume_address(health.try_get<float>());
      consume_address(active.try_get<bool>());
      consume_address(name.try_get<caors::string>());
    }
  }

  {
    CTRACK_NAME("serialization.field_refs_to_variant");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::serialization::variant_result id =
          caors::serialization::to_variant(actor_properties[0].get_ref(instance));
      const caors::serialization::variant_result health =
          caors::serialization::to_variant(actor_properties[1].get_ref(instance));
      const caors::serialization::variant_result active =
          caors::serialization::to_variant(actor_properties[2].get_ref(instance));
      const caors::serialization::variant_result name =
          caors::serialization::to_variant(actor_properties[3].get_ref(instance));
      if (id.failed() || health.failed() || active.failed() || name.failed()) {
        std::cerr << "字段 ref 转 DOM benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(id.value);
      consume_value(health.value);
      consume_value(active.value);
      consume_value(name.value);
    }
  }

  {
    CTRACK_NAME("serialization.manual_variant_object_direct");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::variant result = make_benchmark_actor_variant_direct(actor);
      consume_value(result);
    }
  }

  {
    CTRACK_NAME("serialization.write_value_to_json_reused_buffer");
    const caors::variant actor_value = make_benchmark_actor_variant_direct(actor);
    caors::string output;
    output.reserve(benchmark_actor_json.size());
    caors::serialization::json_writer writer{output};
    for (std::size_t index = 0; index < iterations; ++index) {
      output.clear();
      writer.reset();
      const caors::error status = caors::serialization::write_value_to(writer, actor_value);
      if (status.failed()) {
        std::cerr << "JSON 写出 benchmark DOM 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(output.size()));
    }
  }

  {
    CTRACK_NAME("serialization.direct_json_writer_object");
    caors::string output;
    output.reserve(benchmark_actor_json.size());
    caors::serialization::json_writer writer{output};
    for (std::size_t index = 0; index < iterations; ++index) {
      output.clear();
      writer.reset();
      const caors::error status = write_benchmark_actor_json_direct(writer, actor, actor_schema);
      if (status.failed()) {
        std::cerr << "直接 JSON writer benchmark 失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(output.size()));
    }
  }

  {
    CTRACK_NAME("serialization.write_json_to_reused_buffer");
    caors::string output;
    output.reserve(benchmark_actor_json.size());
    for (std::size_t index = 0; index < iterations; ++index) {
      output.clear();
      const caors::error status = caors::serialization::write_json_to(actor, output);
      if (status.failed()) {
        std::cerr << "JSON 复用 buffer 序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(output.size()));
    }
  }

  {
    CTRACK_NAME("serialization.to_json_allocate");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::serialization_text_result result = caors::to_json(actor);
      if (result.failed()) {
        std::cerr << "JSON 序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
  }

  {
    CTRACK_NAME("serialization.parse_json");
    for (std::size_t index = 0; index < iterations; ++index) {
      caors::serialization::variant_result result =
          caors::serialization::parse_json(benchmark_actor_json);
      if (result.failed()) {
        std::cerr << "JSON parse benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_value(result.value);
    }
  }

  {
    CTRACK_NAME("serialization.deserialize_from_variant");
    for (std::size_t index = 0; index < iterations; ++index) {
      benchmark_actor target{};
      const caors::error status = caors::read_value_from(target, parsed_actor_json.value);
      if (status.failed()) {
        std::cerr << "DOM 反序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(target.id));
    }
  }

  {
    CTRACK_NAME("serialization.read_json_from_text_reader");
    for (std::size_t index = 0; index < iterations; ++index) {
      benchmark_actor target{};
      const caors::error status = caors::read_json_from(target, benchmark_actor_json);
      if (status.failed()) {
        std::cerr << "JSON 反序列化 benchmark 对象失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume(static_cast<std::uintptr_t>(target.id));
    }
  }
}

} // namespace caors::benchmarks
