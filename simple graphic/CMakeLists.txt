cmake_minimum_required(VERSION 3.16)
project(simple_graphic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(simple_graphic main.cpp
        TGA_image.h
        TGA_image.cpp
        geometry.h
        our_gl.h
        our_gl.cpp
        Model.h
        Model.cpp)
