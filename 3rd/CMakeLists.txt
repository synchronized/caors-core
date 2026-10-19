# SPDX-License-Identifier: MIT

####################################################################################
#                                                                                  #
#  Copyright (c) 2014 - 2018 Axel Menzel <info@rttr.org>                           #
#                                                                                  #
#  This file is part of RTTR (Run Time Type Reflection)                            #
#  License: MIT License                                                            #
#                                                                                  #
#  Permission is hereby granted, free of charge, to any person obtaining           #
#  a copy of this software and associated documentation files (the "Software"),    #
#  to deal in the Software without restriction, including without limitation       #
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,        #
#  and/or sell copies of the Software, and to permit persons to whom the           #
#  Software is furnished to do so, subject to the following conditions:            #
#                                                                                  #
#  The above copyright notice and this permission notice shall be included in      #
#  all copies or substantial portions of the Software.                             #
#                                                                                  #
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      #
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,        #
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     #
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          #
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   #
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   #
#  SOFTWARE.                                                                       #
#                                                                                  #
####################################################################################

####################################################################################
# Search and install 3rd party libraries
#
####################################################################################

MESSAGE(STATUS ${LIBRARY_OUTPUT_DIRECTORY})
MESSAGE(STATUS "Finding 3rd party libs...")
MESSAGE(STATUS "===========================")

set(third_party_folder "ThirdParty")
set(THIRD_PARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# standalone Asio 仅作为 CAORS 私有实现依赖。集中配置可以避免不同目标使用不一致的宏。
if(NOT TARGET caors_asio_config)
    add_library(caors_asio_config INTERFACE)
    target_include_directories(
        caors_asio_config
        SYSTEM INTERFACE
            "${THIRD_PARTY_ROOT}/asio-1.38.2/include"
    )
    target_compile_features(caors_asio_config INTERFACE cxx_std_20)
    target_compile_definitions(
        caors_asio_config
        INTERFACE
            ASIO_STANDALONE
            ASIO_NO_DEPRECATED
            # 禁止 Asio 头主动恢复 default visibility，保持 CAORS 的私有符号边界。
            ASIO_DISABLE_VISIBILITY
            $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00>
            $<$<PLATFORM_ID:Windows>:WINVER=0x0A00>
    )
    set_target_properties(caors_asio_config PROPERTIES FOLDER "${third_party_folder}/Asio")
endif()

# 只在使用方启用时配置测试和性能依赖。
if(CAORS_BUILD_TESTING AND BUILD_TESTING)
    if(TARGET Catch2::Catch2)
        if(NOT TARGET Catch2::Catch2WithMain)
            message(FATAL_ERROR "已有 Catch2::Catch2，但缺少 Catch2::Catch2WithMain；请提供完整 Catch2 targets")
        endif()
        if(NOT DEFINED Catch2_VERSION)
            find_package(Catch2 3.15.0 EXACT CONFIG QUIET)
        endif()
        if(NOT DEFINED Catch2_VERSION OR NOT Catch2_VERSION VERSION_EQUAL "3.15.0")
            message(FATAL_ERROR "复用外部 Catch2 需要已验证版本 3.15.0；请通过 Catch2_VERSION 提供实际版本")
        endif()
    else()
        if(TARGET Catch2::Catch2WithMain)
            message(FATAL_ERROR "已有 Catch2WithMain 但缺少 Catch2::Catch2")
        endif()
        set(CATCH_INCLUDE_DIR ${THIRD_PARTY_ROOT}/catch-v3.15.0)
        add_library(caors_catch INTERFACE)
        add_library(Catch2::Catch2 ALIAS caors_catch)
        target_include_directories(caors_catch INTERFACE ${CATCH_INCLUDE_DIR})
        add_library(caors_catch_main STATIC ${CATCH_INCLUDE_DIR}/catch2/catch_amalgamated.cpp)
        add_library(Catch2::Catch2WithMain ALIAS caors_catch_main)
        target_compile_features(caors_catch_main PUBLIC cxx_std_20)
        target_link_libraries(caors_catch_main PUBLIC Catch2::Catch2)
    endif()
endif()

if(CAORS_BUILD_BENCHMARKS)
    if(TARGET ctrack::ctrack)
        if(NOT DEFINED ctrack_VERSION OR NOT ctrack_VERSION VERSION_EQUAL "1.1.0")
            message(FATAL_ERROR "复用外部 ctrack 需要已验证版本 1.1.0；请设置 ctrack_VERSION")
        endif()
    else()
        add_library(caors_ctrack INTERFACE)
        add_library(ctrack::ctrack ALIAS caors_ctrack)
        target_include_directories(caors_ctrack INTERFACE ${THIRD_PARTY_ROOT}/ctrack-1.1.0/include)
        target_compile_features(caors_ctrack INTERFACE cxx_std_20)
        target_compile_definitions(caors_ctrack INTERFACE CTRACK_DISABLE_EXECUTION_POLICY)
    endif()
endif()

MESSAGE(STATUS "Finished finding 3rd party libs!")
