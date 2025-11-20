cmake_minimum_required(VERSION 3.31)
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
	add_compile_options(/utf-8)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages
find_package(OpenCV REQUIRED)
find_package(GTest REQUIRED)
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


function(toriyomi_copy_paddle_dlls target)
	if (NOT TORIYOMI_PADDLE_DLLS)
		return()
	endif()

	foreach(paddle_dll ${TORIYOMI_PADDLE_DLLS})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${paddle_dll}"
				"$<TARGET_FILE_DIR:${target}>"
			COMMENT "Copying Paddle runtime DLL ${paddle_dll} for target ${target}"
			VERBATIM)
	endforeach()
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

set(TORIYOMI_PADDLE_DIR "" CACHE PATH "Root directory of the Paddle Inference installation (must contain paddle/include and paddle/lib)")
set(TORIYOMI_PADDLE_RUNTIME_DIR "" CACHE PATH "Directory containing Paddle runtime DLLs to copy next to the app")

if (NOT TORIYOMI_PADDLE_DIR)
	message(FATAL_ERROR "TORIYOMI_PADDLE_DIR must be set. Point it to the Paddle Inference SDK root (contains paddle/include and paddle/lib directories).")
endif()

set(PADDLE_INCLUDE_DIR "${TORIYOMI_PADDLE_DIR}/paddle/include")
set(PADDLE_LIB_DIR "${TORIYOMI_PADDLE_DIR}/paddle/lib")
if (NOT TORIYOMI_PADDLE_RUNTIME_DIR)
	set(TORIYOMI_PADDLE_RUNTIME_DIR "${PADDLE_LIB_DIR}" CACHE PATH "Directory containing Paddle runtime DLLs to copy next to the app" FORCE)
endif()

set(PADDLE_RUNTIME_DIRS ${TORIYOMI_PADDLE_RUNTIME_DIR})
set(PADDLE_THIRD_PARTY_INSTALL_DIRS
	"${TORIYOMI_PADDLE_DIR}/paddle/third_party/install"
	"${TORIYOMI_PADDLE_DIR}/third_party/install"
)

foreach(paddle_tp_dir ${PADDLE_THIRD_PARTY_INSTALL_DIRS})
	if (EXISTS "${paddle_tp_dir}")
		file(GLOB PADDLE_THIRD_PARTY_COMPONENT_DIRS LIST_DIRECTORIES TRUE "${paddle_tp_dir}/*")
		foreach(component_dir ${PADDLE_THIRD_PARTY_COMPONENT_DIRS})
			foreach(subdir lib bin)
				if (EXISTS "${component_dir}/${subdir}")
					list(APPEND PADDLE_RUNTIME_DIRS "${component_dir}/${subdir}")
				endif()
			endforeach()
		endforeach()
	endif()
endforeach()

find_library(PADDLE_INFERENCE_LIB
	NAMES paddle_inference
	PATHS ${PADDLE_LIB_DIR}
	NO_DEFAULT_PATH)
if (NOT PADDLE_INFERENCE_LIB)
	message(FATAL_ERROR "Unable to find paddle_inference library under ${PADDLE_LIB_DIR}")
endif()

find_package(yaml-cpp CONFIG REQUIRED)

find_package(absl CONFIG REQUIRED)

set(PADDLEOCR_THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party/paddleocr/third_party)

add_subdirectory(${PADDLEOCR_THIRD_PARTY_DIR}/clipper_ver6.4.2/cpp)

set(TORIYOMI_PADDLE_DLLS)
foreach(runtime_dir ${PADDLE_RUNTIME_DIRS})
	if (EXISTS "${runtime_dir}")
		file(GLOB _paddle_dir_dlls CONFIGURE_DEPENDS "${runtime_dir}/*.dll")
		list(APPEND TORIYOMI_PADDLE_DLLS ${_paddle_dir_dlls})
	endif()
endforeach()
list(REMOVE_DUPLICATES TORIYOMI_PADDLE_DLLS)
if (NOT TORIYOMI_PADDLE_DLLS)
	message(WARNING "Unable to locate Paddle runtime DLLs. Checked directories: ${PADDLE_RUNTIME_DIRS}")
endif()

# Include directories
include_directories(
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/third_party
	${OpenCV_INCLUDE_DIRS}
	C:/vcpkg/installed/x64-windows/include
)

