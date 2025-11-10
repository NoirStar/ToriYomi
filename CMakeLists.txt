cmake_minimum_required(VERSION 3.20)
project(ToriYomi VERSION 0.1.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages
find_package(OpenCV REQUIRED)
find_package(GTest REQUIRED)

# Include directories
include_directories(
	${CMAKE_SOURCE_DIR}/src
	${OpenCV_INCLUDE_DIRS}
)

# Core library - Capture module
add_library(toriyomi_capture
	src/core/capture/frame_queue.cpp
)

target_link_libraries(toriyomi_capture
	${OpenCV_LIBS}
)

target_include_directories(toriyomi_capture PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

# Enable testing
enable_testing()

# Unit tests
add_executable(test_frame_queue
	tests/unit/test_frame_queue.cpp
)

target_link_libraries(test_frame_queue
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

# Add test to CTest
add_test(NAME FrameQueueTest COMMAND test_frame_queue)

# Test executable properties
set_target_properties(test_frame_queue PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)
