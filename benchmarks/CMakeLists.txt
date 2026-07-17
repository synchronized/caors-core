# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

add_executable(caors_benchmarks)

target_sources(caors_benchmarks PRIVATE caors_benchmarks.cpp)
target_compile_features(caors_benchmarks PRIVATE cxx_std_20)
target_compile_warnings(caors_benchmarks)
target_link_libraries(caors_benchmarks PRIVATE caors::caors ctrack::ctrack)

set_target_properties(caors_benchmarks PROPERTIES FOLDER "Benchmarks")
