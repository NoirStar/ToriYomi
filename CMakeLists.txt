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
find_package(Tesseract REQUIRED)
find_package(Leptonica REQUIRED)

# Include directories
include_directories(
	${CMAKE_SOURCE_DIR}/src
	${OpenCV_INCLUDE_DIRS}
	C:/vcpkg/installed/x64-windows/include
)

# Core library - Capture module
add_library(toriyomi_capture
	src/core/capture/frame_queue.cpp
	src/core/capture/dxgi_capture.cpp
	src/core/capture/gdi_capture.cpp
	src/core/capture/capture_thread.cpp
)

target_link_libraries(toriyomi_capture
	${OpenCV_LIBS}
	d3d11
	dxgi
)

target_include_directories(toriyomi_capture PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

# Core library - OCR module
add_library(toriyomi_ocr
	src/core/ocr/ocr_engine.cpp
	src/core/ocr/tesseract_wrapper.cpp
)

target_link_libraries(toriyomi_ocr
	${OpenCV_LIBS}
	C:/vcpkg/installed/x64-windows/lib/tesseract55.lib
	C:/vcpkg/installed/x64-windows/lib/leptonica-1.85.0.lib
)

target_include_directories(toriyomi_ocr PUBLIC
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

add_executable(test_dxgi_capture
	tests/unit/test_dxgi_capture.cpp
)

target_link_libraries(test_dxgi_capture
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

add_executable(test_gdi_capture
	tests/unit/test_gdi_capture.cpp
)

target_link_libraries(test_gdi_capture
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

add_executable(test_capture_thread
	tests/unit/test_capture_thread.cpp
)

target_link_libraries(test_capture_thread
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

# Add test to CTest
add_test(NAME FrameQueueTest COMMAND test_frame_queue)
add_test(NAME DxgiCaptureTest COMMAND test_dxgi_capture)
add_test(NAME GdiCaptureTest COMMAND test_gdi_capture)
add_test(NAME CaptureThreadTest COMMAND test_capture_thread)

# Test executable properties
set_target_properties(test_frame_queue PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

set_target_properties(test_dxgi_capture PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

set_target_properties(test_gdi_capture PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

set_target_properties(test_capture_thread PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# OCR tests
add_executable(test_tesseract_wrapper
	tests/unit/test_tesseract_wrapper.cpp
)

target_link_libraries(test_tesseract_wrapper
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_tesseract_wrapper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_test(NAME TesseractWrapperTest COMMAND test_tesseract_wrapper)
