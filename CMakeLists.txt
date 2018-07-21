cmake_minimum_required(VERSION 3.10)
project(external_sort)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Og")

add_executable(external_sort main.cpp chunk.hpp util.cpp util.hpp task_tree.hpp settings.hpp task.hpp pipeline.hpp log.hpp)
target_link_libraries(external_sort pthread)