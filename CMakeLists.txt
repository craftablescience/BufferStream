cmake_minimum_required(VERSION 3.16)
project(bufferstream)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUFFERSTREAM_BUILD_TESTS "Build tests" OFF)

# Create library
add_library(${PROJECT_NAME} INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/BufferStream.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/FileStream.h")

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Create tests
if(BUFFERSTREAM_BUILD_TESTS)
    set(BUFFERSTREAM_TEST_NAME "${PROJECT_NAME}_test")

    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0)
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    add_executable(${BUFFERSTREAM_TEST_NAME}
            "${CMAKE_CURRENT_SOURCE_DIR}/test/BufferStream.cpp")

    target_link_libraries(${BUFFERSTREAM_TEST_NAME} PUBLIC
            gtest_main ${PROJECT_NAME})

    include(GoogleTest)
    gtest_discover_tests(${BUFFERSTREAM_TEST_NAME})
endif()
