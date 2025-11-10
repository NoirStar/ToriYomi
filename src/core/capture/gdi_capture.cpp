// ToriYomi - GDI 화면 캡처 구현
// GDI BitBlt를 사용한 폴백 화면 캡처

#include "core/capture/gdi_capture.h"
#include <Windows.h>

namespace toriyomi::capture {

// Pimpl 패턴으로 GDI 세부 구현 숨김
struct GdiCapture::Impl {
    HWND targetWindow{nullptr};
    bool initialized{false};

    // GDI 핸들
    HDC windowDC{nullptr};      // 윈도우 DC
    HDC memoryDC{nullptr};      // 메모리 DC (비트맵 복사용)
    HBITMAP bitmap{nullptr};    // 호환 비트맵
    HBITMAP oldBitmap{nullptr}; // 이전 비트맵 (복원용)

    // 캡처 영역 정보
    int width{0};
    int height{0};

    bool CreateCompatibleResources();
    void ReleaseResources();
};

GdiCapture::GdiCapture() : pImpl_(std::make_unique<Impl>()) {
}

GdiCapture::~GdiCapture() {
    Shutdown();
}

bool GdiCapture::Initialize(HWND targetWindow) {
    if (!targetWindow || !IsWindow(targetWindow)) {
        return false;
    }

    // 이미 초기화된 경우 먼저 정리
    if (pImpl_->initialized) {
        Shutdown();
    }

    pImpl_->targetWindow = targetWindow;

    // 윈도우 크기 가져오기
    RECT windowRect{};
    if (!GetClientRect(targetWindow, &windowRect)) {
        return false;
    }

    pImpl_->width = windowRect.right - windowRect.left;
    pImpl_->height = windowRect.bottom - windowRect.top;

    // 크기 검증
    if (pImpl_->width <= 0 || pImpl_->height <= 0) {
        return false;
    }

    // 윈도우 DC 가져오기
    pImpl_->windowDC = GetDC(targetWindow);
    if (!pImpl_->windowDC) {
        return false;
    }

    // 호환 리소스 생성
    if (!pImpl_->CreateCompatibleResources()) {
        pImpl_->ReleaseResources();
        return false;
    }

    pImpl_->initialized = true;
    return true;
}

cv::Mat GdiCapture::CaptureFrame() {
    if (!pImpl_->initialized) {
        return cv::Mat();
    }

    // BitBlt로 화면 복사
    // windowDC의 내용을 memoryDC의 비트맵으로 복사
    BOOL result = BitBlt(
        pImpl_->memoryDC,       // 대상 DC
        0, 0,                   // 대상 좌표
        pImpl_->width,          // 너비
        pImpl_->height,         // 높이
        pImpl_->windowDC,       // 소스 DC
        0, 0,                   // 소스 좌표
        SRCCOPY                 // 래스터 연산 (직접 복사)
    );

    if (!result) {
        return cv::Mat();
    }

    // 비트맵 정보 구조체 설정
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = pImpl_->width;
    bi.biHeight = -pImpl_->height;  // 음수 = top-down 비트맵 (OpenCV 호환)
    bi.biPlanes = 1;
    bi.biBitCount = 32;             // 32비트 BGRA
    bi.biCompression = BI_RGB;

    // 비트맵 데이터를 저장할 버퍼 생성
    std::vector<uint8_t> buffer(pImpl_->width * pImpl_->height * 4);

    // GetDIBits로 비트맵 데이터 추출
    int scanLines = GetDIBits(
        pImpl_->memoryDC,
        pImpl_->bitmap,
        0,
        pImpl_->height,
        buffer.data(),
        reinterpret_cast<BITMAPINFO*>(&bi),
        DIB_RGB_COLORS
    );

    if (scanLines == 0) {
        return cv::Mat();
    }

    // OpenCV Mat 생성 (BGRA → BGR 변환)
    cv::Mat bgraFrame(pImpl_->height, pImpl_->width, CV_8UC4, buffer.data());
    cv::Mat bgrFrame;
    cv::cvtColor(bgraFrame, bgrFrame, cv::COLOR_BGRA2BGR);

    // 깊은 복사 (buffer가 파괴되기 전에)
    return bgrFrame.clone();
}

void GdiCapture::Shutdown() {
    if (pImpl_) {
        pImpl_->ReleaseResources();
        pImpl_->initialized = false;
        pImpl_->targetWindow = nullptr;
        pImpl_->width = 0;
        pImpl_->height = 0;
    }
}

bool GdiCapture::IsInitialized() const {
    return pImpl_ && pImpl_->initialized;
}

// === Impl 메서드 구현 ===

bool GdiCapture::Impl::CreateCompatibleResources() {
    // 메모리 DC 생성 (windowDC와 호환)
    memoryDC = CreateCompatibleDC(windowDC);
    if (!memoryDC) {
        return false;
    }

    // 호환 비트맵 생성 (화면 복사용)
    bitmap = CreateCompatibleBitmap(windowDC, width, height);
    if (!bitmap) {
        return false;
    }

    // 비트맵을 메모리 DC에 선택
    oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDC, bitmap));
    if (!oldBitmap) {
        return false;
    }

    return true;
}

void GdiCapture::Impl::ReleaseResources() {
    // 리소스 해제 (역순으로)
    if (memoryDC && oldBitmap) {
        SelectObject(memoryDC, oldBitmap);
        oldBitmap = nullptr;
    }

    if (bitmap) {
        DeleteObject(bitmap);
        bitmap = nullptr;
    }

    if (memoryDC) {
        DeleteDC(memoryDC);
        memoryDC = nullptr;
    }

    if (windowDC && targetWindow) {
        ReleaseDC(targetWindow, windowDC);
        windowDC = nullptr;
    }
}

} // namespace toriyomi::capture
