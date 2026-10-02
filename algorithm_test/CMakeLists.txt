cmake_minimum_required(VERSION 3.16)
project(algorithm_exercises LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(algorithm_matrix matrix_test.cpp)
add_executable(algorithm_sort sort_test.cpp)
add_executable(algorithm_red_black_tree RedBlack_Tree_test.cpp)
