cmake_minimum_required(VERSION 3.16)
project(bufferstream)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUFFERSTREAM_BUILD_TESTS "Build tests" OFF)

# Create library
add_library(${PROJECT_NAME} INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/BufferStream.h")

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
