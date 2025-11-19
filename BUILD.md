# ToriYomi Build Instructions

## Prerequisites

### Required
- **CMake** 3.31+
- **MSVC 2022** (Visual Studio 2022)
- **OpenCV** 4.11+
- **Paddle Inference SDK** 2.6+ (cpp_infer, CPU)
- **MeCab** 0.996+ (Japanese tokenizer)
- **Google Test** 1.17+

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

# Google Test
.\vcpkg install gtest:x64-windows
```

#### 3. Install MeCab (Japanese Tokenizer)
```powershell
# Download and install from https://github.com/ikegami-yukino/mecab/releases
# mecab-0.996-64.exe
# Default install path: C:\Program Files\MeCab
```

#### 4. Install Paddle Inference SDK

1. Download the **Windows CPU x86_64** package from the [Paddle Inference download page](https://www.paddlepaddle.org.cn/inference/download).
2. Extract it to a path such as `C:\Dev\paddle_inference`.
3. Pass the following options to CMake (or set them as cache entries in the GUI):
    - `-DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference"`
    - `-DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib"`
4. All DLLs inside `TORIYOMI_PADDLE_RUNTIME_DIR` will be copied next to every executable/test automatically.

> ℹ️ PaddleOCR 경로는 필수입니다. 더 이상 PaddleOCR을 비활성화할 수 있는 옵션(`TORIYOMI_ENABLE_PADDLEOCR`)은 존재하지 않습니다.

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

You can grab pre-converted packages from the [PaddleOCR release page](https://github.com/PaddlePaddle/PaddleOCR/tree/release/2.7/deploy/cpp_infer) or run the official export scripts.

## Build

### Configure
```powershell
mkdir build
cd build
cmake .. `
    -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64" `
    -DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference" `
    -DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib" `
    -DMECAB_DLL_PATH="C:/Program Files/MeCab/bin/libmecab.dll"
```

> ℹ️ **CMake Options**:
> - `TORIYOMI_PADDLE_DIR`: Root of the Paddle Inference SDK (must contain `paddle/include`, `paddle/lib`).
> - `TORIYOMI_PADDLE_RUNTIME_DIR`: Path to Paddle DLLs. All DLLs in this directory will be copied next to executables.
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
