# **ToriYomi C++ Code Style Guide**

본 문서는 ToriYomi 프로젝트의 코드 일관성과 유지보수성을 위해 정의된 C++ 스타일 가이드입니다.

기본 철학: **Google C++ Style 기반 + Modern C++20 + 주석이 필요 없는 읽히는 코드**

---

## ✅ Naming Conventions

### Classes / Structs

* PascalCase

```cpp
class ScreenCapture;
struct OcrResult;
```

### Functions / Methods

* PascalCase, 동사 기반, 역할 명확

```cpp
OcrResult RunOcr(const cv::Mat& frame);
bool HasTextChanged();
void RenderOverlay();
```

### Variables

* camelCase

```cpp
int frameCount;
bool hasTextChanged;
cv::Mat capturedFrame;
```

### Constants

* `kPascalCase`

```cpp
constexpr int kMaxFrameQueueSize = 5;
constexpr float kMinConfidence = 0.80f;
```

### Namespaces

* lower_snake_case

```cpp
namespace capture {
namespace ocr {
```

---

## ✅ File Naming & Extensions

### File Extensions

| 역할     | 확장자    |
| ------ | ------ |
| Header | `.hpp` |
| Source | `.cpp` |

### File Name Format

* snake_case

```
screen_capture.hpp
screen_capture.cpp
ocr_worker.hpp
ocr_worker.cpp
```


---

## ✅ Formatting Rules

| 항목        | 규칙                                              |
| ----------- | ------------------------------------------------- |
| Line length | ≤ 120 chars                                       |
| Indent      | Tab (4 space)
| Braces      | K&R (Google)                                      |
| Style       | `override`, `const`, `noexcept` where appropriate |

### Examples

```cpp
if (isActive) {
	ProcessFrame();
} else {
	HandleIdle();
}
```

```cpp
for (int i = 0; i < count; ++i) {
	Process(i);
}
```

### Pointer / Reference

```cpp
std::unique_ptr<OcrEngine> ocrEngine;
void Render(const Frame& frame);
```

---

## ✅ Memory & Ownership

* RAII
* `std::unique_ptr` 기본
* `std::shared_ptr`는 공유 목적 명확 시
* raw pointer 지양 (라이브러리 인터페이스 예외)

---

## ✅ Error Handling

* `std::optional` 또는 `std::expected` 사용

```cpp
std::optional<OcrResult> RunOcr(const Frame& frame);
```

* 예외 남용 금지

---

## ✅ Logging

* `spdlog`

```cpp
SPDLOG_DEBUG("Captured frame {}", frameId);
```

---

## ✅ Tests

* GoogleTest
* snake_case test names
* Given / When / Then

```cpp
TEST(CaptureTest, captures_frames_at_target_fps) {
	// Given
	// When
	// Then
}
```

---

## ✅ Comment Philosophy — "Self-Explanatory Code"

### Goal

코드는 주석 없이도 이해 가능해야 한다.

### Guidelines

* 명확한 이름
* 단일 책임 함수
* 구조로 이해를 돕기

### Comments Allowed Only For

* "왜" 설명이 필요한 경우
* 알고리즘/수학적 근거
* OS/라이브러리 제약
* TODO / FIXME (이유 포함)

### Avoid

* 코드 내용을 설명하는 주석

#### Good Example

```cpp
auto frame = capture->NextFrame();
if (!frame) return;

if (textTracker_.HasChanged(*frame)) {
	lastOcrResult_ = ocrEngine_.Run(*frame);
}

overlay_.Render(lastOcrResult_);
```

#### Avoid Example

```cpp
// get frame
auto frame = capture->NextFrame();
// if null return
if (!frame) return;
// run OCR if changed
...
```

---

## ✅ Git Rules

* 의미 있는 커밋 메시지
* 기능 단위 PR
* `clang-format` 적용 필수

---

## ✅ Summary

| 항목        | 규칙                                                 |
| ----------- | ---------------------------------------------------- |
| Naming      | PascalCase funcs/classes, camelCase vars, kConstants |
| File        | .hpp / .cpp, snake_case                              |
| Line length | 120 chars                                            |
| Memory      | RAII, smart pointers                                 |
| Errors      | optional / expected                                  |
| Comments    | Only when unavoidable                                |
| Tests       | GoogleTest + Given/When/Then                         |
| Logging     | spdlog                                               |

---


