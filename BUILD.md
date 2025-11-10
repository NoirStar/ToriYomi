# ToriYomi Build Instructions

## Prerequisites

### Required
- **CMake** 3.20+
- **MSVC 2022** (Visual Studio 2022)
- **OpenCV** 4.8+
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

# Google Test
.\vcpkg install gtest:x64-windows
```

## Build

### Configure
```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

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
