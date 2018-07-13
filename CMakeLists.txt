cmake_minimum_required(VERSION 3.10)
project(external_sort)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ")

add_executable(external_sort main.cpp file_io.cpp file_io.hpp util.cpp util.hpp task_tree.hpp chunk_id.hpp settings.hpp task.hpp pipeline.hpp)
target_link_libraries(external_sort pthread)