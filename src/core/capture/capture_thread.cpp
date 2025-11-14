// ToriYomi - 캡처 스레드 구현
// DXGI/GDI 자동 선택 및 백그라운드 캡처

#include "core/capture/capture_thread.h"
#include "core/capture/dxgi_capture.h"
#include "core/capture/gdi_capture.h"
#include <thread>
#include <chrono>
#include <atomic>

namespace toriyomi::capture {

// Pimpl 구현
struct CaptureThread::Impl {
    std::shared_ptr<FrameQueue> frameQueue;
    std::unique_ptr<std::thread> captureThread;
    std::atomic<bool> running{false};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> changeDetectionEnabled{false};

    // 캡처 인터페이스 (DXGI 또는 GDI)
    std::unique_ptr<DxgiCapture> dxgiCapture;
    std::unique_ptr<GdiCapture> gdiCapture;
    bool usingDxgi{false};

    // 통계
    std::atomic<uint64_t> totalFramesCaptured{0};
    std::atomic<uint64_t> framesSkipped{0};
    std::atomic<double> currentFps{0.0};

    // 프레임 변경 감지용
    cv::Mat previousFrame;
    cv::Mat previousHistogram;

    // 대상 윈도우
    HWND targetWindow{nullptr};

    void CaptureLoop();
    bool InitializeCapture();
    bool CaptureFrame(cv::Mat& outFrame);
    bool HasFrameChanged(const cv::Mat& frame);
    void UpdateFps();

    // FPS 계산용
    std::chrono::steady_clock::time_point fpsStartTime;
    uint64_t fpsFrameCount{0};
};

CaptureThread::CaptureThread(std::shared_ptr<FrameQueue> frameQueue)
    : pImpl_(std::make_unique<Impl>()) {
    pImpl_->frameQueue = frameQueue;
}

CaptureThread::~CaptureThread() {
    Stop();
}

bool CaptureThread::Start(HWND targetWindow) {
    if (pImpl_->running) {
        return false; // 이미 실행 중
    }

    if (!targetWindow || !IsWindow(targetWindow)) {
        return false;
    }

    pImpl_->targetWindow = targetWindow;
    pImpl_->stopRequested = false;
    
    // DXGI 먼저 시도
    pImpl_->dxgiCapture = std::make_unique<DxgiCapture>();
    if (pImpl_->dxgiCapture->Initialize(targetWindow)) {
        pImpl_->usingDxgi = true;
    } else {
        // DXGI 실패 시 GDI로 폴백
        pImpl_->dxgiCapture.reset();
        pImpl_->gdiCapture = std::make_unique<GdiCapture>();
        if (!pImpl_->gdiCapture->Initialize(targetWindow)) {
            pImpl_->gdiCapture.reset();
            return false;
        }
        pImpl_->usingDxgi = false;
    }

    // 스레드 시작
    pImpl_->running = true;
    pImpl_->fpsStartTime = std::chrono::steady_clock::now();
    pImpl_->captureThread = std::make_unique<std::thread>(&Impl::CaptureLoop, pImpl_.get());

    return true;
}

void CaptureThread::Stop() {
    if (!pImpl_->running) {
        return;
    }

    pImpl_->stopRequested = true;

    // 스레드 종료 대기
    if (pImpl_->captureThread && pImpl_->captureThread->joinable()) {
        pImpl_->captureThread->join();
    }

    // 리소스 정리
    if (pImpl_->dxgiCapture) {
        pImpl_->dxgiCapture->Shutdown();
        pImpl_->dxgiCapture.reset();
    }
    if (pImpl_->gdiCapture) {
        pImpl_->gdiCapture->Shutdown();
        pImpl_->gdiCapture.reset();
    }

    pImpl_->running = false;
    pImpl_->captureThread.reset();
}

bool CaptureThread::IsRunning() const {
    return pImpl_->running;
}

void CaptureThread::SetChangeDetection(bool enable) {
    pImpl_->changeDetectionEnabled = enable;
    if (!enable) {
        pImpl_->previousFrame.release();
        pImpl_->previousHistogram.release();
    }
}

CaptureStatistics CaptureThread::GetStatistics() const {
    CaptureStatistics stats;
    stats.totalFramesCaptured = pImpl_->totalFramesCaptured;
    stats.framesSkipped = pImpl_->framesSkipped;
    stats.currentFps = pImpl_->currentFps;
    stats.usingDxgi = pImpl_->usingDxgi;
    return stats;
}

// === Impl 메서드 구현 ===

void CaptureThread::Impl::CaptureLoop() {
    while (!stopRequested) {
        cv::Mat frame;
        
        // 프레임 캡처
        bool captured = CaptureFrame(frame);
        
        if (!captured || frame.empty()) {
            // 캡처 실패 - 짧은 대기 후 재시도
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 변경 감지가 활성화된 경우 중복 프레임 스킵
        if (changeDetectionEnabled && !HasFrameChanged(frame)) {
            framesSkipped++;
            continue;
        }

        // 프레임을 큐에 푸시 (FrameQueue 내부에서 clone 수행)
        frameQueue->Push(frame);
        totalFramesCaptured++;
        fpsFrameCount++;

        // FPS 업데이트 (1초마다)
        UpdateFps();

        // CPU 부하 줄이기 (약 60 FPS로 제한)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

bool CaptureThread::Impl::CaptureFrame(cv::Mat& outFrame) {
    if (usingDxgi && dxgiCapture) {
        outFrame = dxgiCapture->CaptureFrame();
        return !outFrame.empty();
    } else if (gdiCapture) {
        outFrame = gdiCapture->CaptureFrame();
        return !outFrame.empty();
    }
    return false;
}

bool CaptureThread::Impl::HasFrameChanged(const cv::Mat& frame) {
    if (previousFrame.empty()) {
        // 첫 프레임 - 항상 변경됨
        previousFrame = frame.clone();
        
        // 히스토그램 계산 (그레이스케일)
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        
        int histSize = 256;
        float range[] = {0, 256};
        const float* histRange = {range};
        cv::calcHist(&gray, 1, 0, cv::Mat(), previousHistogram, 1, &histSize, &histRange);
        cv::normalize(previousHistogram, previousHistogram, 0, 1, cv::NORM_MINMAX);
        
        return true;
    }

    // 현재 프레임의 히스토그램 계산
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat currentHistogram;
    cv::calcHist(&gray, 1, 0, cv::Mat(), currentHistogram, 1, &histSize, &histRange);
    cv::normalize(currentHistogram, currentHistogram, 0, 1, cv::NORM_MINMAX);

    // 히스토그램 비교 (Correlation 방법)
    double similarity = cv::compareHist(previousHistogram, currentHistogram, cv::HISTCMP_CORREL);

    // 유사도가 0.95 이상이면 변경 없음으로 간주
    if (similarity > 0.95) {
        return false;
    }

    // 프레임이 변경됨 - 저장
    previousFrame = frame.clone();
    previousHistogram = currentHistogram.clone();
    
    return true;
}

void CaptureThread::Impl::UpdateFps() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsStartTime);
    
    // 1초 이상 경과 시 FPS 계산
    if (elapsed.count() >= 1000) {
        currentFps = (fpsFrameCount * 1000.0) / elapsed.count();
        fpsFrameCount = 0;
        fpsStartTime = now;
    }
}

} // namespace toriyomi::capture
