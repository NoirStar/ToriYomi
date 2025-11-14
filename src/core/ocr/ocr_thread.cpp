// ToriYomi - OCR 스레드 구현
// FrameQueue에서 프레임을 받아 OCR 처리

#include "ocr_thread.h"
#include <chrono>

namespace toriyomi {
namespace ocr {

OcrThread::OcrThread(std::shared_ptr<FrameQueue> frameQueue,
                     std::shared_ptr<IOcrEngine> ocrEngine)
    : frameQueue_(frameQueue)
    , ocrEngine_(ocrEngine)  // shared_ptr 복사 (참조 카운트 증가)
    , lastFpsUpdate_(std::chrono::steady_clock::now()) {
    
    if (ocrEngine_) {
        stats_.engineName = ocrEngine_->GetEngineName();
    }
}

OcrThread::~OcrThread() {
    Stop();
}

bool OcrThread::Start() {
    // OCR 엔진 초기화 확인
    if (!ocrEngine_ || !ocrEngine_->IsInitialized()) {
        return false;
    }

    // 이미 실행 중이면 실패
    if (running_) {
        return false;
    }

    // 스레드 시작
    running_ = true;
    ocrThread_ = std::thread(&OcrThread::OcrLoop, this);

    return true;
}

void OcrThread::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (ocrThread_.joinable()) {
        ocrThread_.join();
    }
}

bool OcrThread::IsRunning() const {
    return running_;
}

std::vector<TextSegment> OcrThread::GetLatestResults() const {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return latestResults_;
}

OcrStatistics OcrThread::GetStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void OcrThread::OcrLoop() {
    while (running_) {
        auto frameOpt = frameQueue_->Pop(100);
        
        if (!frameOpt.has_value()) {
            continue;
        }

        if (!running_) {
            break;
        }

        cv::Mat frame = frameOpt.value();
        auto results = ocrEngine_->RecognizeText(frame);
        
        if (!running_) {
            break;
        }
        
        {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            latestResults_ = results;
        }

        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.totalFramesProcessed++;
            stats_.totalTextSegments += results.size();
            framesProcessedSinceLastUpdate_++;
        }

        UpdateFps();
    }
}

void OcrThread::UpdateFps() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastFpsUpdate_).count();

    // 1초 이상 경과했으면 FPS 계산
    if (elapsed >= 1000) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        
        double elapsedSeconds = elapsed / 1000.0;
        stats_.currentFps = framesProcessedSinceLastUpdate_ / elapsedSeconds;

        // 리셋
        framesProcessedSinceLastUpdate_ = 0;
        lastFpsUpdate_ = now;
    }
}

}  // namespace ocr
}  // namespace toriyomi
