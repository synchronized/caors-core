// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;
TEST_CASE("reflection - sequence field view exposes element metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<vector_holder>("caors.test.reflection.vector_holder")
              .field("values", &vector_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.vector_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::sequence);
  CHECK(values->is_sequence());
  CHECK(values->value_type() == registered_type_id("int"));
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  vector_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::value::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.value_type() == registered_type_id("int"));
  CHECK(sequence.value_type_name() == "int");
  CHECK(sequence.element_type() == registered_type_id("int"));
  CHECK(sequence.element_type_name() == "int");

  int sequence_sum = 0;
  for (caors::any_ref value : sequence) {
    const int* typed = value.try_get<int>();
    REQUIRE(typed != nullptr);
    sequence_sum += *typed;
  }
  CHECK(sequence_sum == 8);

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  const vector_holder& readonly_instance = instance;
  caors::value::sequence_view readonly_sequence =
      values->get_ref(&readonly_instance).create_sequence_view();
  CHECK(readonly_sequence.valid());
  CHECK_FALSE(readonly_sequence.writable());

  caors::any_ref first = readonly_sequence.element_ref(0);
  REQUIRE(first.valid());
  CHECK_FALSE(first.writable());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.writable());
  CHECK(second.try_set<int>(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  REQUIRE(instance.values.size() == 3);
  CHECK(instance.values[2] == 13);

  CHECK(sequence.resize(4));
  REQUIRE(instance.values.size() == 4);
  CHECK(instance.values[3] == 0);

  CHECK(sequence.clear());
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - custom sequence traits expose field view", "[reflection]") {
  REQUIRE(caors::reflection::class_<custom_sequence_holder>(
              "caors.test.reflection.custom_sequence_holder")
              .field("values", &custom_sequence_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.custom_sequence_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::sequence);
  CHECK(values->type_name() == "test.custom_sequence<int>");
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  custom_sequence_holder instance{};
  caors::value::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  CHECK(sequence.append_value(caors::any::from(21)));
  CHECK(sequence.append_value(caors::any::from(34)));

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  caors::any_ref first = sequence.element_ref(0);
  REQUIRE(first.valid());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 21);

  CHECK(sequence.resize(1));
  CHECK(instance.values.values.size() == 1);
  CHECK(sequence.clear());
  CHECK(instance.values.values.empty());
}

TEST_CASE("reflection - std array field view uses fixed sequence traits", "[reflection]") {
  REQUIRE(caors::reflection::class_<array_holder>("caors.test.reflection.array_holder")
              .field("values", &array_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.array_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::sequence);
  CHECK(values->type_name() == "std.array<int,3>");
  CHECK(values->element_type() == registered_type_id("int"));

  array_holder instance{{1, 2, 3}};
  caors::value::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
  CHECK_FALSE(sequence.append_value(caors::any::from(13)));
}

TEST_CASE("reflection - std span field view is borrowed sequence", "[reflection]") {
  REQUIRE(caors::reflection::class_<span_holder>("caors.test.reflection.span_holder")
              .field("values", &span_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.span_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::sequence);
  CHECK(values->type_name() == "std.span<int>");
  CHECK(values->element_type() == registered_type_id("int"));

  std::array<int, 3> storage{1, 2, 3};
  span_holder instance{std::span<int>{storage}};
  caors::value::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  REQUIRE(sequence.valid());
  CHECK(sequence.writable());

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));
  CHECK(storage[1] == 8);

  CHECK(sequence.resize(3));
  CHECK_FALSE(sequence.resize(2));
  CHECK_FALSE(sequence.append_value(caors::any::from(13)));
}

TEST_CASE("reflection - fixed std span field name includes extent", "[reflection]") {
  REQUIRE(caors::reflection::class_<fixed_span_holder>("caors.test.reflection.fixed_span_holder")
              .field("values", &fixed_span_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.fixed_span_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::sequence);
  CHECK(values->type_name() == "std.span<int,3>");
  CHECK(values->element_type() == registered_type_id("int"));
}

TEST_CASE("reflection - std pair field view exposes first and second values", "[reflection]") {
  REQUIRE(caors::reflection::class_<pair_holder>("caors.test.reflection.pair_holder")
              .field("value", &pair_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.pair_holder");
  REQUIRE(reflected.has_value());

  auto value = reflected->find_field("value");
  REQUIRE(value.has_value());
  CHECK(value->kind() == caors::meta::type_kind::pair);
  CHECK(value->is_pair());
  CHECK(value->type_name() == "std.pair<int,caors.string>");
  CHECK(value->first_type() == registered_type_id("int"));
  CHECK(value->first_type_name() == "int");
  CHECK(value->second_type() == registered_type_id("caors.string"));
  CHECK(value->second_type_name() == "caors.string");
  CHECK(value->value_type() == registered_type_id("caors.string"));

  pair_holder instance{{7, caors::string{"seven"}}};
  caors::value::pair_view pair = value->get_ref(&instance).create_pair_view();
  REQUIRE(pair.valid());
  CHECK(pair.writable());
  CHECK(pair.first_type() == registered_type_id("int"));
  CHECK(pair.second_type() == registered_type_id("caors.string"));

  caors::any_ref first = pair.first_ref();
  REQUIRE(first.valid());
  CHECK(first.try_set(9));

  caors::any_ref second = pair.second_ref();
  REQUIRE(second.valid());
  CHECK(second.try_set(caors::string{"nine"}));

  CHECK(instance.value.first == 9);
  CHECK(instance.value.second == "nine");
}

TEST_CASE("reflection - std tuple field view exposes indexed elements", "[reflection]") {
  REQUIRE(caors::reflection::class_<tuple_holder>("caors.test.reflection.tuple_holder")
              .field("value", &tuple_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.tuple_holder");
  REQUIRE(reflected.has_value());

  auto value = reflected->find_field("value");
  REQUIRE(value.has_value());
  CHECK(value->kind() == caors::meta::type_kind::tuple);
  CHECK(value->is_tuple());
  CHECK(value->type_name() == "std.tuple<int,caors.string,float>");

  tuple_holder instance{std::tuple<int, caors::string, float>{7, caors::string{"seven"}, 1.5F}};
  caors::value::tuple_view tuple = value->get_ref(&instance).create_tuple_view();
  REQUIRE(tuple.valid());
  CHECK(tuple.writable());
  CHECK(tuple.size() == 3);

  REQUIRE(tuple.element_type(0).has_value());
  REQUIRE(tuple.element_type(1).has_value());
  REQUIRE(tuple.element_type(2).has_value());
  CHECK(*tuple.element_type(0) == registered_type_id("int"));
  CHECK(*tuple.element_type(1) == registered_type_id("caors.string"));
  CHECK(*tuple.element_type(2) == registered_type_id("float"));
  CHECK(tuple.element_type_name(1) == "caors.string");

  caors::any_ref first = tuple.element_ref(0);
  caors::any_ref second = tuple.element_ref(1);
  caors::any_ref third = tuple.element_ref(2);
  REQUIRE(first.valid());
  REQUIRE(second.valid());
  REQUIRE(third.valid());

  CHECK(first.try_set(9));
  CHECK(second.try_set(caors::string{"nine"}));
  CHECK(third.try_set(2.5F));

  CHECK(std::get<0>(instance.value) == 9);
  CHECK(std::get<1>(instance.value) == "nine");
  CHECK(std::get<2>(instance.value) == 2.5F);
}

TEST_CASE("reflection - std variant field view exposes active alternative", "[reflection]") {
  REQUIRE(caors::reflection::class_<variant_holder>("caors.test.reflection.variant_holder")
              .field("value", &variant_holder::value)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.variant_holder");
  REQUIRE(reflected.has_value());

  auto value = reflected->find_field("value");
  REQUIRE(value.has_value());
  CHECK(value->kind() == caors::meta::type_kind::variant);
  CHECK(value->is_variant());
  CHECK(value->type_name() == "std.variant<int,caors.string,float>");

  variant_holder instance{std::variant<int, caors::string, float>{7}};
  caors::value::variant_view variant = value->get_ref(&instance).create_variant_view();
  REQUIRE(variant.valid());
  CHECK(variant.writable());
  CHECK(variant.size() == 3);

  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 0);
  REQUIRE(variant.alternative_type(0).has_value());
  REQUIRE(variant.alternative_type(1).has_value());
  REQUIRE(variant.alternative_type(2).has_value());
  CHECK(*variant.alternative_type(0) == registered_type_id("int"));
  CHECK(*variant.alternative_type(1) == registered_type_id("caors.string"));
  CHECK(*variant.alternative_type(2) == registered_type_id("float"));
  CHECK(variant.alternative_type_name(1) == "caors.string");
  CHECK(variant.value_type() == registered_type_id("int"));

  caors::any_ref active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(9));
  CHECK(std::get<int>(instance.value) == 9);

  REQUIRE(variant.emplace(1));
  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 1);
  CHECK(variant.value_type_name() == "caors.string");

  active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(caors::string{"nine"}));
  CHECK(std::get<caors::string>(instance.value) == "nine");
}

TEST_CASE("reflection - list field view uses sequence traits", "[reflection]") {
  REQUIRE(caors::reflection::class_<list_holder>("caors.test.reflection.list_holder")
              .field("values", &list_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.list_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::sequence);
  CHECK(values->type_name() == "caors.list<int>");
  CHECK(values->element_type() == registered_type_id("int"));

  list_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::value::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));

  auto iterator = instance.values.begin();
  std::advance(iterator, 1);
  CHECK(*iterator == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  CHECK(instance.values.back() == 13);
}

TEST_CASE("reflection - associative field view exposes key and value metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<hash_map_holder>("caors.test.reflection.hash_map_holder")
              .field("values", &hash_map_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.hash_map_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::associative);
  CHECK(values->is_associative());
  CHECK(values->type_name() == "caors.hash_map<int,int>");
  CHECK(values->key_type() == registered_type_id("int"));
  CHECK(values->key_type_name() == "int");
  CHECK(values->value_type() == registered_type_id("int"));
  CHECK(values->mapped_type() == registered_type_id("int"));
  CHECK(values->mapped_type_name() == "int");
}

TEST_CASE("reflection - associative field view supports dynamic operations", "[reflection]") {
  REQUIRE(
      caors::reflection::class_<hash_map_ops_holder>("caors.test.reflection.hash_map_ops_holder")
          .field("values", &hash_map_ops_holder::values)
          .commit()
          .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.hash_map_ops_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());

  hash_map_ops_holder instance{};
  caors::value::associative_view map = values->get_ref(&instance).create_associative_view();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.key_type() == registered_type_id("int"));
  CHECK(map.key_type_name() == "int");
  CHECK(map.value_type() == registered_type_id("int"));
  CHECK(map.value_type_name() == "int");
  CHECK(map.mapped_type() == registered_type_id("int"));
  CHECK(map.mapped_type_name() == "int");

  CHECK(map.insert_value(caors::any::from(7), caors::any::from(70)));
  CHECK_FALSE(map.insert_value(caors::any::from(7), caors::any::from(700)));
  CHECK_FALSE(map.insert_value(caors::any::from(8.0F), caors::any::from(80)));

  int key_sum = 0;
  int value_sum = 0;
  for (caors::value::associative_entry_ref entry : map) {
    const int* key = entry.key().try_get<int>();
    const int* value = entry.value().try_get<int>();
    REQUIRE(key != nullptr);
    REQUIRE(value != nullptr);
    key_sum += *key;
    value_sum += *value;
  }
  CHECK(key_sum == 7);
  CHECK(value_sum == 70);

  const std::optional<std::size_t> size = map.size();
  REQUIRE(size.has_value());
  CHECK(*size == 1);

  const hash_map_ops_holder& readonly_instance = instance;
  caors::value::associative_view readonly_map =
      values->get_ref(&readonly_instance).create_associative_view();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(8), caors::any::from(80)));
  CHECK_FALSE(readonly_map.clear());

  struct visit_state {
    int key = 0;
    int mapped = 0;
    int count = 0;
  };

  visit_state state{};
  CHECK(readonly_map.for_each_ref(
      [](caors::any_ref key, caors::any_ref mapped, void* user_data) {
        auto* state = static_cast<visit_state*>(user_data);
        const int* key_value = key.try_get<int>();
        const int* mapped_value = mapped.try_get<int>();
        if (key_value == nullptr || mapped_value == nullptr) {
          return false;
        }

        state->key = *key_value;
        state->mapped = *mapped_value;
        ++state->count;
        return true;
      },
      &state));

  CHECK(state.count == 1);
  CHECK(state.key == 7);
  CHECK(state.mapped == 70);

  CHECK(map.clear());
  const std::optional<std::size_t> empty_size = map.size();
  REQUIRE(empty_size.has_value());
  CHECK(*empty_size == 0);
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - container views can be created from any_ref", "[reflection]") {
  require_registered_type<caors::vector<int>>("caors.vector<int>");
  require_registered_type<caors::hash_set<int>>("caors.hash_set<int>");
  require_registered_type<caors::hash_map<int, int>>("caors.hash_map<int,int>");

  caors::vector<int> scores{};
  caors::any_ref scores_ref = caors::any_ref::from(scores, "caors.vector<int>");
  REQUIRE(scores_ref.valid());

  caors::value::sequence_view sequence = scores_ref.create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.append_value(caors::any::from(10)));
  CHECK(sequence.append_value(caors::any::from(20)));
  CHECK(scores.size() == 2);
  caors::any_ref second_score = sequence.element_ref(1);
  REQUIRE(second_score.valid());
  CHECK(second_score.try_set(25));
  CHECK(scores[1] == 25);

  caors::hash_set<int> ids{};
  caors::any_ref ids_ref = caors::any_ref::from(ids, "caors.hash_set<int>");
  REQUIRE(ids_ref.valid());

  caors::value::set_view set = ids_ref.create_set_view();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.insert_value(caors::any::from(3)));
  CHECK(ids.contains(3));

  caors::hash_map<int, int> values{};
  caors::any_ref ref = caors::any_ref::from(values, "caors.hash_map<int,int>");
  REQUIRE(ref.valid());

  caors::value::associative_view map = ref.create_associative_view();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.insert_value(caors::any::from(3), caors::any::from(30)));
  CHECK(values.at(3) == 30);

  const caors::any_ref readonly_ref = caors::any_ref::from_const(values, "caors.hash_map<int,int>");
  caors::value::associative_view readonly_map = readonly_ref.create_associative_view();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(4), caors::any::from(40)));

  caors::any copied = caors::any::from(values, "caors.hash_map<int,int>");
  caors::value::associative_view copied_map = copied.create_associative_view();
  CHECK(copied_map.valid());
  CHECK(copied_map.writable());
  CHECK(copied_map.insert_value(caors::any::from(5), caors::any::from(50)));

  const std::optional<std::size_t> copied_size = copied_map.size();
  REQUIRE(copied_size.has_value());
  CHECK(*copied_size == 2);
}

