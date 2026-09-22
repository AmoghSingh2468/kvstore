cmake_minimum_required(VERSION 3.16)
project(kvstore CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -g -O2)

option(ENABLE_ASAN "AddressSanitizer + UBSan" OFF)
option(ENABLE_TSAN "ThreadSanitizer" OFF)

if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()

add_library(kvcore
  src/protocol.cpp src/store.cpp
  src/command.cpp  src/connection.cpp
  src/wal.cpp)
target_include_directories(kvcore PUBLIC src)

find_package(Threads REQUIRED)
target_link_libraries(kvcore PUBLIC Threads::Threads)

add_executable(kvserver src/main.cpp)
target_link_libraries(kvserver PRIVATE kvcore)

add_executable(test_protocol tests/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE kvcore)

add_executable(store_bench bench/store_bench.cpp)
target_link_libraries(store_bench PRIVATE kvcore)