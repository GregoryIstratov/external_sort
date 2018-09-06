cmake_minimum_required(VERSION 3.10)
project(external_sort)

set(CMAKE_CXX_STANDARD 14)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Og")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
    set(Boost_USE_STATIC_LIBS        ON) # only find static libs
    set(Boost_USE_MULTITHREADED      ON)
    set(Boost_USE_STATIC_RUNTIME    OFF)
endif()

#set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#set(Boost_DEBUG On)
find_package(Boost COMPONENTS iostreams)

if(Boost_FOUND)
    #add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})
    add_definitions(-D__BOOST_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})  
endif()

add_executable(external_sort main.cpp 
                             task_tree.hpp 
                             config.hpp 
                             task.hpp
                             log.cpp 
                             log.hpp 
                             pipeline/memory_management_unit.hpp
                             pipeline/merging_unit.hpp
                             pipeline/pipeline.hpp 
                             pipeline/pipeline_controller.hpp
                             pipeline/sorting_unit.hpp
                             pipeline/task_management_unit.hpp
                             pipeline/thread_management_unit.hpp
                             extra/crc64.cpp
                             extra/crc64.hpp
                             extra/hasher.hpp
                             extra/sort.hpp
                             tools/exception.cpp 
                             tools/exception.hpp
                             tools/barrier.hpp
                             tools/file.cpp
                             tools/file.hpp
                             tools/format.hpp
                             tools/literals.hpp
                             tools/mapped_file.hpp
                             tools/mapped_file.cpp
                             tools/perf_timer.hpp
                             tools/sequential_mutex.hpp
                             tools/span.hpp
                             tools/spinlock.hpp
                             tools/unique_guard.hpp
                             tools/util.hpp
                             chunk/chunk_id.hpp
                             chunk/chunk_istream.hpp
                             chunk/chunk_ostream.hpp
                             chunk/chunk_stream.hpp
                             )

target_link_libraries(external_sort Threads::Threads)

if(Boost_FOUND)
target_link_libraries(external_sort ${Boost_LIBRARIES})
target_link_libraries(external_sort dl)
endif()
