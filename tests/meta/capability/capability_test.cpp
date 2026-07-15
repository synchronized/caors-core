// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include "caors/adapters/stl_traits.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace {

struct runtime_capability {
  int value = 0;
};

struct runtime_capability_type {};

struct creator_capability_type {};

struct creator_ops {
  int (*value)() = nullptr;

  [[nodiscard]] bool valid() const noexcept { return value != nullptr; }
};

class creator_capability final : public caors::meta::type_capability_base<creator_ops> {
public:
  using base_type = caors::meta::type_capability_base<creator_ops>;
  using base_type::base_type;

  [[nodiscard]] int value() const noexcept { return valid() ? ops_->value() : 0; }
};

struct owned_matrix {
  int values[4]{};
};

struct proxy_matrix {
  int values[4]{};

  [[nodiscard]] std::size_t rows() const noexcept { return 2; }
  [[nodiscard]] std::size_t columns() const noexcept { return 2; }
  [[nodiscard]] int get(std::size_t row, std::size_t column) const {
    return values[row * 2 + column];
  }
  void set(std::size_t row, std::size_t column, int value) { values[row * 2 + column] = value; }
};

struct inline_proxy_matrix {
  int values[4]{};

  [[nodiscard]] std::size_t rows() const noexcept { return 2; }
  [[nodiscard]] std::size_t columns() const noexcept { return 2; }
  [[nodiscard]] int get(std::size_t row, std::size_t column) const {
    return values[row * 2 + column];
  }
  void set(std::size_t row, std::size_t column, int value) { values[row * 2 + column] = value; }
};

struct macro_proxy_matrix {
  int values[4]{};

  [[nodiscard]] std::size_t rows() const noexcept { return 2; }
  [[nodiscard]] std::size_t columns() const noexcept { return 2; }
  [[nodiscard]] int get(std::size_t row, std::size_t column) const {
    return values[row * 2 + column];
  }
  void set(std::size_t row, std::size_t column, int value) { values[row * 2 + column] = value; }
};

struct builder_proxy_matrix {
  int values[4]{};

  [[nodiscard]] static builder_proxy_matrix create() {
    builder_proxy_matrix matrix{};
    matrix.values[0] = 5;
    return matrix;
  }

  [[nodiscard]] std::size_t rows() const noexcept { return 2; }
  [[nodiscard]] std::size_t columns() const noexcept { return 2; }
  [[nodiscard]] int get(std::size_t row, std::size_t column) const {
    return values[row * 2 + column];
  }
  void set(std::size_t row, std::size_t column, int value) { values[row * 2 + column] = value; }
};

enum class facade_enum : int {
  low = -1,
  high = 9,
};

struct matrix_ops {
  std::size_t (*rows)(const void*) = nullptr;
  std::size_t (*columns)(const void*) = nullptr;
  int (*get)(const void*, std::size_t, std::size_t) = nullptr;
  void (*set)(void*, std::size_t, std::size_t, int) = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return rows != nullptr && columns != nullptr && get != nullptr && set != nullptr;
  }
};

class matrix_view final : public caors::meta::capability_view_base<matrix_ops> {
public:
  using base_type = caors::meta::capability_view_base<matrix_ops>;
  using base_type::base_type;

  matrix_view() noexcept = default;

  [[nodiscard]] std::size_t rows() const noexcept { return ops()->rows(data()); }
  [[nodiscard]] std::size_t columns() const noexcept { return ops()->columns(data()); }
  [[nodiscard]] int get(std::size_t row, std::size_t column) const {
    return ops()->get(data(), row, column);
  }
  void set(std::size_t row, std::size_t column, int value) const {
    ops()->set(mutable_data(), row, column, value);
  }
};

class matrix_capability final : public caors::meta::type_capability_base<matrix_ops, matrix_view> {
public:
  using base_type = caors::meta::type_capability_base<matrix_ops, matrix_view>;

  matrix_capability() noexcept = default;
  explicit matrix_capability(caors::meta::type type) noexcept : base_type(type) {}
};

struct static_capability_data {
  int value = 0;
};

struct static_capability_type {};

struct named_capability {};

struct first_extension_capability {
  int value = 0;
};

struct second_extension_capability {
  int value = 0;
};

struct extension_capability_type {};

[[nodiscard]] std::size_t matrix_rows(const void*) { return 2; }

[[nodiscard]] std::size_t matrix_columns(const void*) { return 2; }

[[nodiscard]] int matrix_get(const void* object, std::size_t row, std::size_t column) {
  const auto* matrix = static_cast<const owned_matrix*>(object);
  return matrix->values[row * 2 + column];
}

