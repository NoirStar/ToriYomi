// ToriYomi - 오버레이 렌더링 스레드 (60 FPS)
#pragma once

#include "overlay_window.h"
#include "core/tokenizer/furigana_mapper.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <chrono>

namespace toriyomi {
namespace ui {

/**
 * @brief 후리가나 데이터를 스레드 안전하게 공유하는 더블 버퍼
 * 
 * Lock-free 읽기를 위해 std::atomic<int>로 버퍼 인덱스를 스왑합니다.
 * Writer(OcrThread)는 백 버퍼에 쓰고, Reader(OverlayThread)는 프론트 버퍼를 읽습니다.
 */
class FuriganaBuffer {
public:
    FuriganaBuffer();
    ~FuriganaBuffer() = default;

    /**
     * @brief 새 후리가나 데이터 업데이트 (Writer용)
     * @param data 업데이트할 후리가나 목록
     */
    void Update(const std::vector<tokenizer::FuriganaInfo>& data);

    /**
     * @brief 현재 후리가나 데이터 읽기 (Reader용)
     * @return 현재 프론트 버퍼의 후리가나 목록
     */
    std::vector<tokenizer::FuriganaInfo> Get() const;

    /**
     * @brief 데이터가 업데이트되었는지 확인
     */
    bool HasUpdate();

private:
    mutable std::mutex mutex_;                                // 쓰기 보호
    std::vector<tokenizer::FuriganaInfo> buffer_[2];          // 더블 버퍼
    std::atomic<int> frontIndex_;                             // 읽기용 버퍼 인덱스 (0 or 1)
    std::atomic<bool> hasUpdate_;                             // 업데이트 플래그
};

/**
 * @brief 오버레이 윈도우를 60 FPS로 렌더링하는 스레드
 * 
 * OcrThread에서 생성된 후리가나 데이터를 받아서
 * 16ms 주기로 OverlayWindow에 렌더링합니다.
 */
class OverlayThread {
public:
    OverlayThread();
    ~OverlayThread();

    // 복사/이동 금지
    OverlayThread(const OverlayThread&) = delete;
    OverlayThread& operator=(const OverlayThread&) = delete;

    /**
     * @brief 스레드 시작 및 오버레이 윈도우 생성
     * @param x 윈도우 X 좌표
     * @param y 윈도우 Y 좌표
     * @param width 윈도우 너비
     * @param height 윈도우 높이
     * @return 성공 시 true
     */
    bool Start(int x, int y, int width, int height);

    /**
     * @brief 스레드 정지 및 윈도우 파괴
     */
    void Stop();

    /**
     * @brief 스레드가 실행 중인지 확인
     */
    bool IsRunning() const;

    /**
     * @brief 후리가나 데이터 업데이트 (OcrThread에서 호출)
     * @param furiganaList 새 후리가나 목록
     */
    void UpdateFurigana(const std::vector<tokenizer::FuriganaInfo>& furiganaList);

    /**
     * @brief 렌더링 통계 가져오기
     */
    struct Stats {
        double averageFps;      // 평균 FPS
        int frameCount;         // 총 프레임 수
        int updateCount;        // 후리가나 업데이트 횟수
    };
    Stats GetStats() const;

private:
    /**
     * @brief 렌더링 루프 (스레드 함수)
     */
    void RenderLoop();

    /**
     * @brief FPS 계산 업데이트
     */
    void UpdateFps();

private:
    std::unique_ptr<OverlayWindow> window_;      // 오버레이 윈도우
    FuriganaBuffer furiganaBuffer_;              // 스레드 안전 후리가나 버퍼
    
    std::thread renderThread_;                   // 렌더링 스레드
    std::atomic<bool> running_;                  // 실행 플래그
    
    // 통계
    std::atomic<int> frameCount_;                // 프레임 카운트
    std::atomic<int> updateCount_;               // 업데이트 카운트
    std::atomic<double> averageFps_;             // 평균 FPS
    
    std::chrono::steady_clock::time_point lastFpsUpdate_;  // 마지막 FPS 업데이트 시간
    int framesSinceLastUpdate_;                  // 마지막 업데이트 이후 프레임 수
};

}  // namespace ui
}  // namespace toriyomi
