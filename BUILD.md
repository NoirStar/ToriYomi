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

권장 방법은 저장소에 포함된 스크립트를 사용하는 것입니다. Abseil 20250814 이후 버전은 C++20 강제 옵션과 pthread 탐지를 우회해야 하므로, 스크립트가 이 환경 변수를 자동으로 설정합니다.

```powershell
cd ToriYomi
.\scripts\install_vcpkg_dependencies.ps1 `
    -VcpkgRoot "H:\dev\vcpkg" `
    -Packages @('abseil','opencv','gtest','fmt','yaml-cpp','spdlog')
```

> 필요에 따라 `-Packages` 목록을 수정하세요. `vcpkg.exe`를 직접 실행해야 한다면 `VCPKG_CMAKE_CONFIGURE_OPTIONS="-DCMAKE_CXX_STANDARD=20;-DTHREADS_PREFER_PTHREAD_FLAG=OFF"` 와 `VCPKG_CXX_FLAGS="/std:c++20"` 를 동일하게 설정해야 Abseil DLL 구성이 멈추지 않습니다.

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
# ToriYomi 빌드 가이드 (Windows)

개인 개발 환경에서 ToriYomi를 다시 빌드할 때 필요한 최소한의 절차만 정리한 문서입니다.

---

## 1. 필수 도구

- **CMake** 3.31+
- **Visual Studio 2022 (MSVC 2022)**
- **OpenCV** 4.11+
- **Paddle Inference SDK** 2.6+ (cpp_infer, CPU 전용)
- **MeCab** 0.996+ (일본어 토크나이저)

> 지금 시점에서는 프로젝트가 PaddleOCR에 100% 의존하므로, Paddle Inference SDK와 모델 경로는 항상 필수라고 가정합니다.

---

## 2. vcpkg 및 C++ 라이브러리 설치

### 2.1 vcpkg 설치

```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
./vcpkg integrate install
```

### 2.2 의존성 설치 스크립트 실행

레포에 포함된 스크립트를 사용하는 것을 기준으로 합니다.

```powershell
cd ToriYomi
./scripts/install_vcpkg_dependencies.ps1 `
    -VcpkgRoot "H:\dev\vcpkg" `
    -Packages @('abseil','opencv','gtest','fmt','yaml-cpp','spdlog')
```

Abseil 최신 버전의 C++20 강제 옵션, pthread 플래그 등은 이 스크립트에서 자동으로 처리한다고 가정하고, 여기서는 세부 환경 변수 설명은 생략합니다.

---

## 3. MeCab 설치

```powershell
# https://github.com/ikegami-yukino/mecab/releases 에서
# mecab-0.996-64.exe 다운로드 후 설치
# 기본 경로 예시: C:\Program Files\MeCab
```

`libmecab.dll` 경로는 CMake 설정 시 `MECAB_DLL_PATH` 옵션으로 넘겨서, 실행 파일/테스트 옆으로 자동 복사되도록 합니다.

---

## 4. Paddle Inference SDK 설치

1. [Paddle Inference 다운로드 페이지](https://www.paddlepaddle.org.cn/inference/download)에서 **Windows CPU x86_64** 패키지를 받습니다.
2. 예를 들어 `C:\Dev\paddle_inference` 에 압축을 풉니다.
3. CMake 설정 시 다음 옵션을 넘깁니다.

    - `-DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference"`
    - `-DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib"`

`TORIYOMI_PADDLE_RUNTIME_DIR` 아래에 있는 DLL 들은 빌드 시 실행 파일/테스트 옆으로 자동 복사됩니다.

> PaddleOCR를 끄는 빌드 옵션은 더 이상 고려하지 않습니다. 항상 Paddle을 쓴다는 가정으로만 관리합니다.

---

## 5. PaddleOCR 모델 배치

앱 바이너리 기준으로 `models/paddleocr` 아래에 PP-OCR 모델들을 둡니다.

필수 디렉터리 구조는 다음과 같습니다.

```text
models/
  paddleocr/
    det/
    rec/
    cls/
    ppocr_keys_v1.txt
```

모델 파일은 [PaddleOCR cpp_infer 릴리스](https://github.com/PaddlePaddle/PaddleOCR/tree/release/2.7/deploy/cpp_infer)에서 받거나, 공식 스크립트로 직접 export 해서 넣으면 됩니다.

---

## 6. CMake 설정 및 빌드

### 6.1 Configure

```powershell
mkdir build
cd build
cmake .. `
  -DCMAKE_TOOLCHAIN_FILE="H:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.0/msvc2022_64" `
  -DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference" `
  -DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib" `
  -DMECAB_DLL_PATH="C:/Program Files/MeCab/bin/libmecab.dll"
```

각 경로는 실제 본인 개발 환경에 맞게 수정하면 됩니다.

### 6.2 Build

```powershell
cmake --build . --config Release
```

디버그 빌드가 필요하면 `--config Debug` 로만 바꿔서 쓰면 됩니다.

---

## 7. 테스트 실행 (선택)

현재는 혼자 개발하는 상황이라 필수는 아니지만, 기존 단위 테스트를 돌리고 싶을 때만 사용합니다.

### 7.1 전체 테스트

```powershell
cd build
ctest -C Release --output-on-failure
```

### 7.2 개별 테스트 실행 예시

```powershell
./bin/tests/Release/test_frame_queue.exe
```

---

## 8. 참고

- 전체 기술 명세 및 로드맵: `docs/spec.md`
- 코드 스타일: `docs/code-style.md`

이 문서는 앞으로 개발 환경이 바뀔 때마다, 사용하지 않는 옵션/경고 문구는 바로 지우고, 실제로 사용하는 설정만 남기는 것을 원칙으로 합니다.
