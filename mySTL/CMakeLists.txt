cmake_minimum_required(VERSION 3.31)
project(mySTL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

include(CTest)
if(BUILD_TESTING)
    add_executable(mystl_regression test/regression_test.cpp)
    add_test(NAME mystl_regression COMMAND mystl_regression)

    add_executable(mystl_algorithm_regression test/algorithm_regression.cpp)
    add_test(NAME mystl_algorithm_regression COMMAND mystl_algorithm_regression)

    # Compile each header in a separate translation unit so include order cannot hide dependencies.
    # vector remains under development and is intentionally excluded from this check.
    file(GLOB_RECURSE MYSTL_HEADERS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "src/*.h")
    list(REMOVE_ITEM MYSTL_HEADERS "src/vector.h")
    set(MYSTL_HEADER_CHECK_SOURCES)
    foreach(header IN LISTS MYSTL_HEADERS)
        string(MAKE_C_IDENTIFIER "${header}" header_id)
        set(check_source "${CMAKE_CURRENT_BINARY_DIR}/header_checks/${header_id}.cpp")
        file(GENERATE OUTPUT "${check_source}" CONTENT "#include \"${header}\"\n")
        list(APPEND MYSTL_HEADER_CHECK_SOURCES "${check_source}")
    endforeach()
    add_library(mystl_header_checks OBJECT ${MYSTL_HEADER_CHECK_SOURCES})
    target_include_directories(mystl_header_checks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    option(MYSTL_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers for tests" OFF)
    if(MYSTL_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        foreach(target mystl_regression mystl_algorithm_regression)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
endif()
