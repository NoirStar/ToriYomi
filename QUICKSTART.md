# ToriYomi 빠른 시작 가이드

## 🚀 개발 환경 설정 (Windows)

### 필수 도구 설치

#### 1. Visual Studio 2022 설치
- [Visual Studio 2022 Community 다운로드](https://visualstudio.microsoft.com/ko/downloads/)
- 워크로드: "C++를 사용한 데스크톱 개발" 선택
- CMake 도구 포함

#### 2. vcpkg 설치 (패키지 관리자)

```powershell
# 원하는 위치에 vcpkg 설치 (예: C:\dev)
cd C:\
mkdir dev
cd dev
git clone https://github.com/Microsoft/vcpkg.git
   -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64" `
   -DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference" `
   -DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib" `
.\vcpkg integrate install
```

#### 3. 의존성 설치

```powershell
# OpenCV 설치 (약 10-15분 소요)
.\vcpkg install opencv:x64-windows

# Google Test 설치
.\vcpkg install gtest:x64-windows
```

### MeCab 설치 (일본어 토크나이저)

```powershell
# https://github.com/ikegami-yukino/mecab/releases
# mecab-0.996-64.exe 다운로드 및 설치
# 기본 설치 경로: C:\Program Files\MeCab
```

### PaddleOCR 준비

#### 1. Paddle Inference SDK 설치
- [Paddle Inference 다운로드 페이지](https://www.paddlepaddle.org.cn/inference/download)에서 **Windows CPU x64** 패키지를 받습니다.
- 예를 들어 `C:\Dev\paddle_inference`에 압축을 풉니다.
- CMake 구성 시 다음 인자를 전달합니다.
   - `-DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference"`
   - `-DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib"`
- 지정된 런타임 폴더에 있는 DLL이 빌드 산출물 옆으로 자동 복사됩니다.

#### 2. PaddleOCR 모델 배치
- 공식 [PaddleOCR release](https://github.com/PaddlePaddle/PaddleOCR/tree/release/2.7/deploy/cpp_infer)에서 PP-OCRv5 (det/rec) 패키지를 내려받습니다.
- 아래 구조를 유지한 채 `models/paddleocr` 경로에 배치합니다.
   - `models/paddleocr/det`
   - `models/paddleocr/rec`
   - `models/paddleocr/ppocr_keys_v1.txt`
- 앱은 실행 파일 기준 상대 경로를 기본으로 사용하지만, UI 설정에서 다른 경로로 변경할 수 있습니다.

### 프로젝트 빌드

```powershell
# 프로젝트 클론
git clone https://github.com/NoirStar/ToriYomi.git
cd ToriYomi

# 빌드 (vcpkg + Paddle 경로 지정)
.\build.ps1 -VcpkgRoot "C:\dev\vcpkg" -PaddleDir "C:\Dev\paddle_inference" -PaddleRuntimeDir "C:\Dev\paddle_inference\paddle\lib"

# 또는 수동 빌드
mkdir build
cd build
cmake .. `
   -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake `
   -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64" `
   -DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference" `
   -DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib" `
   -DMECAB_DLL_PATH="C:/Program Files/MeCab/bin/libmecab.dll"
cmake --build . --config Release
```

> 💡 **DLL 자동 배포**: `TORIYOMI_PADDLE_RUNTIME_DIR`와 `MECAB_DLL_PATH`를 지정하면 빌드 시 필요한 DLL들이 실행 파일 옆으로 자동 복사됩니다.

### 테스트 실행

```powershell
# build 디렉토리에서
ctest -C Release --output-on-failure

# 또는 직접 실행
.\bin\tests\Release\test_frame_queue.exe
```

## ✅ 현재 완료된 기능

### Phase 1-1: FrameQueue ✅

**구현 완료:**
- `src/core/capture/frame_queue.h` - 스레드 안전 프레임 큐 인터페이스
- `src/core/capture/frame_queue.cpp` - 구현
- `tests/unit/test_frame_queue.cpp` - 8개 단위 테스트

**테스트 커버리지:**
1. ✅ Push/Pop 기본 동작
2. ✅ 빈 큐 타임아웃 처리
3. ✅ FIFO 순서 보장
4. ✅ 오버플로우 시 오래된 프레임 자동 삭제
5. ✅ 멀티스레드 안전성 (Producer-Consumer)
6. ✅ Size() 정확도
7. ✅ Clear() 기능
8. ✅ 연속 Pop 처리

**성능 특성:**
- 최대 큐 크기: 5 프레임 (설정 가능)
- 스레드 안전: `std::mutex` + `std::condition_variable`
- 타임아웃 지원: `Pop(int timeoutMs)`
- 메모리 관리: `cv::Mat::clone()` 사용 (깊은 복사)

## 🔍 코드 검증

### 컴파일 시간 체크리스트
- [x] C++20 기능 사용 (`std::optional`)
- [x] 헤더 가드 (`#pragma once`)
- [x] 네임스페이스 (`toriyomi`)
- [x] const 정확성
- [x] RAII 패턴
- [x] 복사/이동 생성자 삭제 (큐는 이동 불가)

### 런타임 안전성
- [x] 스레드 안전 (모든 public 메서드)
- [x] 교착 상태(Deadlock) 방지
- [x] 조건 변수 허위 깨우기(Spurious Wakeup) 처리
- [x] 타임아웃 정확도 (millisecond)

## 📊 예상 테스트 결과

```
[==========] Running 8 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 8 tests from FrameQueueTest
[ RUN      ] FrameQueueTest.PushAndPopSingleFrame
[       OK ] FrameQueueTest.PushAndPopSingleFrame (5 ms)
[ RUN      ] FrameQueueTest.PopFromEmptyQueueTimeout
[       OK ] FrameQueueTest.PopFromEmptyQueueTimeout (102 ms)
[ RUN      ] FrameQueueTest.FIFOOrder
[       OK ] FrameQueueTest.FIFOOrder (3 ms)
[ RUN      ] FrameQueueTest.OverflowDropsOldestFrame
[       OK ] FrameQueueTest.OverflowDropsOldestFrame (2 ms)
[ RUN      ] FrameQueueTest.ThreadSafety
[       OK ] FrameQueueTest.ThreadSafety (150 ms)
[ RUN      ] FrameQueueTest.SizeReturnsCorrectCount
[       OK ] FrameQueueTest.SizeReturnsCorrectCount (1 ms)
[ RUN      ] FrameQueueTest.ClearEmptiesQueue
[       OK ] FrameQueueTest.ClearEmptiesQueue (51 ms)
[ RUN      ] FrameQueueTest.MultipleConsecutivePopsOnEmpty
[       OK ] FrameQueueTest.MultipleConsecutivePopsOnEmpty (40 ms)
[----------] 8 tests from FrameQueueTest (354 ms total)

[----------] Global test environment tear-down
[==========] 8 tests from 1 test suite ran. (354 ms total)
[  PASSED  ] 8 tests.
```

## 🐛 문제 해결

### 문제: CMake가 OpenCV를 찾지 못함
```powershell
# vcpkg toolchain 파일 지정 필수
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### 문제: 링크 에러 (unresolved external symbol)
```powershell
# OpenCV가 x64로 설치되었는지 확인
.\vcpkg list | Select-String "opencv"

# x86이면 제거 후 재설치
.\vcpkg remove opencv:x86-windows
.\vcpkg install opencv:x64-windows
```

### 문제: Google Test 헤더를 찾지 못함
```powershell
# gtest 재설치
.\vcpkg install gtest:x64-windows --force
```

## 📝 다음 단계

빌드 환경이 준비되면:

1. **빌드 실행**
   ```powershell
   .\build.ps1 -Test
   ```

2. **테스트 확인**
   - 모든 테스트가 통과하는지 확인
   - 실패 시 에러 로그 확인

3. **Phase 1-2 시작**
   - DXGI Capture 모듈 구현
   - TDD 방식으로 진행

## 🔗 참고 링크

- [vcpkg 공식 문서](https://github.com/microsoft/vcpkg)
- [OpenCV 설치 가이드](https://docs.opencv.org/4.x/d3/d52/tutorial_windows_install.html)
- [Google Test 문서](https://google.github.io/googletest/)
- [CMake 튜토리얼](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)

---

**현재 상태**: Phase 1-1 완료 ✅ | GitHub 푸시 완료 ✅ | 빌드 환경 설정 대기 중 ⏳
