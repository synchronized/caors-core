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

# Find Catch2
if(NOT TARGET Catch2::Catch2)
    set(CATCH_INCLUDE_DIR ${THIRD_PARTY_ROOT}/catch-v3.15.0)
    add_library(Catch2 INTERFACE)
    add_library(Catch2::Catch2 ALIAS Catch2)
    target_include_directories(Catch2 INTERFACE ${CATCH_INCLUDE_DIR})
    target_compile_features(Catch2 INTERFACE cxx_std_20)
    set_target_properties(Catch2 PROPERTIES FOLDER "${third_party_folder}/Catch2")

    add_library(Catch2WithMain STATIC ${CATCH_INCLUDE_DIR}/catch2/catch_amalgamated.cpp)
    add_library(Catch2::Catch2WithMain ALIAS Catch2WithMain)
    target_compile_features(Catch2WithMain PUBLIC cxx_std_20)
    target_link_libraries(Catch2WithMain PUBLIC Catch2::Catch2)
    set_target_properties(Catch2WithMain PROPERTIES FOLDER "${third_party_folder}/Catch2")
endif()

MESSAGE(STATUS "Finished finding 3rd party libs!")
