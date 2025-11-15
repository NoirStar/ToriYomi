cmake_minimum_required(VERSION 3.20)
project(ToriYomi VERSION 0.1.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CRITICAL: Force MSVC runtime library to /MD (Release) and /MDd (Debug)
# This MUST match vcpkg x64-windows triplet (dynamic CRT)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    message(STATUS "MSVC Runtime Library: MultiThreaded$<$<CONFIG:Debug>:Debug>DLL (/MD or /MDd)")
endif()

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
find_package(Qt6 REQUIRED COMPONENTS Quick Qml Widgets UiTools)

include(FetchContent)

set(MECAB_DLL_PATH "C:/Program Files/MeCab/bin/libmecab.dll" CACHE FILEPATH "Path to libmecab.dll for runtime deployment")
if (EXISTS "${MECAB_DLL_PATH}")
	set(TORIYOMI_HAS_MECAB_DLL TRUE)
else()
	set(TORIYOMI_HAS_MECAB_DLL FALSE)
endif()
set(TORIYOMI_MECAB_DLL_WARNING_EMITTED FALSE)

function(toriyomi_copy_mecab_dll target)
	if (NOT TORIYOMI_HAS_MECAB_DLL)
		if (NOT TORIYOMI_MECAB_DLL_WARNING_EMITTED)
			message(WARNING "libmecab.dll not found at MECAB_DLL_PATH=${MECAB_DLL_PATH}. MeCab-dependent targets may fail at runtime.")
			set(TORIYOMI_MECAB_DLL_WARNING_EMITTED TRUE PARENT_SCOPE)
		endif()
		return()
	endif()

	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${MECAB_DLL_PATH}"
			"$<TARGET_FILE_DIR:${target}>"
		COMMENT "Copying MeCab runtime DLL for target ${target}"
		VERBATIM)
endfunction()

find_package(spdlog CONFIG QUIET)
if (NOT spdlog_FOUND)
	message(STATUS "spdlog not found via package manager - fetching header-only dependency")
	set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(spdlog
		URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.zip
	)
	FetchContent_MakeAvailable(spdlog)
endif()

option(TORIYOMI_ENABLE_PADDLEOCR "Enable PaddleOCR backend via FastDeploy" ON)
set(TORIYOMI_FASTDEPLOY_RUNTIME_DIR "" CACHE PATH "Directory containing FastDeploy runtime DLLs to copy next to the app")

if (TORIYOMI_ENABLE_PADDLEOCR)
	find_package(FastDeploy CONFIG QUIET)
	if (NOT FastDeploy_FOUND)
		message(WARNING "FastDeploy package not found; disabling PaddleOCR build. Install FastDeploy and rerun CMake or set -DTORIYOMI_ENABLE_PADDLEOCR=OFF explicitly.")
		set(TORIYOMI_ENABLE_PADDLEOCR OFF CACHE BOOL "Enable PaddleOCR backend via FastDeploy" FORCE)
	endif()
endif()

set(TORIYOMI_FASTDEPLOY_DLLS)
if (TORIYOMI_ENABLE_PADDLEOCR AND TORIYOMI_FASTDEPLOY_RUNTIME_DIR)
	file(GLOB TORIYOMI_FASTDEPLOY_DLLS
		"${TORIYOMI_FASTDEPLOY_RUNTIME_DIR}/*.dll")
	if (NOT TORIYOMI_FASTDEPLOY_DLLS)
		message(WARNING "TORIYOMI_FASTDEPLOY_RUNTIME_DIR is set but no DLLs were found: ${TORIYOMI_FASTDEPLOY_RUNTIME_DIR}")
	endif()
endif()

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
	src/core/ocr/paddle_ocr_wrapper.cpp
	src/core/ocr/ocr_engine_bootstrapper.cpp
	src/core/ocr/ocr_thread.cpp
)

target_link_libraries(toriyomi_ocr
	toriyomi_capture
	${OpenCV_LIBS}
	debug C:/vcpkg/installed/x64-windows/debug/lib/tesseract55d.lib
	optimized C:/vcpkg/installed/x64-windows/lib/tesseract55.lib
	debug C:/vcpkg/installed/x64-windows/debug/lib/leptonica-1.85.0d.lib
	optimized C:/vcpkg/installed/x64-windows/lib/leptonica-1.85.0.lib
	spdlog::spdlog_header_only
)

