# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.23)
project(caors_web_consumer LANGUAGES CXX)
enable_testing()
find_package(caors CONFIG REQUIRED)
add_executable(caors_web_consumer main.cpp)
target_link_libraries(caors_web_consumer PRIVATE caors::caors)
add_test(NAME consumer.web COMMAND caors_web_consumer)
