cmake_minimum_required(VERSION 3.31)
project(mySTL)

set(CMAKE_CXX_STANDARD 26)

add_executable(mySTL main.cpp
        src/impl/type_traits_normal.h
        src/utility.h
        src/iterator.h
        src/impl/algobase.h
        src/cstring.h
        src/numeric.h
        src/functional.h
        test/type_traits_test.h
        src/impl/construct.h
        src/impl/allocator.h
        src/impl/uninitialized.h
        src/memory.h
        src/bitset.h
        src/impl/basic_string.h
        src/exception.h
        src/stdexcept.h
        src/impl/functions.h
        src/impl/invoke_traits.h
        src/type_traits.h
        src/impl/wrappers.h
        src/algorithm.h
        src/impl/algo_partition.h
        src/impl/algo_ordered.h
        src/impl/heap_algo.h
        test/algorithm_test.h
        src/cmath.h
        src/impl/temporary_buffer.h
        src/vector.h
        src/initializer_list.h
        src/impl/pointer_traits.h)
