cmake_minimum_required(VERSION 3.16)
project(word_count LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(word_count test.cpp)
configure_file(Bible.txt Bible.txt COPYONLY)
