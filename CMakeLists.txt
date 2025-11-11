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
find_package(mecab CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets UiTools)

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
	src/core/ocr/ocr_thread.cpp
)

target_link_libraries(toriyomi_ocr
	toriyomi_capture
	${OpenCV_LIBS}
	C:/vcpkg/installed/x64-windows/lib/tesseract55.lib
	C:/vcpkg/installed/x64-windows/lib/leptonica-1.85.0.lib
)

target_include_directories(toriyomi_ocr PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

# Core library - Tokenizer module
add_library(toriyomi_tokenizer
	src/core/tokenizer/japanese_tokenizer.cpp
	src/core/tokenizer/furigana_mapper.cpp
)

target_link_libraries(toriyomi_tokenizer
	${OpenCV_LIBS}
	"C:/Program Files/MeCab/sdk/libmecab.lib"
)

target_include_directories(toriyomi_tokenizer PUBLIC
	${CMAKE_SOURCE_DIR}/src
	"C:/Program Files/MeCab/sdk"
)

# UTF-8 인코딩 설정 (일본어 문자열 지원)
target_compile_options(toriyomi_tokenizer PRIVATE /utf-8)

# UI library - Overlay module
add_library(toriyomi_overlay
	src/ui/overlay/overlay_window.cpp
	src/ui/overlay/overlay_thread.cpp
)

target_link_libraries(toriyomi_overlay
	toriyomi_tokenizer
	${OpenCV_LIBS}
	gdiplus
)

target_include_directories(toriyomi_overlay PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

# UTF-8 인코딩 설정
target_compile_options(toriyomi_overlay PRIVATE /utf-8)

# UI library - Qt App module
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(toriyomi_app
	src/ui/app/main_app.cpp
	src/ui/app/roi_selector_dialog.cpp
	src/ui/app/draggable_image_label.cpp
	src/ui/app/interactive_sentence_widget.cpp
	src/ui/app/app.ui
)

target_link_libraries(toriyomi_app
	toriyomi_tokenizer
	Qt6::Widgets
	Qt6::UiTools
	${OpenCV_LIBS}
)

target_include_directories(toriyomi_app PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

# UTF-8 인코딩 설정
target_compile_options(toriyomi_app PRIVATE /utf-8 /Zc:__cplusplus)

# Qt Desktop Application Executable
add_executable(ToriYomiApp WIN32
	src/ui/app/main.cpp
)

target_link_libraries(ToriYomiApp
	toriyomi_app
	toriyomi_tokenizer
	Qt6::Widgets
	Qt6::UiTools
	${OpenCV_LIBS}
)

set_target_properties(ToriYomiApp PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	WIN32_EXECUTABLE TRUE
)

# UTF-8 인코딩 및 C++ 표준 설정
target_compile_options(ToriYomiApp PRIVATE /utf-8 /Zc:__cplusplus)

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

# OCR Thread tests
add_executable(test_ocr_thread
	tests/unit/test_ocr_thread.cpp
)

target_link_libraries(test_ocr_thread
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_ocr_thread PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_test(NAME OcrThreadTest COMMAND test_ocr_thread)

# Tokenizer tests
add_executable(test_japanese_tokenizer
	tests/unit/test_japanese_tokenizer.cpp
)

target_link_libraries(test_japanese_tokenizer
	toriyomi_tokenizer
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_japanese_tokenizer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# UTF-8 인코딩 설정 (일본어 문자열 지원)
target_compile_options(test_japanese_tokenizer PRIVATE /utf-8)

add_test(NAME JapaneseTokenizerTest COMMAND test_japanese_tokenizer)

# Furigana Mapper tests
add_executable(test_furigana_mapper
	tests/unit/test_furigana_mapper.cpp
)

target_link_libraries(test_furigana_mapper
	toriyomi_tokenizer
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_furigana_mapper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# UTF-8 인코딩 설정 (일본어 문자열 지원)
target_compile_options(test_furigana_mapper PRIVATE /utf-8)

add_test(NAME FuriganaMapperTest COMMAND test_furigana_mapper)

# Overlay Window tests
add_executable(test_overlay_window
	tests/unit/test_overlay_window.cpp
)

target_link_libraries(test_overlay_window
	toriyomi_overlay
	toriyomi_tokenizer
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_overlay_window PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# UTF-8 인코딩 설정
target_compile_options(test_overlay_window PRIVATE /utf-8)

add_test(NAME OverlayWindowTest COMMAND test_overlay_window)

# Overlay Thread tests
add_executable(test_overlay_thread
	tests/unit/test_overlay_thread.cpp
)

target_link_libraries(test_overlay_thread
	toriyomi_overlay
	toriyomi_tokenizer
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_overlay_thread PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# UTF-8 인코딩 설정
target_compile_options(test_overlay_thread PRIVATE /utf-8)

add_test(NAME OverlayThreadTest COMMAND test_overlay_thread)
