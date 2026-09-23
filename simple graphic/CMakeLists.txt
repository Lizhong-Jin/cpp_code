cmake_minimum_required(VERSION 3.0)
project(simple_graphic)

set(CMAKE_CXX_STANDARD 26)

add_executable(simple_graphic main.cpp
        TGA_image.h
        TGA_image.cpp
        geometry.h
        our_gl.h
        our_gl.cpp
        Model.h
        Model.cpp)
