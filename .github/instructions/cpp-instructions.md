# C++ Code Generation Instructions for ToriYomi

## Project Context

You are working on **ToriYomi**, a real-time Japanese OCR overlay tool for Windows. The project uses:
- **C++20** with modern idioms (RAII, smart pointers, STL algorithms)
- **Qt 6** for UI (Widgets, transparent overlay windows)
- **Tesseract** for Japanese OCR
- **OpenCV** for image processing
- **CMake + Ninja** for builds
- **Windows-specific APIs** (DXGI/GDI for screen capture, DWM for layered windows)

**Performance is critical**: Target 30+ FPS capture, ≤200ms OCR latency, minimal CPU/memory overhead.

---

## Code Style Rules

### Naming Conventions

- **Classes/Structs**: `PascalCase` (e.g., `ScreenCapture`, `OcrResult`)
- **Functions/Methods**: `PascalCase`, verb-based (e.g., `RunOcr()`, `HasTextChanged()`, `RenderOverlay()`)
- **Variables**: `camelCase` (e.g., `frameCount`, `hasTextChanged`, `capturedFrame`)
- **Constants**: `kPascalCase` (e.g., `kMaxFrameQueueSize`, `kMinConfidence`)
- **Member variables**: Suffix with `_` (e.g., `frameQueue_`, `ocrEngine_`)
- **Namespaces**: `lower_snake_case` (e.g., `namespace capture`, `namespace ocr`)

### File Naming

- **Headers**: `.hpp` extension, `snake_case` (e.g., `screen_capture.hpp`)
- **Sources**: `.cpp` extension, `snake_case` (e.g., `screen_capture.cpp`)

### Formatting

- **Line length**: ≤ 120 characters
- **Braces**: K&R style (opening brace on same line)
- **Spacing**: Space after control keywords (`if (`, `for (`, `while (`), no space before `(`

Example:
```cpp
if (isActive) {
	ProcessFrame();
} else {
	HandleIdle();
}
```

---

## Language Features & Best Practices

### Memory Management

- **ALWAYS use RAII**: No manual `new`/`delete`
- **Prefer `std::unique_ptr`** for exclusive ownership
- **Use `std::shared_ptr`** only when sharing is genuinely needed
- **Avoid raw pointers** except for non-owning references or library interfaces

```cpp
// Good
auto capture = std::make_unique<ScreenCapture>();

// Avoid
ScreenCapture* capture = new ScreenCapture();  // Don't do this
```

### Error Handling

- **Prefer `std::optional<T>`** for operations that may fail without exceptional circumstances
- **Use exceptions sparingly**: Only for truly exceptional errors (initialization failures, resource exhaustion)
- **Return status via return value**, not out parameters

```cpp
// Good
std::optional<Frame> CaptureFrame();

// Avoid
bool CaptureFrame(Frame* outFrame);  // Avoid out parameters
```

### Threading & Concurrency

- **Use `std::thread` or `std::jthread`** for threads
- **Always protect shared data** with `std::mutex` or lock-free structures
- **Use condition variables** for producer-consumer patterns
- **Prefer lock-free queues** where performance is critical
- **NEVER block the UI thread**: Offload heavy work to background threads

```cpp
// Good pattern
ThreadSafeQueue<Frame> frameQueue_;
std::jthread captureThread_;

void CaptureLoop() {
	while (running_) {
		auto frame = CaptureFrame();
		if (frame) frameQueue_.push(*frame);
	}
}
```

### Modern C++ Idioms

- **Use `auto`** for type deduction when type is obvious or verbose
- **Prefer range-based for loops**
- **Use structured bindings** (C++17) for pairs/tuples
- **Use `constexpr`** for compile-time constants
- **Mark functions `noexcept`** when they don't throw
- **Use `override`** keyword for virtual function overrides

```cpp
// Good
for (const auto& box : ocrResult.boxes) {
	ProcessBox(box);
}

auto [width, height] = GetDimensions();

constexpr int kMaxRetries = 3;
```

---

## Comments & Documentation

**Philosophy**: Code should be self-explanatory. Write code that doesn't need comments.

### Comment Language

**CRITICAL**: All comments must be written in **Korean (한글)**.

