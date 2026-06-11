cmake_minimum_required(VERSION 3.20)
project(poker_equity CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:Release>:-march=native>)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(${CMAKE_SOURCE_DIR}/include)

enable_testing()

add_executable(cards_test test/cards_test.cpp)
target_link_libraries(cards_test PRIVATE gtest_main)
add_test(NAME cards_test COMMAND cards_test)

add_executable(prng_test test/prng_test.cpp)
target_link_libraries(prng_test PRIVATE gtest_main)
add_test(NAME prng_test COMMAND prng_test)
