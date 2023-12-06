cmake_minimum_required(VERSION 3.16)
project(bufferstream)
set(CMAKE_CXX_STANDARD 20)

add_library(${PROJECT_NAME} STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/BufferStream.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/BufferStream.cpp")

target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
