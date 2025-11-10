# ToriYomi (トリ読み)

> 일본어 게임을 위한 실시간 후리가나 오버레이 + 사전 & Anki 통합 도구

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg)](https://cmake.org/)

## 📖 소개

ToriYomi는 일본어 게임 플레이 중 **실시간으로 한자에 후리가나를 표시**하여 읽기를 돕는 학습 도구입니다. 게임 화면에 비간섭 오버레이로 후리가나를 띄우고, 추출된 문장을 데스크톱 앱에서 관리하며, 사전 검색과 Anki 카드 생성까지 지원합니다.

### ✨ 주요 기능

- 🎮 **게임 화면 실시간 캡처** (DXGI/GDI)
- 📝 **일본어 OCR** (Tesseract)
- 🔤 **한자에만 후리가나 표시** (게임 화면 오버레이)
- 📚 **문장 저장 및 관리** (Qt 데스크톱 앱)
- 🔍 **로컬 사전 검색**
- 📤 **Anki 카드 자동 생성** (AnkiConnect)

### 🎯 목표

일본어 게임을 통한 **몰입형 학습**을 지원합니다. 번역기가 아니라 **읽기 보조 도구**입니다.

---

## 🏗️ 아키텍처

```
┌─────────────┐
│ 게임 화면   │ ──DXGI──> CaptureThread
└─────────────┘              │
                             v
                        FrameQueue (스레드 안전)
                             │
                             v
                         OcrThread (Tesseract)
                             │
                             v
                         Tokenizer (한자→후리가나)
                             │
                     ┌───────┴────────┐
                     v                v
              OverlayWindow     MainApp (Qt)
              (후리가나 표시)    (사전 + Anki)
```

### 스레드 모델

| 스레드 | 역할 |
|--------|------|
| **Main Thread** | Qt UI 이벤트 루프 |
| **CaptureThread** | 화면 캡처 (≥30 FPS) |
| **OcrThread** | OCR + 토큰화 처리 |
| **OverlayRenderThread** | 오버레이 렌더링 (60 FPS) |

---

## 🚀 시작하기

### 📋 요구사항

- **OS**: Windows 10/11 (DirectX 11 지원)
- **컴파일러**: MSVC 2022 (C++20)
- **CMake**: 3.20 이상
- **의존성**:
  - OpenCV 4.8+
  - Qt 6.5+
  - Tesseract 5.0+
  - Google Test 1.12+

### 🔧 설치

#### 1. vcpkg로 의존성 설치

```powershell
# vcpkg 설치
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 라이브러리 설치
.\vcpkg install opencv:x64-windows
.\vcpkg install gtest:x64-windows
.\vcpkg install qt6:x64-windows  # (추후 Phase 5에서 사용)
```

#### 2. 프로젝트 빌드

```powershell
git clone https://github.com/NoirStar/ToriYomi.git
cd ToriYomi

# 자동 빌드 스크립트
.\build.ps1 -Test

# 또는 수동 빌드
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg 경로]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

#### 3. 테스트 실행

```powershell
cd build
ctest -C Release --output-on-failure
```

---

## 📂 프로젝트 구조

```
ToriYomi/
├── CMakeLists.txt              # CMake 설정
├── build.ps1                   # 빌드 자동화 스크립트
├── README.md                   # 프로젝트 소개 (이 파일)
├── BUILD.md                    # 상세 빌드 가이드
├── docs/
│   ├── spec.md                 # 기술 명세서
│   └── code-style.md           # 코드 스타일 가이드
├── src/
│   ├── core/
│   │   ├── capture/            # 화면 캡처 모듈
│   │   │   ├── frame_queue.h/cpp      ✅ (Phase 1-1 완료)
│   │   │   ├── dxgi_capture.h/cpp     (Phase 1-2)
│   │   │   └── frame_differ.h/cpp     (Phase 1-3)
│   │   ├── ocr/                # OCR 모듈
│   │   │   └── tesseract_wrapper.h/cpp (Phase 2-1)
│   │   └── tokenizer/          # 토큰화 모듈
│   │       ├── japanese_tokenizer.h/cpp (Phase 2-2)
│   │       └── furigana_mapper.h/cpp    (Phase 2-3)
│   ├── ui/
│   │   ├── overlay/            # 오버레이 UI
│   │   │   ├── overlay_window.h/cpp     (Phase 3-1)
│   │   │   └── furigana_renderer.h/cpp  (Phase 3-2)
│   │   └── app/                # Qt 데스크톱 앱
│   │       └── main_window.h/cpp        (Phase 5)
│   ├── dict/                   # 사전 모듈
│   │   └── dictionary.h/cpp    (Phase 4-1)
│   └── anki/                   # Anki 통합
│       └── anki_connect_client.h/cpp (Phase 4-2)
└── tests/
    ├── unit/                   # 단위 테스트
    │   └── test_frame_queue.cpp         ✅ (Phase 1-1 완료)
    └── integration/            # 통합 테스트
        └── test_full_pipeline.cpp       (Phase 5)
```

---

## 🧪 개발 방법론: TDD (Test-Driven Development)

모든 기능은 **테스트 주도 개발** 방식으로 구현됩니다:

1. 🔴 **Red**: 실패하는 테스트 먼저 작성
2. 🟢 **Green**: 테스트를 통과하는 최소 코드 작성
3. 🔵 **Refactor**: 코드 품질 개선

### 현재 진행 상황

- [x] ✅ **Phase 1-1**: FrameQueue 구현 (TDD 완료)
  - 스레드 안전 프레임 큐
  - 8개 단위 테스트 작성 및 통과
- [ ] **Phase 1-2**: DXGI Capture 기본 구조
- [ ] **Phase 1-3**: 프레임 변경 감지
- [ ] **Phase 2**: OCR & Tokenization
- [ ] **Phase 3**: Overlay UI
- [ ] **Phase 4**: Dictionary & Anki
- [ ] **Phase 5**: Qt UI & 통합 테스트

상세 로드맵: [docs/spec.md](docs/spec.md)

---

## 🎨 코드 스타일

- **C++20** 표준 사용
- **들여쓰기**: 탭 (TabWidth=4)
- **네이밍**:
  - 클래스/함수: `PascalCase`
  - 변수: `camelCase`
  - 상수: `kPascalCase`
- **RAII**: 스마트 포인터 사용 (`std::unique_ptr`, `std::shared_ptr`)
- **스레드 안전**: 명시적 동기화 (`std::mutex`, `std::atomic`)

자세한 내용: [docs/code-style.md](docs/code-style.md)

---

## 📊 성능 목표

| 항목 | 목표 |
|------|------|
| **레이턴시** (캡처→오버레이) | ≤ 200ms |
| **오버레이 렌더링** | ≤ 16ms (60 FPS) |
| **CPU 사용률** | ≤ 30% (평균) |
| **메모리 사용량** | ≤ 300MB |
| **OCR 스킵률** | ≥ 90% (변경 없는 프레임) |

---

## 🤝 기여하기

이 프로젝트는 현재 개발 초기 단계입니다. 기여를 환영합니다!

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### 개발 규칙

- 모든 기능은 **테스트 먼저** 작성
- 코드 스타일 가이드 준수
- Commit 메시지는 한글 또는 영어로 명확하게

---

## 📄 라이선스

MIT License - 자세한 내용은 [LICENSE](LICENSE) 파일 참조

---

## 📧 문의

- **개발자**: NoirStar
- **GitHub**: [@NoirStar](https://github.com/NoirStar)
- **블로그**: [noirstar.tistory.com](https://noirstar.tistory.com)
- **이메일**: sky_9233@naver.com

---

## 🙏 감사의 말

- [Tesseract OCR](https://github.com/tesseract-ocr/tesseract) - 오픈소스 OCR 엔진
- [OpenCV](https://opencv.org/) - 컴퓨터 비전 라이브러리
- [Qt](https://www.qt.io/) - 크로스 플랫폼 UI 프레임워크
- [AnkiConnect](https://foosoft.net/projects/anki-connect/) - Anki 통합

---

**ToriYomi** = トリ読み (트리요미)
