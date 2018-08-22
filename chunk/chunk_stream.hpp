#pragma once
#include <type_traits>

#include "../config.hpp"

struct chunk_stream_cpp {};
struct chunk_stream_stdio {};
struct chunk_stream_mmap {};

template<typename T, typename Type>
class _chunk_istream;

template<typename T, typename Type>
class _chunk_ostream;

constexpr bool _use_cpp_streams = IS_ENABLED(CONFIG_USE_CPP_STREAMS);

using cpp_stdio_stream_type = std::conditional<_use_cpp_streams, 
                                               chunk_stream_cpp, 
                                               chunk_stream_stdio>::type;

using chunk_stream_type = std::conditional<IS_ENABLED(CONFIG_USE_MMAP),
                                           chunk_stream_mmap, 
                                           cpp_stdio_stream_type>::type;

template<typename T>
using chunk_istream = _chunk_istream<T, chunk_stream_type>;

template<typename T>
using chunk_ostream = _chunk_ostream<T, chunk_stream_type>;
