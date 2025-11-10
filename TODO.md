# ToriYomi 개발 진행 상황

**최종 업데이트**: 2025-11-10

---

## ✅ 완료된 단계

### Phase 1-1: FrameQueue 구현 ✅
**완료일**: 2025-11-10  
**파일**: 
- `src/core/capture/frame_queue.h`
- `src/core/capture/frame_queue.cpp`
- `tests/unit/test_frame_queue.cpp`

**구현 내용**:
- 스레드 안전한 프레임 큐 (최대 5개 프레임)
- `std::mutex` + `std::condition_variable` 사용
- FIFO 순서 보장
- 오버플로우 시 가장 오래된 프레임 자동 삭제
- 타임아웃 지원하는 Pop 연산

**테스트 결과**: 8개 테스트 모두 통과 ✅

---

### Phase 1-2: DXGI 화면 캡처 구현 ✅
**완료일**: 2025-11-10  
**파일**:
- `src/core/capture/dxgi_capture.h`
- `src/core/capture/dxgi_capture.cpp`
- `tests/unit/test_dxgi_capture.cpp`

**구현 내용**:
- DirectX 11 Desktop Duplication API 사용
- Pimpl 패턴으로 DXGI 세부 구현 캡슐화
- 스테이징 텍스처를 통한 CPU 접근
- BGRA → BGR 변환 (OpenCV 호환)
- 100ms 타임아웃 지원
- 접근 손실(ACCESS_LOST) 시 자동 정리

**성능**: **141 FPS** 달성! 🚀

**테스트 결과**: 8개 테스트 모두 통과 ✅

**핵심 코드 구조**:
```cpp
// 초기화
bool Initialize(HWND targetWindow);

// 프레임 캡처 (블로킹, 100ms 타임아웃)
cv::Mat CaptureFrame();

// 정리
void Shutdown();
```

---

### Phase 1-3: GDI 폴백 캡처 구현 ✅
**완료일**: 2025-11-10  
**파일**:
- `src/core/capture/gdi_capture.h`
- `src/core/capture/gdi_capture.cpp`
- `tests/unit/test_gdi_capture.cpp`

**구현 내용**:
- GDI BitBlt를 사용한 화면 캡처
- DXGI 미지원 환경을 위한 폴백
- Device Context + 호환 비트맵 사용
- BGRA → BGR 변환
- 모든 Windows 환경에서 작동

**성능**: **44 FPS** 달성 (DXGI보다 느리지만 충분)

**테스트 결과**: 9개 테스트 모두 통과 ✅

**핵심 코드 구조**:
```cpp
// GDI 리소스 생성
bool Initialize(HWND targetWindow);

// BitBlt로 화면 복사
cv::Mat CaptureFrame();

// GDI 리소스 해제
void Shutdown();
```

**기술 설명**:
1. **윈도우 DC 가져오기**: `GetDC(targetWindow)`
2. **메모리 DC 생성**: `CreateCompatibleDC(windowDC)`
3. **호환 비트맵 생성**: `CreateCompatibleBitmap(windowDC, width, height)`
4. **화면 복사**: `BitBlt(memoryDC, 0, 0, width, height, windowDC, 0, 0, SRCCOPY)`
5. **비트맵 데이터 추출**: `GetDIBits()`
6. **OpenCV Mat 변환**: BGRA → BGR

---

### Phase 1-4: 캡처 스레드 통합 ✅
**완료일**: 2025-11-10  
**파일**:
- `src/core/capture/capture_thread.h`
- `src/core/capture/capture_thread.cpp`
- `tests/unit/test_capture_thread.cpp`

**구현 내용**:
- 백그라운드 스레드에서 DXGI/GDI 자동 선택 후 캡처
- FrameQueue에 프레임 전달
- 히스토그램 비교를 통한 변경 감지 (0.95 유사도 임계값)
- 변경되지 않은 프레임 스킵으로 CPU 절약
- FPS 통계 추적 (1초 단위 업데이트)
- 스레드 안전한 시작/정지

**성능**: DXGI 모드에서 **32 FPS** (변경 감지 필터링 적용)

**테스트 결과**: 3개 테스트 모두 통과 ✅

**핵심 코드 구조**:
```cpp
// 캡처 시작
bool Start(HWND targetWindow);

// 캡처 정지
void Stop();

// 통계 조회
Statistics GetStatistics();

// 캡처 루프 (백그라운드 스레드)
void CaptureLoop();

// 프레임 변경 감지 (히스토그램 상관계수)
bool HasFrameChanged(const cv::Mat& frame);
```

**기술 설명**:
1. **DXGI 우선 시도**: Initialize 성공하면 DXGI 사용
2. **GDI 자동 폴백**: DXGI 실패 시 GDI로 전환
3. **변경 감지**: `cv::compareHist(hist1, hist2, HISTCMP_CORREL)`
4. **프레임 스킵**: 상관계수 > 0.95이면 동일 프레임으로 판단
5. **FPS 계산**: 1초마다 `capturedCount / elapsedTime`

---

## 🔄 진행 중

없음 (Phase 1 완료!)

---

## 📋 대기 중

### Phase 2-1: Tesseract 래퍼 구현 (다음 작업)
- Tesseract OCR API 래핑
- 일본어 모델 (jpn) 사용
- TextSegment 구조체 출력

### Phase 2-2: OCR 스레드 구현
- OcrThread 클래스
- FrameQueue에서 프레임 소비
- 텍스트 추출 및 좌표 저장

### Phase 3-1: 일본어 토크나이저 구현
- MeCab 통합 또는 사전 기반 분석
- 한자/히라가나/가타카나 분리
- 읽기 정보 추출

### Phase 3-2: 후리가나 매퍼 구현
- 한자에 대한 후리가나 생성
- 화면 좌표 매핑
- FuriganaSegment 구조체

---

## 📊 전체 진행률

```
Phase 1 (캡처): ████████ 100% (4/4) ✅
Phase 2 (OCR):  ░░░░░░░░   0% (0/2)
Phase 3 (토큰): ░░░░░░░░   0% (0/2)
Phase 4 (UI):   ░░░░░░░░   0% (0/2)
```

**전체**: 4/14 완료 (29%)
