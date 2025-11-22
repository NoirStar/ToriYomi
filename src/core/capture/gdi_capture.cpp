// ToriYomi - GDI 화면 캡처 구현
// GDI BitBlt를 사용한 폴백 화면 캡처

#include "core/capture/gdi_capture.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace {

bool IsFrameNearlyBlack(const cv::Mat& frame) {
    if (frame.empty()) {
        return true;
    }

    cv::Scalar meanScalar;
    cv::Scalar stddevScalar;
    cv::meanStdDev(frame, meanScalar, stddevScalar);
    const double maxMean = std::max({meanScalar[0], meanScalar[1], meanScalar[2]});
    const double maxStdDev = std::max({stddevScalar[0], stddevScalar[1], stddevScalar[2]});
    return maxMean < 2.5 && maxStdDev < 1.5;
}

} // namespace

namespace toriyomi::capture {

// Pimpl 패턴으로 GDI 세부 구현 숨김
struct GdiCapture::Impl {
    HWND targetWindow{nullptr};
    bool initialized{false};
    bool preferPrintWindow{false};

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

    auto tryBitBlt = [this]() -> bool {
        return BitBlt(
            pImpl_->memoryDC,
            0,
            0,
            pImpl_->width,
            pImpl_->height,
            pImpl_->windowDC,
            0,
            0,
            SRCCOPY
        );
    };

    auto tryPrintWindow = [this](bool requestFullContent) -> bool {
        if (!pImpl_->targetWindow || pImpl_->targetWindow == GetDesktopWindow()) {
            return false;
        }

        UINT flags = PW_CLIENTONLY;
        if (requestFullContent) {
            flags |= PW_RENDERFULLCONTENT;
        }

        if (PrintWindow(pImpl_->targetWindow, pImpl_->memoryDC, flags) != FALSE) {
            return true;
        }

        if (requestFullContent) {
            return PrintWindow(pImpl_->targetWindow, pImpl_->memoryDC, PW_CLIENTONLY) != FALSE;
        }

        return false;
    };

    const bool canUsePrintWindow = pImpl_->targetWindow && pImpl_->targetWindow != GetDesktopWindow();

    bool lastCaptureUsedPrintWindow = false;

    auto captureWithBitBlt = [&]() -> bool {
        lastCaptureUsedPrintWindow = false;
        return tryBitBlt();
    };

    auto captureWithPrintWindow = [&]() -> bool {
        lastCaptureUsedPrintWindow = true;
        return tryPrintWindow(true);
    };

    auto extractCapturedFrame = [&]() -> cv::Mat {
        BITMAPINFOHEADER bi{};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = pImpl_->width;
        bi.biHeight = -pImpl_->height;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        std::vector<uint8_t> buffer(static_cast<size_t>(pImpl_->width) * static_cast<size_t>(pImpl_->height) * 4);
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

        cv::Mat bgraFrame(pImpl_->height, pImpl_->width, CV_8UC4, buffer.data());
        cv::Mat bgrFrame;
        cv::cvtColor(bgraFrame, bgrFrame, cv::COLOR_BGRA2BGR);

        if (lastCaptureUsedPrintWindow && IsFrameNearlyBlack(bgrFrame)) {
            return cv::Mat();
        }

        return bgrFrame.clone();
    };

    auto captureAndExtract = [&](auto captureFunc) -> cv::Mat {
        if (!captureFunc()) {
            return cv::Mat();
        }
        return extractCapturedFrame();
    };

    cv::Mat frame;
    if (pImpl_->preferPrintWindow && canUsePrintWindow) {
        frame = captureAndExtract(captureWithPrintWindow);
        if (frame.empty()) {
            frame = captureAndExtract(captureWithBitBlt);
        }
    } else {
        frame = captureAndExtract(captureWithBitBlt);
        if (frame.empty() && canUsePrintWindow) {
            frame = captureAndExtract(captureWithPrintWindow);
            if (frame.empty()) {
                frame = captureAndExtract(captureWithBitBlt);
            }
        }
    }

    return frame;
}

void GdiCapture::SetPreferPrintWindow(bool enable) {
    if (pImpl_) {
        pImpl_->preferPrintWindow = enable;
    }
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