if (TORIYOMI_ENABLE_PADDLEOCR)
	target_link_libraries(toriyomi_ocr FastDeploy::fastdeploy)
	target_compile_definitions(toriyomi_ocr PUBLIC TORIYOMI_ENABLE_PADDLEOCR)
endif()

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

# UI library - QML Backend module
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 Qml과 Quick 모듈 찾기
find_package(Qt6 REQUIRED COMPONENTS Quick Qml)

add_library(toriyomi_qml_backend
	src/ui/qml_backend/app_backend.cpp
	src/ui/qml_backend/app_backend.h
	src/ui/qml_backend/process_enumerator.cpp
	src/ui/qml_backend/sentence_assembler.cpp
)

target_link_libraries(toriyomi_qml_backend
	toriyomi_capture
	toriyomi_ocr
	toriyomi_tokenizer
	toriyomi_overlay
	Qt6::Quick
	Qt6::Qml
	${OpenCV_LIBS}
	spdlog::spdlog_header_only
)

target_include_directories(toriyomi_qml_backend PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

# UTF-8 인코딩 설정
target_compile_options(toriyomi_qml_backend PRIVATE /utf-8 /Zc:__cplusplus)

# Qt Desktop Application Executable (QML)
qt_add_executable(ToriYomiApp
	src/ui/qml_app/main.cpp
	src/qml/ToriYomiApp/ToriYomiApp.qrc
)
toriyomi_copy_mecab_dll(ToriYomiApp)

target_link_libraries(ToriYomiApp PRIVATE
	toriyomi_qml_backend
	toriyomi_capture
	toriyomi_ocr
	toriyomi_tokenizer
	toriyomi_overlay
	Qt6::Quick
	Qt6::Qml
	${OpenCV_LIBS}
)

set_target_properties(ToriYomiApp PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	WIN32_EXECUTABLE FALSE
)

# UTF-8 인코딩 및 C++ 표준 설정
target_compile_options(ToriYomiApp PRIVATE /utf-8 /Zc:__cplusplus)

# Enable testing
enable_testing()

# 테스트는 Qt를 사용하지 않으므로 AUTOMOC/AUTOUIC 비활성화
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# Unit tests
add_executable(test_frame_queue
	tests/unit/test_frame_queue.cpp
)
toriyomi_copy_mecab_dll(test_frame_queue)

target_link_libraries(test_frame_queue
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

add_executable(test_dxgi_capture
	tests/unit/test_dxgi_capture.cpp
)
toriyomi_copy_mecab_dll(test_dxgi_capture)

target_link_libraries(test_dxgi_capture
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

add_executable(test_gdi_capture
	tests/unit/test_gdi_capture.cpp
)
toriyomi_copy_mecab_dll(test_gdi_capture)

target_link_libraries(test_gdi_capture
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

add_executable(test_capture_thread
	tests/unit/test_capture_thread.cpp
)
toriyomi_copy_mecab_dll(test_capture_thread)

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
	AUTOMOC OFF
	AUTOUIC OFF
)

set_target_properties(test_dxgi_capture PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

set_target_properties(test_gdi_capture PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

set_target_properties(test_capture_thread PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

# OCR tests
add_executable(test_tesseract_wrapper
	tests/unit/test_tesseract_wrapper.cpp
)
toriyomi_copy_mecab_dll(test_tesseract_wrapper)

target_link_libraries(test_tesseract_wrapper
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_tesseract_wrapper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

add_test(NAME TesseractWrapperTest COMMAND test_tesseract_wrapper)

# OCR Thread tests
add_executable(test_ocr_thread
	tests/unit/test_ocr_thread.cpp
)
toriyomi_copy_mecab_dll(test_ocr_thread)

target_link_libraries(test_ocr_thread
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_ocr_thread PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

add_test(NAME OcrThreadTest COMMAND test_ocr_thread)

add_executable(test_ocr_engine_bootstrapper
	tests/unit/test_ocr_engine_bootstrapper.cpp
)
toriyomi_copy_mecab_dll(test_ocr_engine_bootstrapper)

target_link_libraries(test_ocr_engine_bootstrapper
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_ocr_engine_bootstrapper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

add_test(NAME OcrEngineBootstrapperTest COMMAND test_ocr_engine_bootstrapper)

# Tokenizer tests
add_executable(test_japanese_tokenizer
	tests/unit/test_japanese_tokenizer.cpp
)
toriyomi_copy_mecab_dll(test_japanese_tokenizer)

target_link_libraries(test_japanese_tokenizer
	toriyomi_tokenizer
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_japanese_tokenizer PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

# UTF-8 인코딩 설정 (일본어 문자열 지원)
target_compile_options(test_japanese_tokenizer PRIVATE /utf-8)

add_test(NAME JapaneseTokenizerTest COMMAND test_japanese_tokenizer)

# Furigana Mapper tests
add_executable(test_furigana_mapper
	tests/unit/test_furigana_mapper.cpp
)
toriyomi_copy_mecab_dll(test_furigana_mapper)

target_link_libraries(test_furigana_mapper
	toriyomi_tokenizer
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_furigana_mapper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

# UTF-8 인코딩 설정 (일본어 문자열 지원)
target_compile_options(test_furigana_mapper PRIVATE /utf-8)

add_test(NAME FuriganaMapperTest COMMAND test_furigana_mapper)

# Overlay Window tests
add_executable(test_overlay_window
	tests/unit/test_overlay_window.cpp
)
toriyomi_copy_mecab_dll(test_overlay_window)

target_link_libraries(test_overlay_window
	toriyomi_overlay
	toriyomi_tokenizer
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_overlay_window PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

# UTF-8 인코딩 설정
target_compile_options(test_overlay_window PRIVATE /utf-8)

add_test(NAME OverlayWindowTest COMMAND test_overlay_window)

# Overlay Thread tests
add_executable(test_overlay_thread
	tests/unit/test_overlay_thread.cpp
)
toriyomi_copy_mecab_dll(test_overlay_thread)

target_link_libraries(test_overlay_thread
	toriyomi_overlay
	toriyomi_tokenizer
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_overlay_thread PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

# UTF-8 인코딩 설정
target_compile_options(test_overlay_thread PRIVATE /utf-8)

add_test(NAME OverlayThreadTest COMMAND test_overlay_thread)

# ============================================================================
# DLL 자동 복사 규칙 (Debug/Release 분리)
# ============================================================================

# vcpkg DLL 경로 설정
set(VCPKG_BIN_DEBUG "C:/vcpkg/installed/x64-windows/debug/bin")
set(VCPKG_BIN_RELEASE "C:/vcpkg/installed/x64-windows/bin")

# Debug 빌드: Debug DLL만 복사
add_custom_command(TARGET ToriYomiApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying Debug DLLs..."
    # Tesseract Debug
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_BIN_DEBUG}/tesseract55d.dll"
        "$<TARGET_FILE_DIR:ToriYomiApp>"
    # Leptonica Debug
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_BIN_DEBUG}/leptonica-1.85.0d.dll"
        "$<TARGET_FILE_DIR:ToriYomiApp>"
    # Release DLL 제거 (혼재 방지)
    COMMAND ${CMAKE_COMMAND} -E remove -f
        "$<TARGET_FILE_DIR:ToriYomiApp>/tesseract55.dll"
    COMMAND ${CMAKE_COMMAND} -E remove -f
        "$<TARGET_FILE_DIR:ToriYomiApp>/leptonica-1.85.0.dll"
    COMMAND ${CMAKE_COMMAND} -E remove -f
        "$<TARGET_FILE_DIR:ToriYomiApp>/opencv_core4.dll"
    COMMAND ${CMAKE_COMMAND} -E remove -f
        "$<TARGET_FILE_DIR:ToriYomiApp>/opencv_imgproc4.dll"
    COMMENT "Ensuring Debug-only DLLs in output directory"
    VERBATIM
)

if (TORIYOMI_ENABLE_PADDLEOCR AND TORIYOMI_FASTDEPLOY_DLLS)
	foreach(fd_dll ${TORIYOMI_FASTDEPLOY_DLLS})
		add_custom_command(TARGET ToriYomiApp POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${fd_dll}"
				"$<TARGET_FILE_DIR:ToriYomiApp>"
			COMMENT "Copy FastDeploy runtime DLL: ${fd_dll}"
			VERBATIM)
	endforeach()
elseif (TORIYOMI_ENABLE_PADDLEOCR AND NOT TORIYOMI_FASTDEPLOY_RUNTIME_DIR)
	message(STATUS "Set TORIYOMI_FASTDEPLOY_RUNTIME_DIR to copy FastDeploy DLLs automatically, or add the FastDeploy bin directory to PATH.")
endif()
