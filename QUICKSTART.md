# ToriYomi 빠른 시작 가이드 (Windows)

이 문서는 “지금 내 개발 PC에서 다시 빌드해서 실행해 보기”만을 위한 아주 짧은 요약입니다. 세부 설명은 `BUILD.md`를 참고합니다.

---

## 1. 준비물

- Visual Studio 2022 (C++ 데스크톱 개발 워크로드)
- CMake 3.31+
- vcpkg (C++ 라이브러리 설치용)
- Paddle Inference SDK (CPU, C++)
- MeCab 0.996+

이미 한 번 환경을 맞춰 놓았다는 전제로, 대략 경로만 다시 맞춰주면 됩니다.

---

## 2. 의존성 설치

### 2.1 vcpkg 설치 (처음 한 번만)

```powershell
cd C:\dev
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
./vcpkg integrate install
```

### 2.2 vcpkg 패키지

```powershell
./vcpkg install opencv:x64-windows
./vcpkg install gtest:x64-windows
```

자세한 패키지 목록/Abseil 설정 등은 `scripts/install_vcpkg_dependencies.ps1`와 `BUILD.md`를 기준으로 관리합니다.

---

## 3. MeCab & Paddle 설정

### 3.1 MeCab

```powershell
# https://github.com/ikegami-yukino/mecab/releases 에서
# mecab-0.996-64.exe 설치 (예: C:\Program Files\MeCab)
```

### 3.2 Paddle Inference SDK

```powershell
# 예시: C:\Dev\paddle_inference 에 압축 해제
```

모델과 상세 옵션은 `BUILD.md`의 “Paddle Inference SDK 설치”와 “PaddleOCR 모델 배치” 섹션을 그대로 따르면 됩니다.

---

## 4. 빌드

가장 간단한 경로는 레포에 있는 빌드 스크립트를 쓰는 것입니다.

```powershell
git clone https://github.com/NoirStar/ToriYomi.git
cd ToriYomi

./build.ps1 `
  -VcpkgRoot "C:\dev\vcpkg" `
  -PaddleDir "C:\Dev\paddle_inference" `
  -PaddleRuntimeDir "C:\Dev\paddle_inference\paddle\lib"
```

직접 CMake를 돌리고 싶으면 `BUILD.md`의 설정 예시를 그대로 가져다 쓰면 됩니다.

---

## 5. 테스트/실행

### 5.1 테스트 (선택)

```powershell
cd build
ctest -C Release --output-on-failure
```

### 5.2 앱 실행

빌드 후 `build/bin/Release` (또는 Debug) 아래 생성된 실행 파일에서 메인 앱을 실행합니다. 실행 파일 이름/위치는 향후 변경될 수 있으니, CMake 설정과 `src/ui` 구조를 보면서 맞춰가면 됩니다.

---

## 6. 더 자세한 내용

- 빌드/경로 설정 전체 그림: `BUILD.md`
- 시스템 아키텍처/성능 목표: `docs/spec.md`

퀵스타트 문서는 “다시 빌드해서 돌려보는 용도”만 남기고, 과거 단계별 검증 로그나 TDD 결과 등은 다른 문서에서만 관리합니다.