void matrix_set(void* object, std::size_t row, std::size_t column, int value) {
  auto* matrix = static_cast<owned_matrix*>(object);
  matrix->values[row * 2 + column] = value;
}

[[nodiscard]] int creator_value() noexcept { return 31; }

[[nodiscard]] builder_proxy_matrix make_builder_proxy_matrix() {
  builder_proxy_matrix matrix{};
  matrix.values[1] = 9;
  return matrix;
}

#define TEST_MATRIX_PROXY_METHODS(CONST_METHOD, MUTABLE_METHOD)                                    \
  CONST_METHOD(rows, std::size_t, noexcept)                                                        \
  CONST_METHOD(columns, std::size_t, noexcept)                                                     \
  CONST_METHOD(get, int, , (std::size_t, row), (std::size_t, column))                              \
  MUTABLE_METHOD(set, void, , (std::size_t, row), (std::size_t, column), (int, value))

CAORS_META_PROXY_CAPABILITY(test_matrix_capability, TEST_MATRIX_PROXY_METHODS)

#define TEST_TYPE_PROXY_METHODS(METHOD) METHOD(value, int, noexcept)

CAORS_META_PROXY_TYPE_CAPABILITY(test_creator_capability, TEST_TYPE_PROXY_METHODS)

CAORS_META_DEF_MEM_DISPATCH(test_builder_rows, rows);
CAORS_META_DEF_MEM_DISPATCH(test_builder_columns, columns);
CAORS_META_DEF_MEM_DISPATCH(test_builder_get, get);
CAORS_META_DEF_MEM_DISPATCH(test_builder_set, set);
CAORS_META_DEF_TYPE_DISPATCH(test_builder_create, create);
CAORS_META_DEF_FREE_TYPE_DISPATCH(test_builder_make, make, &make_builder_proxy_matrix);

using builder_matrix_capability =
    caors::meta::capability_builder::add_convention<test_builder_rows, std::size_t() noexcept>::
        add_convention<test_builder_columns, std::size_t() noexcept>::add_convention<
            test_builder_get,
            int(std::size_t, std::size_t)>::add_convention<test_builder_set,
                                                           void(std::size_t, std::size_t, int)>::
            add_type_convention<test_builder_create, builder_proxy_matrix()>::add_type_convention<
                test_builder_make, builder_proxy_matrix()>::build;

} // namespace

namespace caors::meta {

template <> struct capability_traits<named_capability> {
  static constexpr std::string_view name = "caors.test.meta.named_capability";
  static constexpr guid stable_guid = guid::from_bytes(guid::byte_array{
      0x2b,
      0x69,
      0x43,
      0x4a,
      0x4f,
      0x5c,
      0x42,
      0x17,
      0x88,
      0x2c,
      0x5f,
      0xe4,
      0x30,
      0x7a,
      0x9a,
      0x31,
  });
  static constexpr std::uint32_t version = 3;
};

} // namespace caors::meta

namespace caors::meta::capabilities {

struct first_extension_module {};
struct second_extension_module {};

template <> struct static_capability<static_capability_data, static_capability_type> {
  [[nodiscard]] static const static_capability_data* get() {
    static const static_capability_data data{42};
    return &data;
  }
};

template <>
struct module_static_capability_list<custom_static_capability_module, static_capability_type> {
  using type = type_list<static_capability_data>;
};

template <> struct static_capability<first_extension_capability, extension_capability_type> {
  [[nodiscard]] static const first_extension_capability* get() {
    static const first_extension_capability data{11};
    return &data;
  }
};

template <> struct static_capability<second_extension_capability, extension_capability_type> {
  [[nodiscard]] static const second_extension_capability* get() {
    static const second_extension_capability data{22};
    return &data;
  }
};

template <>
struct module_static_capability_list<first_extension_module, extension_capability_type> {
  using type = type_list<first_extension_capability>;
};

template <>
struct module_static_capability_list<second_extension_module, extension_capability_type> {
  using type = type_list<second_extension_capability>;
};

template <> struct extension_static_capability_modules<extension_capability_type> {
  using type = type_list<first_extension_module, second_extension_module>;
};

} // namespace caors::meta::capabilities

TEST_CASE("meta - capability info exposes optional stable metadata", "[meta]") {
  const caors::meta::capability_info info = caors::meta::capability_info_of<named_capability>();

  CHECK(info.valid());
  CHECK(info.key == caors::meta::capability_key_of<named_capability>());
  CHECK(info.name == "caors.test.meta.named_capability");
  CHECK(info.stable_guid);
  CHECK(info.version == 3);
}