TEST_CASE("reflection - set field view exposes element metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<hash_set_holder>("caors.test.reflection.hash_set_holder")
              .field("values", &hash_set_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.hash_set_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::meta::type_kind::set);
  CHECK(values->is_set());
  CHECK(values->type_name() == "caors.hash_set<int>");
  CHECK(values->value_type() == registered_type_id("int"));
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  hash_set_holder instance{};
  caors::value::set_view set = values->get_ref(&instance).create_set_view();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.value_type() == registered_type_id("int"));
  CHECK(set.value_type_name() == "int");
  CHECK(set.element_type() == registered_type_id("int"));
  CHECK(set.element_type_name() == "int");
  CHECK(set.insert_value(caors::any::from(11)));
  CHECK_FALSE(set.insert_value(caors::any::from(11)));
  CHECK_FALSE(set.insert_value(caors::any::from(2.0F)));

  int set_sum = 0;
  for (caors::any_ref value : set) {
    const int* typed = value.try_get<int>();
    REQUIRE(typed != nullptr);
    set_sum += *typed;
  }
  CHECK(set_sum == 11);

  const std::optional<std::size_t> size = set.size();
  REQUIRE(size.has_value());
  CHECK(*size == 1);

  const hash_set_holder& readonly_instance = instance;
  caors::value::set_view readonly_set = values->get_ref(&readonly_instance).create_set_view();
  CHECK(readonly_set.valid());
  CHECK_FALSE(readonly_set.writable());
  CHECK_FALSE(readonly_set.insert_value(caors::any::from(12)));
  CHECK_FALSE(readonly_set.clear());

  int visited = 0;
  CHECK(readonly_set.for_each_ref(
      [](caors::any_ref element, void* user_data) {
        auto* visited = static_cast<int*>(user_data);
        const int* value = element.try_get<int>();
        if (value == nullptr) {
          return false;
        }

        *visited = *value;
        return true;
      },
      &visited));
  CHECK(visited == 11);

  CHECK(set.clear());
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - set iterator creation failure is treated as empty range", "[reflection]") {
  const caors::error begin_type_status = caors::meta::type<test_reflection::failing_begin_set>(
                                             "caors.test.reflection.failing_begin_set")
                                             .result();
  REQUIRE((begin_type_status == caors::error{} ||
           begin_type_status == meta_error(caors::errc::duplicate_id)));

  const caors::error end_type_status =
      caors::meta::type<test_reflection::failing_end_set>("caors.test.reflection.failing_end_set")
          .result();
  REQUIRE((end_type_status == caors::error{} ||
           end_type_status == meta_error(caors::errc::duplicate_id)));

  auto begin_type = caors::meta::find<test_reflection::failing_begin_set>();
  auto end_type = caors::meta::find<test_reflection::failing_end_set>();
  REQUIRE(begin_type.has_value());
  REQUIRE(end_type.has_value());

  const caors::error int_type_status = caors::meta::type<int>("int").result();
  REQUIRE((int_type_status == caors::error{} ||
           int_type_status == meta_error(caors::errc::duplicate_id)));

  caors::meta::set_data failing_begin{};
  failing_begin.value_type = registered_type_id("int");
  failing_begin.size = [](const void*) noexcept -> std::size_t { return 1; };
  failing_begin.visit = [](const void*, caors::meta::set_raw_visitor, void*) -> bool {
    return true;
  };
  failing_begin.begin = [](const void*) noexcept -> void* { return nullptr; };
  failing_begin.end = [](const void*) noexcept -> void* { return new (std::nothrow) int{0}; };
  failing_begin.iterator_clone = [](const void* iterator) noexcept -> void* {
    if (iterator == nullptr) {
      return nullptr;
    }

    return new (std::nothrow) int{*static_cast<const int*>(iterator)};
  };
  failing_begin.iterator_destroy = [](void* iterator) noexcept {
    delete static_cast<int*>(iterator);
  };
  failing_begin.iterator_advance = [](void*) noexcept {};
  failing_begin.iterator_equal = [](const void* lhs, const void* rhs) noexcept -> bool {
    return lhs == rhs;
  };
  failing_begin.iterator_value = [](const void*) noexcept -> const void* {
    static const int value = 7;
    return &value;
  };

  caors::meta::set_data failing_end = failing_begin;
  failing_end.begin = [](const void*) noexcept -> void* { return new (std::nothrow) int{0}; };
  failing_end.end = [](const void*) noexcept -> void* { return nullptr; };

  const caors::error begin_component_status =
      caors::meta::get_registry().emplace<caors::meta::set_data>(begin_type->id(), failing_begin);
  REQUIRE((begin_component_status == caors::error{} ||
           begin_component_status == meta_error(caors::errc::duplicate_component)));

  const caors::error end_component_status =
      caors::meta::get_registry().emplace<caors::meta::set_data>(end_type->id(), failing_end);
  REQUIRE((end_component_status == caors::error{} ||
           end_component_status == meta_error(caors::errc::duplicate_component)));

  test_reflection::failing_begin_set begin_storage{};
  caors::value::set_view begin_view =
      caors::any_ref::from(begin_storage, "caors.test.reflection.failing_begin_set")
          .create_set_view();
  REQUIRE(begin_view.valid());

  int begin_visit_count = 0;
  for (caors::any_ref value : begin_view) {
    (void)value;
    ++begin_visit_count;
  }
  CHECK(begin_visit_count == 0);

  test_reflection::failing_end_set end_storage{};
  caors::value::set_view end_view =
      caors::any_ref::from(end_storage, "caors.test.reflection.failing_end_set").create_set_view();
  REQUIRE(end_view.valid());

  int end_visit_count = 0;
  for (caors::any_ref value : end_view) {
    (void)value;
    ++end_visit_count;
  }
  CHECK(end_visit_count == 0);
}
