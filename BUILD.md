# ToriYomi Build Instructions

## Prerequisites

### Required
- **CMake** 3.20+
- **MSVC 2022** (Visual Studio 2022)
- **OpenCV** 4.8+
- **FastDeploy** 2.3+ (PaddleOCR runtime, default)
- **Tesseract** 5.5+ (fallback OCR engine)
- **MeCab** 0.996+ (Japanese tokenizer)
- **Google Test** 1.12+

### Installation (Windows)

#### 1. Install vcpkg (Package Manager)
```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

#### 2. Install Dependencies
```powershell
# OpenCV
.\vcpkg install opencv:x64-windows

# Tesseract (OCR fallback)
.\vcpkg install tesseract:x64-windows
.\vcpkg install leptonica:x64-windows

# Google Test
.\vcpkg install gtest:x64-windows
```

#### 3. Install MeCab (Japanese Tokenizer)
```powershell
# Download and install from https://github.com/ikegami-yukino/mecab/releases
# mecab-0.996-64.exe
# Default install path: C:\Program Files\MeCab
```

#### 4. Install FastDeploy

1. Download the latest Windows CPU package from the [FastDeploy releases](https://github.com/PaddlePaddle/FastDeploy/releases)
2. Extract it to a path such as `C:\dev\fastdeploy`
3. Add `C:\dev\fastdeploy\lib\cmake\FastDeploy` to `CMAKE_PREFIX_PATH` **or** set `-DFastDeploy_DIR=C:\dev\fastdeploy\lib\cmake\FastDeploy`
4. (Optional) Point `TORIYOMI_FASTDEPLOY_RUNTIME_DIR` to the folder that contains the DLLs so CMake can copy them next to the executable

> ℹ️ If FastDeploy is not available, the build will automatically fall back to Tesseract-only mode. You can also explicitly disable PaddleOCR with `-DTORIYOMI_ENABLE_PADDLEOCR=OFF`.

#### 5. Download PaddleOCR models

Place the PP-OCR models under `models/paddleocr` (relative to the app binary). The directory must contain `det`, `rec`, `cls`, and `ppocr_keys_v1.txt`.

```
models/
    paddleocr/
        det/
        rec/
        cls/
        ppocr_keys_v1.txt
```

You can grab pre-converted packages from the [FastDeploy model zoo](https://github.com/PaddlePaddle/FastDeploy/blob/develop/docs/en/build_and_install/download_fastdeploy_and_models.md) or run the official export scripts.

## Build

### Configure
```powershell
mkdir build
cd build
cmake .. `
    -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64;C:/dev/fastdeploy" `
    -DTORIYOMI_FASTDEPLOY_RUNTIME_DIR="C:/dev/fastdeploy/lib" `
    -DMECAB_DLL_PATH="C:/Program Files/MeCab/bin/libmecab.dll"
```

> ℹ️ **CMake Options**:
> - `TORIYOMI_ENABLE_PADDLEOCR` (default: `ON`): Enable PaddleOCR engine. Automatically disabled if FastDeploy is not found.
> - `TORIYOMI_FASTDEPLOY_RUNTIME_DIR`: Path to FastDeploy DLLs. All DLLs in this directory will be copied next to executables.
> - `MECAB_DLL_PATH`: Path to `libmecab.dll`. Will be copied next to all executables and tests automatically.

### Compile
```powershell
cmake --build . --config Release
```

## Run Tests

### All Tests
```powershell
cd build
ctest -C Release --output-on-failure
```

### Specific Test
```powershell
.\bin\tests\Release\test_frame_queue.exe
```

## Project Structure

```
ToriYomi/
├── CMakeLists.txt
├── docs/
│   ├── spec.md           # Technical specification
│   └── code-style.md     # Code style guide
├── src/
│   └── core/
│       └── capture/
│           ├── frame_queue.h
│           └── frame_queue.cpp
└── tests/
    └── unit/
        └── test_frame_queue.cpp
```

## Development Workflow (TDD)

1. **Red**: Write failing test first
2. **Green**: Write minimal code to pass the test
3. **Refactor**: Improve code quality

### Example: Adding a new feature
```powershell
# 1. Write test
# Edit tests/unit/test_my_feature.cpp

# 2. Run test (should fail)
cmake --build build --config Release
.\build\bin\tests\Release\test_my_feature.exe

# 3. Implement feature
# Edit src/my_feature.cpp

# 4. Run test again (should pass)
.\build\bin\tests\Release\test_my_feature.exe

# 5. Refactor and commit
git add .
git commit -m "Add my_feature with tests"
```

## Troubleshooting

### OpenCV not found
```powershell
# Make sure CMAKE_TOOLCHAIN_FILE points to vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Google Test not found
```powershell
# Reinstall gtest
.\vcpkg remove gtest:x64-windows
.\vcpkg install gtest:x64-windows
```

## Next Steps

See [docs/spec.md](docs/spec.md) for the full technical specification and development roadmap.