TEST_CASE("meta - capability registry stores runtime capabilities by type id", "[meta]") {
  static const runtime_capability capability{7};
  const caors::type_id id = caors::meta::id_of<runtime_capability_type>();

  CHECK(caors::meta::capabilities::find<runtime_capability>(id) == nullptr);
  REQUIRE(caors::meta::class_<runtime_capability_type>()
              .capability<runtime_capability>(&capability)
              .succeeded());

  const runtime_capability* found = caors::meta::capabilities::find<runtime_capability>(id);
  REQUIRE(found != nullptr);
  CHECK(found == &capability);
  CHECK(found->value == 7);
  CHECK(caors::meta::capabilities::contains<runtime_capability>(id));

  const caors::meta::type type = caors::meta::resolve<runtime_capability_type>();
  CHECK(type.get_capability<runtime_capability>() == &capability);
  CHECK(type.has_capability<runtime_capability>());
}

TEST_CASE("meta - type builder stores owned runtime concept table", "[meta]") {
  matrix_ops local_capability{
      &matrix_rows,
      &matrix_columns,
      &matrix_get,
      &matrix_set,
  };

  REQUIRE(caors::meta::class_<owned_matrix>().capability(local_capability).succeeded());

  const caors::type_id id = caors::meta::id_of<owned_matrix>();
  const matrix_ops* found = caors::meta::capabilities::find<matrix_ops>(id);
  REQUIRE(found != nullptr);
  CHECK(found != &local_capability);
  CHECK(found->rows != nullptr);
  CHECK(found->columns != nullptr);
  CHECK(found->get != nullptr);
  CHECK(found->set != nullptr);

  const caors::meta::type type = caors::meta::resolve<owned_matrix>();
  CHECK(type.get_capability<matrix_ops>() == found);
  const matrix_capability capability = type.get_capability<matrix_capability>();
  REQUIRE(capability);

  owned_matrix matrix{};
  caors::meta::any_ref matrix_ref = caors::meta::any_ref::from(matrix);
  const matrix_view view = capability.bind(matrix_ref);
  REQUIRE(view);
  view.set(1, 0, 17);
  CHECK(found->rows(&matrix) == 2);
  CHECK(found->columns(&matrix) == 2);
  CHECK(view.rows() == 2);
  CHECK(view.columns() == 2);
  CHECK(found->get(&matrix, 1, 0) == 17);
  CHECK(view.get(1, 0) == 17);

  const matrix_view direct_view = matrix_ref.get_capability<matrix_view>();
  REQUIRE(direct_view);
  CHECK(direct_view.get(1, 0) == 17);

  const matrix_view capability_view = matrix_ref.get_capability<matrix_capability>();
  REQUIRE(capability_view);
  CHECK(capability_view.get(1, 0) == 17);

  int wrong_value = 0;
  CHECK_FALSE(capability.bind(caors::meta::any_ref::from(wrong_value)));
}

TEST_CASE("meta - type capability base auto exposes custom type facade", "[meta]") {
  creator_ops local_ops{&creator_value};

  REQUIRE(caors::meta::class_<creator_capability_type>().capability(local_ops).succeeded());

  const caors::meta::type type = caors::meta::resolve<creator_capability_type>();
  const creator_capability capability = type.get_capability<creator_capability>();

  REQUIRE(capability);
  CHECK(capability.get_type().id() == type.id());
  CHECK(capability.value() == 31);
  CHECK(type.has_capability<creator_capability>());
}

TEST_CASE("meta - proxy capability macro generates ops view and auto model", "[meta]") {
  REQUIRE(
      caors::meta::class_<macro_proxy_matrix>().capability<test_matrix_capability>().succeeded());

  const test_matrix_capability capability =
      caors::meta::resolve<macro_proxy_matrix>().get_capability<test_matrix_capability>();
  REQUIRE(capability);

  macro_proxy_matrix matrix{};
  const test_matrix_capability_view view =
      caors::meta::any_ref::from(matrix).get_capability<test_matrix_capability>();
  REQUIRE(view);

  view.set(1, 0, 61);
  CHECK(view.rows() == 2);
  CHECK(view.columns() == 2);
  CHECK(view.get(1, 0) == 61);
  CHECK(matrix.values[2] == 61);
}