```cpp
// ✅ Good: Korean comments
// 하이브리드 GPU 시스템에서 DXGI 실패 가능성 있음 - GDI로 폴백
if (!dxgiCapture_->Initialize()) {
	SPDLOG_WARN("DXGI initialization failed, falling back to GDI");
	capture_ = std::make_unique<GdiCapture>();
}

// TODO: 프레임 드롭 감지 로직 추가 필요 (성능 모니터링용)

// ❌ Bad: English comments
// DXGI may fail on hybrid GPU systems; fall back to GDI
```

### When to Comment

✅ **DO comment** (in Korean):
- **Why** you made a non-obvious decision (왜 이렇게 했는지)
- Algorithm references or mathematical formulas (알고리즘 설명, 수식)
- OS/library-specific constraints or workarounds (OS/라이브러리 제약사항)
- `TODO`/`FIXME` with reasoning (이유 포함)
- Performance-critical sections (성능 크리티컬 구간)

❌ **DON'T comment**:
- What the code does (should be obvious from names/structure)
- Redundant information that just repeats the code

### Examples

```cpp
// ❌ Bad: Redundant comment
// 다음 프레임을 가져옴
auto frame = capture->NextFrame();
// 프레임이 null인지 확인
if (!frame) return;

// ✅ Good: Self-explanatory code (no comment needed)
auto frame = capture->NextFrame();
if (!frame) return;

if (textTracker_.HasChanged(*frame)) {
	lastOcrResult_ = ocrEngine_.Run(*frame);
}

// ✅ Good: Explains "why" in Korean
// 하이브리드 GPU 노트북에서 DXGI Desktop Duplication API가
// 실패할 수 있음 (Intel iGPU + NVIDIA dGPU 조합)
// GDI BitBlt로 폴백하여 호환성 확보
if (!dxgiCapture_->Initialize()) {
	SPDLOG_WARN("DXGI initialization failed, falling back to GDI");
	capture_ = std::make_unique<GdiCapture>();
}

// 프레임 재사용으로 메모리 할당 최소화 (30+ FPS 목표)
cv::Mat frameBuffer_;
void CaptureFrame() {
	CaptureInto(frameBuffer_);
}
```

---

## Qt-Specific Guidelines

### UI Files

- **Use Qt Designer** for `.ui` files (XML)
- **Load UI in C++** using `setupUi()`
- **Keep business logic separate** from UI code

### Slots & Signals

- **Use Qt 5+ connection syntax** (lambda or function pointers)
- **Avoid old-style SIGNAL/SLOT macros**

```cpp
// Good
connect(button, &QPushButton::clicked, this, [this]() {
	OnButtonClicked();
});

// Avoid
connect(button, SIGNAL(clicked()), this, SLOT(OnButtonClicked()));
```

### Transparent Overlay Windows

- **Set window flags** for layered, always-on-top windows
- **Use `WS_EX_LAYERED` and `WS_EX_TRANSPARENT`** via `winId()`
- **Forward input** to underlying windows as needed

---

## Testing

### Test Structure

- **Use GoogleTest** (`TEST`, `EXPECT_*`, `ASSERT_*`)
- **Name tests descriptively** in `snake_case`
- **Follow Given/When/Then** structure in comments or code blocks

```cpp
TEST(ScreenCaptureTest, captures_frames_at_30fps) {
	// Given
	auto capture = std::make_unique<ScreenCapture>();
	capture->SetTargetFPS(30);

	// When
	auto startTime = std::chrono::steady_clock::now();
	std::vector<Frame> frames;
	for (int i = 0; i < 60; ++i) {
		frames.push_back(capture->CaptureFrame());
	}
	auto elapsed = std::chrono::steady_clock::now() - startTime;

	// Then
	auto measuredFPS = 60.0 / std::chrono::duration<double>(elapsed).count();
	EXPECT_GE(measuredFPS, 30.0);
}
```

### Test Coverage

- **Unit test each module** in isolation
- **Mock external dependencies** (Tesseract, Windows APIs)
- **Test edge cases**: empty inputs, null pointers, timeouts

---

## Logging

- **Use spdlog** for all logging
- **Levels**: `SPDLOG_TRACE`, `SPDLOG_DEBUG`, `SPDLOG_INFO`, `SPDLOG_WARN`, `SPDLOG_ERROR`
- **Log performance metrics** at DEBUG level
- **Log errors with context**: Include file paths, error codes, etc.

```cpp
SPDLOG_DEBUG("Captured frame {} at {}ms", frameId, latencyMs);
SPDLOG_ERROR("Failed to initialize Tesseract: {}", errorMsg);
```

