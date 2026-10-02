cmake_minimum_required(VERSION 3.31)
project(cpp_learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_subdirectory(mySTL)
add_subdirectory(mini_algorithm)
add_subdirectory("simple graphic")
add_subdirectory(thread_pool)
add_subdirectory(algorithm_test)
add_subdirectory(test)
add_subdirectory(txt_read)