TEST_CASE("meta - proxy capability builder generates convention facade", "[meta]") {
  REQUIRE(caors::meta::class_<builder_proxy_matrix>()
              .capability<builder_matrix_capability>()
              .succeeded());

  const caors::meta::type type = caors::meta::resolve<builder_proxy_matrix>();
  const builder_matrix_capability capability = type.get_capability<builder_matrix_capability>();
  REQUIRE(capability);
  const builder_proxy_matrix created = capability.create();
  CHECK(created.values[0] == 5);
  const builder_proxy_matrix made = capability.make();
  CHECK(made.values[1] == 9);

  builder_proxy_matrix matrix{};
  auto view = caors::meta::any_ref::from(matrix).get_capability<builder_matrix_capability>();
  REQUIRE(view);

  view.set(1, 1, 73);
  CHECK(view.rows() == 2);
  CHECK(view.columns() == 2);
  CHECK(view.get(1, 1) == 73);
  CHECK(matrix.values[3] == 73);
}

TEST_CASE("meta - proxy type capability macro generates type facade", "[meta]") {
  test_creator_capability_ops ops{&creator_value};

  REQUIRE(caors::meta::class_<creator_capability_type>().capability(ops).succeeded());

  const test_creator_capability capability =
      caors::meta::resolve<creator_capability_type>().get_capability<test_creator_capability>();
  REQUIRE(capability);
  CHECK(capability.value() == 31);
}

TEST_CASE("meta - type capability facade binds built in value views", "[meta]") {
  std::vector<int> values{1, 2, 3};
  const caors::meta::type sequence_type = caors::meta::resolve<std::vector<int>>();

  const caors::meta::sequence_capability sequence =
      sequence_type.get_capability<caors::meta::sequence_capability>();
  REQUIRE(sequence);
  CHECK(sequence_type.has_capability<caors::meta::sequence_capability>());
  CHECK(sequence.get_value_type().id() == caors::meta::resolve<int>().id());

  const caors::meta::sequence_view view = sequence.bind(caors::meta::any_ref::from(values));
  REQUIRE(view);
  REQUIRE(view.size().has_value());
  CHECK(*view.size() == 3);

  const caors::meta::sequence_view direct_view =
      caors::meta::any_ref::from(values).get_capability<caors::meta::sequence_capability>();
  REQUIRE(direct_view);
  REQUIRE(direct_view.size().has_value());
  CHECK(*direct_view.size() == 3);

  int wrong_value = 0;
  CHECK_FALSE(sequence.bind(caors::meta::any_ref::from(wrong_value)));
  CHECK_FALSE(caors::meta::resolve<int>().has_capability<caors::meta::sequence_capability>());
}

TEST_CASE("meta - enum type capability facade converts bound enum values", "[meta]") {
  facade_enum value = facade_enum::high;
  const caors::meta::type type = caors::meta::resolve<facade_enum>();

  const caors::meta::enum_capability capability =
      type.get_capability<caors::meta::enum_capability>();
  REQUIRE(capability);
  CHECK(capability.is_signed());

  caors::meta::any_ref ref = caors::meta::any_ref::from(value);
  const std::optional<std::int64_t> encoded = capability.to_i64(ref);
  REQUIRE(encoded.has_value());
  CHECK(*encoded == 9);

  CHECK(capability.from_i64(ref, -1));
  CHECK(value == facade_enum::low);
}

TEST_CASE("meta - capability static provider can be registered into runtime index", "[meta]") {
  const static_capability_data* capability =
      caors::meta::capabilities::get<static_capability_data, static_capability_type>();
  REQUIRE(capability != nullptr);
  CHECK(capability->value == 42);

  REQUIRE(caors::meta::class_<static_capability_type>()
              .capability<static_capability_data>()
              .succeeded());

  const static_capability_data* found = caors::meta::capabilities::find<static_capability_data>(
      caors::meta::id_of<static_capability_type>());
  REQUIRE(found != nullptr);
  CHECK(found == capability);
}

TEST_CASE("meta - type view resolves descriptor static capabilities", "[meta]") {
  const caors::meta::type type = caors::meta::resolve<static_capability_type>();
  const static_capability_data* found = type.get_capability<static_capability_data>();
  REQUIRE(found != nullptr);
  CHECK(found->value == 42);
  CHECK(type.has_capability<static_capability_data>());
}

TEST_CASE("meta - static capability modules can be extended by module tag list", "[meta]") {
  const caors::meta::type type = caors::meta::resolve<extension_capability_type>();

  const first_extension_capability* first = type.get_capability<first_extension_capability>();
  REQUIRE(first != nullptr);
  CHECK(first->value == 11);

  const second_extension_capability* second = type.get_capability<second_extension_capability>();
  REQUIRE(second != nullptr);
  CHECK(second->value == 22);
}
