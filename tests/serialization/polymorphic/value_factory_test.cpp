// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_polymorphic_value_factory_polymorphic_value_factory_roundtrips_unique_pointer_test.cpp

#include "serialization_polymorphic_value_factory_test_support.h"

#include <cstdint>

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

namespace {

class envelope_reader final : public caors::serialization::reader {
public:
  explicit envelope_reader(const caors::meta::variant& value) noexcept : reader_(value) {}

  [[nodiscard]] caors::serialization::format_features features() const noexcept override {
    return reader_.features();
  }

  [[nodiscard]] caors::error begin_object(const caors::serialization::type_schema& type) override {
    return reader_.begin_object(type);
  }

  [[nodiscard]] caors::error begin_field(const caors::serialization::field_schema& field) override {
    return reader_.begin_field(field);
  }

  [[nodiscard]] caors::error begin_array(std::size_t& size) override {
    return reader_.begin_array(size);
  }

  [[nodiscard]] caors::error begin_element(std::size_t index) override {
    return reader_.begin_element(index);
  }

  [[nodiscard]] caors::error begin_dictionary(std::size_t& size) override {
    return reader_.begin_dictionary(size);
  }

  [[nodiscard]] caors::error begin_entry(std::size_t index) override {
    return reader_.begin_entry(index);
  }

  [[nodiscard]] caors::error begin_key() override { return reader_.begin_key(); }
  [[nodiscard]] caors::error end_key() override { return reader_.end_key(); }
  [[nodiscard]] caors::error begin_value() override { return reader_.begin_value(); }
  [[nodiscard]] caors::error end_value() override { return reader_.end_value(); }
  [[nodiscard]] caors::error end_entry() override { return reader_.end_entry(); }
  [[nodiscard]] caors::error end_dictionary() override { return reader_.end_dictionary(); }

  [[nodiscard]] caors::error read_bool(bool& value) override { return reader_.read_bool(value); }
  [[nodiscard]] caors::error read_i64(std::int64_t& value) override {
    return reader_.read_i64(value);
  }
  [[nodiscard]] caors::error read_u64(std::uint64_t& value) override {
    return reader_.read_u64(value);
  }
  [[nodiscard]] caors::error read_f64(double& value) override { return reader_.read_f64(value); }
  [[nodiscard]] caors::error read_string(caors::string& value) override {
    return reader_.read_string(value);
  }

  [[nodiscard]] bool is_null() const noexcept override { return reader_.is_null(); }
  [[nodiscard]] const caors::meta::variant* current_variant() const noexcept override {
    return nullptr;
  }

  [[nodiscard]] bool
  is_polymorphic_envelope(const caors::serialization::context& context) const noexcept override {
    return reader_.is_polymorphic_envelope(context);
  }

  [[nodiscard]] caors::error
  read_polymorphic_type_guid(const caors::serialization::context& context,
                             caors::type_guid& value) override {
    return reader_.read_polymorphic_type_guid(context, value);
  }

  [[nodiscard]] caors::error
  begin_polymorphic_value(const caors::serialization::context& context) override {
    return reader_.begin_polymorphic_value(context);
  }

  [[nodiscard]] caors::error end_polymorphic_value() override {
    return reader_.end_polymorphic_value();
  }

  [[nodiscard]] caors::error end_element() override { return reader_.end_element(); }
  [[nodiscard]] caors::error end_array() override { return reader_.end_array(); }
  [[nodiscard]] caors::error end_field() override { return reader_.end_field(); }
  [[nodiscard]] caors::error end_object() override { return reader_.end_object(); }

private:
  caors::serialization::variant_reader reader_;
};

} // namespace

TEST_CASE("serialization - polymorphic value factory roundtrips unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 42;
  player->name = "Una";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::to_string(holder);
  REQUIRE(json.succeeded());

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 42);
  CHECK(decoded_owner->name == "Una");
}

TEST_CASE("serialization - polymorphic value factory reads envelope through reader hooks",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 44;
  player->name = "Via";
  holder.owner = std::move(owner);

  const caors::serialization::variant_result value = caors::to_value(holder);
  REQUIRE(value.succeeded());

  envelope_reader reader{value.value};
  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize(decoded, reader);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 44);
  CHECK(decoded_owner->name == "Via");
}

// 原测试文件：serialization_polymorphic_value_factory_polymorphic_value_factory_uses_memory_resource_for_unique_pointer_test.cpp

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory uses memory resource for unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 43;
  player->name = "Uma";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::to_string(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize(context, decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 43);
  CHECK(decoded_owner->name == "Uma");
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.owner.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：serialization_polymorphic_value_factory_polymorphic_value_factory_uses_memory_resource_test.cpp

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory uses memory resource", "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_holder holder{};
  auto shared = std::make_shared<polymorphic_factory_player>(1);
  shared->level = 41;
  shared->name = "Rin";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::to_string(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  polymorphic_factory_holder decoded{};
  const caors::error status = caors::deserialize(context, decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_shared = dynamic_cast<polymorphic_factory_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 41);
  CHECK(decoded_shared->name == "Rin");
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.shared.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