set(PADDLEOCR_SRC_DIR ${CMAKE_SOURCE_DIR}/third_party/paddleocr/src)
file(GLOB_RECURSE PADDLEOCR_SOURCES CONFIGURE_DEPENDS
	${PADDLEOCR_SRC_DIR}/*.cc)
list(FILTER PADDLEOCR_SOURCES EXCLUDE REGEX "src/utils/args.cc$")

add_library(toriyomi_paddleocr STATIC ${PADDLEOCR_SOURCES})
target_include_directories(toriyomi_paddleocr
	PUBLIC
		${CMAKE_SOURCE_DIR}/third_party/paddleocr
		${CMAKE_SOURCE_DIR}/third_party/paddleocr/src
		${CMAKE_SOURCE_DIR}/third_party/paddleocr/third_party/nlohmann
		${PADDLEOCR_THIRD_PARTY_DIR}/clipper_ver6.4.2/cpp
		${PADDLEOCR_THIRD_PARTY_DIR}/polyclipping
		${PADDLEOCR_THIRD_PARTY_DIR}
		${PADDLE_INCLUDE_DIR}
)
target_link_libraries(toriyomi_paddleocr
	PUBLIC
		${PADDLE_INFERENCE_LIB}
		yaml-cpp::yaml-cpp
		absl::statusor
		polyclipping
)
target_compile_options(toriyomi_paddleocr PRIVATE /bigobj)

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
	src/core/ocr/paddle_ocr_wrapper.cpp
	src/core/ocr/ocr_engine_bootstrapper.cpp
	src/core/ocr/ocr_thread.cpp
	src/core/ocr/paddle/paddle_ocr_options.cpp
)

target_link_libraries(toriyomi_ocr
	toriyomi_capture
	${OpenCV_LIBS}
	spdlog::spdlog_header_only
	toriyomi_paddleocr
)

target_include_directories(toriyomi_ocr PUBLIC
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/third_party/paddleocr
	${CMAKE_SOURCE_DIR}/third_party/paddleocr/src
	${CMAKE_SOURCE_DIR}/third_party/paddleocr/third_party/nlohmann
	${PADDLEOCR_THIRD_PARTY_DIR}/clipper_ver6.4.2/cpp
	${PADDLEOCR_THIRD_PARTY_DIR}/polyclipping
	${PADDLEOCR_THIRD_PARTY_DIR}
	${PADDLE_INCLUDE_DIR}
)

# UTF-8 인코딩 설정 (한글/일본어 소스 경고 방지)
target_compile_options(toriyomi_ocr PRIVATE /utf-8)

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
toriyomi_copy_paddle_dlls(ToriYomiApp)

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
toriyomi_copy_paddle_dlls(test_frame_queue)

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
toriyomi_copy_paddle_dlls(test_dxgi_capture)

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
toriyomi_copy_paddle_dlls(test_gdi_capture)

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
toriyomi_copy_paddle_dlls(test_capture_thread)

target_link_libraries(test_capture_thread
	toriyomi_capture
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

add_executable(test_paddle_ocr_integration
	tests/integration/test_paddle_ocr_integration.cpp
)
toriyomi_copy_mecab_dll(test_paddle_ocr_integration)
toriyomi_copy_paddle_dlls(test_paddle_ocr_integration)

target_link_libraries(test_paddle_ocr_integration
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

target_include_directories(test_paddle_ocr_integration PRIVATE
	${CMAKE_SOURCE_DIR}/third_party/paddleocr/third_party/nlohmann
)

target_compile_definitions(test_paddle_ocr_integration PRIVATE
	TORIYOMI_UI_SCREENSHOT_BASE64_PATH="${CMAKE_SOURCE_DIR}/tests/data/ocr_samples/ui_screenshot_base64.txt"
)

# Add test to CTest
add_test(NAME FrameQueueTest COMMAND test_frame_queue)
add_test(NAME DxgiCaptureTest COMMAND test_dxgi_capture)
add_test(NAME GdiCaptureTest COMMAND test_gdi_capture)
add_test(NAME CaptureThreadTest COMMAND test_capture_thread)
add_test(NAME PaddleOcrIntegrationTest COMMAND test_paddle_ocr_integration)

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

set_target_properties(test_paddle_ocr_integration PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

# OCR tests
add_executable(test_paddle_ocr_wrapper
	tests/unit/test_paddle_ocr_wrapper.cpp
)
toriyomi_copy_mecab_dll(test_paddle_ocr_wrapper)
toriyomi_copy_paddle_dlls(test_paddle_ocr_wrapper)

target_link_libraries(test_paddle_ocr_wrapper
	toriyomi_ocr
	GTest::gtest
	GTest::gtest_main
	${OpenCV_LIBS}
)

set_target_properties(test_paddle_ocr_wrapper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
	AUTOMOC OFF
	AUTOUIC OFF
)

add_test(NAME PaddleOcrWrapperTest COMMAND test_paddle_ocr_wrapper)

# OCR Thread tests
add_executable(test_ocr_thread
	tests/unit/test_ocr_thread.cpp
)
toriyomi_copy_mecab_dll(test_ocr_thread)
toriyomi_copy_paddle_dlls(test_ocr_thread)

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
toriyomi_copy_paddle_dlls(test_ocr_engine_bootstrapper)

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
toriyomi_copy_paddle_dlls(test_japanese_tokenizer)

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

# vcpkg DLL 경로 설정 (참고용 상수)
set(VCPKG_BIN_DEBUG "C:/vcpkg/installed/x64-windows/debug/bin")
set(VCPKG_BIN_RELEASE "C:/vcpkg/installed/x64-windows/bin")
