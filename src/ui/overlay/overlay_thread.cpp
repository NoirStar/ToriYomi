// ToriYomi - 오버레이 렌더링 스레드 구현
#include "overlay_thread.h"
#include <algorithm>

namespace toriyomi {
namespace ui {

// ============================================================================
// FuriganaBuffer 구현
// ============================================================================

FuriganaBuffer::FuriganaBuffer()
    : frontIndex_(0)
    , hasUpdate_(false) {
}

void FuriganaBuffer::Update(const std::vector<tokenizer::FuriganaInfo>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 백 버퍼에 데이터 쓰기
    int backIndex = 1 - frontIndex_.load(std::memory_order_relaxed);
    buffer_[backIndex] = data;
    
    // 버퍼 스왑 (원자적 연산)
    frontIndex_.store(backIndex, std::memory_order_release);
    hasUpdate_.store(true, std::memory_order_release);
}

std::vector<tokenizer::FuriganaInfo> FuriganaBuffer::Get() const {
    // Lock-free 읽기
    int index = frontIndex_.load(std::memory_order_acquire);
    
    // 읽기 중에는 뮤텍스 필요 (복사 안전성)
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_[index];
}

bool FuriganaBuffer::HasUpdate() {
    return hasUpdate_.exchange(false, std::memory_order_acq_rel);
}

// ============================================================================
// OverlayThread 구현
// ============================================================================

OverlayThread::OverlayThread()
    : window_(nullptr)
    , running_(false)
    , frameCount_(0)
    , updateCount_(0)
    , averageFps_(0.0)
    , framesSinceLastUpdate_(0) {
}

OverlayThread::~OverlayThread() {
    Stop();
}

bool OverlayThread::Start(int x, int y, int width, int height) {
    if (running_.load()) {
        return true;  // 이미 실행 중
    }

    // 오버레이 윈도우 생성
    window_ = std::make_unique<OverlayWindow>();
    if (!window_->Create(x, y, width, height)) {
        window_.reset();
        return false;
    }

    // 통계 초기화
    frameCount_.store(0);
    updateCount_.store(0);
    averageFps_.store(0.0);
    framesSinceLastUpdate_ = 0;
    lastFpsUpdate_ = std::chrono::steady_clock::now();

    // 렌더링 스레드 시작
    running_.store(true);
    renderThread_ = std::thread(&OverlayThread::RenderLoop, this);

    return true;
}

void OverlayThread::Stop() {
    if (!running_.load()) {
        return;
    }

    // 스레드 정지 신호
    running_.store(false);

    // 스레드 종료 대기
    if (renderThread_.joinable()) {
        renderThread_.join();
    }

    // 윈도우 파괴
    if (window_) {
        window_->Destroy();
        window_.reset();
    }
}

bool OverlayThread::IsRunning() const {
    return running_.load();
}

void OverlayThread::UpdateFurigana(const std::vector<tokenizer::FuriganaInfo>& furiganaList) {
    furiganaBuffer_.Update(furiganaList);
    updateCount_.fetch_add(1, std::memory_order_relaxed);
}

OverlayThread::Stats OverlayThread::GetStats() const {
    Stats stats;
    stats.frameCount = frameCount_.load(std::memory_order_relaxed);
    stats.updateCount = updateCount_.load(std::memory_order_relaxed);
    stats.averageFps = averageFps_.load(std::memory_order_relaxed);
    return stats;
}

void OverlayThread::RenderLoop() {
    // 60 FPS = 16.666ms per frame
    const auto targetFrameTime = std::chrono::microseconds(16667);
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (running_.load()) {
        auto frameStart = std::chrono::steady_clock::now();

        // 윈도우 메시지 처리
        if (!window_->ProcessMessages()) {
            // WM_QUIT 받음
            break;
        }

        // 후리가나 업데이트가 있으면 적용
        if (furiganaBuffer_.HasUpdate()) {
            auto furiganaData = furiganaBuffer_.Get();
            window_->UpdateFurigana(furiganaData);
        }

        // 윈도우 다시 그리기
        window_->Redraw();

        // 프레임 카운트 증가
        frameCount_.fetch_add(1, std::memory_order_relaxed);
        framesSinceLastUpdate_++;

        // FPS 계산 (1초마다)
        UpdateFps();

        // 프레임 타이밍 조정 (60 FPS 유지)
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameDuration = frameEnd - frameStart;
        
        if (frameDuration < targetFrameTime) {
            // 남은 시간만큼 대기
            std::this_thread::sleep_for(targetFrameTime - frameDuration);
        }

        lastFrameTime = frameStart;
    }
}

void OverlayThread::UpdateFps() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsUpdate_);

    // 1초마다 FPS 업데이트
    if (elapsed.count() >= 1000) {
        double fps = framesSinceLastUpdate_ * 1000.0 / elapsed.count();
        averageFps_.store(fps, std::memory_order_relaxed);
        
        framesSinceLastUpdate_ = 0;
        lastFpsUpdate_ = now;
    }
}

}  // namespace ui
}  // namespace toriyomi
