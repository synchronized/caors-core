# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

add_executable(caors_benchmarks)

target_sources(
  caors_benchmarks
  PRIVATE main.cpp
          support/benchmark_support.cpp
          core/string_name_bench.cpp
          core/small_vector_bench.cpp
          meta/type_bench.cpp
          meta/property_bench.cpp
          meta/method_bench.cpp
          meta/wrapper_samples.cpp
          meta/value_bench.cpp
          math/math_bench.cpp
          serialization/serialization_bench.cpp
          resources/resources_bench.cpp
          memory/pool_bench.cpp
          memory/allocation_samples.cpp)
target_compile_features(caors_benchmarks PRIVATE cxx_std_20)
target_compile_warnings(caors_benchmarks)
target_include_directories(caors_benchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(caors_benchmarks PRIVATE caors::caors ctrack::ctrack)

set_target_properties(caors_benchmarks PROPERTIES FOLDER "Benchmarks")