---

## Performance Considerations

### Critical Paths

- **Screen capture loop**: Must run at 30+ FPS → avoid allocations, minimize copies
- **OCR processing**: Heavy; run in separate thread with queue
- **Overlay rendering**: Must complete in <16ms for smooth 60Hz updates

### Optimization Strategies

- **Reuse buffers**: Don't allocate `cv::Mat` every frame
- **Skip unchanged frames**: Use frame differencing to detect changes
- **Profile before optimizing**: Use Visual Studio Profiler or Tracy

```cpp
// Good: Reuse buffer
cv::Mat frameBuffer_;
void CaptureFrame() {
	CaptureInto(frameBuffer_);  // Reuse existing buffer
}

// Avoid: Allocate every frame
cv::Mat CaptureFrame() {
	return cv::Mat(...);  // New allocation each time
}
```

---

## Windows-Specific APIs

### Screen Capture

- **Prefer DXGI Desktop Duplication API** (fastest, lowest latency)
- **Fallback to GDI `BitBlt`** if DXGI fails (e.g., hybrid GPU laptops)
- **Use `SetProcessDPIAware()`** to handle high-DPI displays correctly

### Overlay Windows

- **Use `SetWindowLong` with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST`**
- **Set transparency** with `SetLayeredWindowAttributes`
- **Forward mouse events** to underlying window if click-through is needed

---

## Code Generation Guidelines

When generating code:

1. **Follow the style guide strictly**: PascalCase functions, camelCase variables, 2-space indent
2. **Include error handling**: Check return values, use `std::optional` where appropriate
3. **Write self-documenting code**: Clear names, single-responsibility functions
4. **Add logging**: Use `spdlog` for key events and errors
5. **Make it testable**: Inject dependencies, avoid global state
6. **Optimize for readability first**: Premature optimization is the root of all evil
7. **Use modern C++20 features**: Ranges, concepts, `std::jthread`, etc.
8. **Write comments in Korean (한글)**: Only when necessary to explain "why"

---

## Example: Complete Module

```cpp
// screen_capture.hpp
#pragma once
#include <optional>
#include <memory>
#include "core/Types.hpp"

namespace ToriYomi {

class ScreenCapture {
public:
	virtual ~ScreenCapture() = default;
	
	virtual bool Initialize() = 0;
	virtual std::optional<Frame> CaptureFrame() = 0;
	virtual void SetTargetWindow(HWND hwnd) = 0;
};

std::unique_ptr<ScreenCapture> CreateScreenCapture();

} // namespace ToriYomi
```

```cpp
// screen_capture.cpp
#include "core/capture/screen_capture.hpp"
#include "core/capture/dxgi_capture.hpp"
#include "core/capture/gdi_capture.hpp"
#include <spdlog/spdlog.h>

namespace ToriYomi {

std::unique_ptr<ScreenCapture> CreateScreenCapture() {
	auto dxgiCapture = std::make_unique<DxgiCapture>();
	
	if (dxgiCapture->Initialize()) {
		SPDLOG_INFO("Using DXGI capture backend");
		return dxgiCapture;
	}
	
	SPDLOG_WARN("DXGI initialization failed, falling back to GDI");
	auto gdiCapture = std::make_unique<GdiCapture>();
	
	if (!gdiCapture->Initialize()) {
		SPDLOG_ERROR("Failed to initialize GDI capture");
		return nullptr;
	}
	
	return gdiCapture;
}

} // namespace ToriYomi
```

---

## Summary Checklist

When writing code for ToriYomi, ensure:

- ✅ Naming follows conventions (PascalCase functions, camelCase vars, kConstants)
- ✅ Files use `.hpp`/`.cpp` with `snake_case` names
- ✅ Memory managed via RAII and smart pointers
- ✅ Error handling uses `std::optional` or exceptions (sparingly)
- ✅ Threading is safe (mutexes, lock-free queues)
- ✅ UI thread never blocks
- ✅ Code is self-explanatory (minimal comments)
- ✅ Comments are in Korean (한글) when needed
- ✅ Tests follow Given/When/Then structure
- ✅ Logging uses `spdlog`
- ✅ Performance-critical paths are optimized (no unnecessary allocations)

---

**Remember**: Readability and maintainability first. Optimize only when profiling shows a bottleneck.

