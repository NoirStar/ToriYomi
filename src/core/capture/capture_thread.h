// ToriYomi - 캡처 스레드
// DXGI/GDI 캡처를 백그라운드 스레드에서 실행하고 FrameQueue에 푸시

#pragma once

#include "core/capture/frame_queue.h"
#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <memory>
#include <atomic>

namespace toriyomi::capture {

/**
 * @brief 캡처 통계 정보
 */
struct CaptureStatistics {
    uint64_t totalFramesCaptured{0};  // 총 캡처된 프레임 수
    uint64_t framesSkipped{0};        // 스킵된 프레임 수 (중복)
    double currentFps{0.0};           // 현재 FPS
    bool usingDxgi{false};            // DXGI 사용 여부 (false면 GDI)
};

/**
 * @brief 화면 캡처를 백그라운드 스레드에서 실행하는 클래스
 * 
 * DXGI를 먼저 시도하고 실패 시 GDI로 폴백합니다.
 * 캡처된 프레임을 FrameQueue에 자동으로 푸시합니다.
 * 
 * 주요 기능:
 * - DXGI/GDI 자동 선택
 * - 프레임 변경 감지 (히스토그램 비교)
 * - 중복 프레임 스킵 (CPU/메모리 절약)
 * - 실시간 FPS 측정
 * 
 * 사용 예시:
 * ```cpp
 * auto queue = std::make_shared<FrameQueue>(5);
 * CaptureThread capture(queue);
 * 
 * capture.SetChangeDetection(true); // 변경 감지 활성화
 * capture.Start(gameWindow);
 * 
 * // ... 다른 작업 ...
 * 
 * capture.Stop();
 * ```
 */
class CaptureThread {
public:
    /**
     * @brief CaptureThread 생성자
     * 
     * @param frameQueue 캡처된 프레임을 푸시할 큐
     */
    explicit CaptureThread(std::shared_ptr<FrameQueue> frameQueue);
    
    /**
     * @brief 소멸자 - 자동으로 스레드 정지
     */
    ~CaptureThread();

    CaptureThread(const CaptureThread&) = delete;
    CaptureThread& operator=(const CaptureThread&) = delete;

    /**
     * @brief 캡처 스레드 시작
     * 
     * DXGI를 먼저 시도하고, 실패 시 GDI로 폴백합니다.
     * 
     * @param targetWindow 캡처할 윈도우 핸들
     * @return 시작 성공 시 true, 실패 시 false
     */
    bool Start(HWND targetWindow);

    /**
     * @brief 캡처 스레드 정지
     * 
     * 스레드가 안전하게 종료될 때까지 대기합니다.
     */
    void Stop();

    /**
     * @brief 스레드 실행 상태 확인
     * 
     * @return 실행 중이면 true
     */
    bool IsRunning() const;

    /**
     * @brief 프레임 변경 감지 활성화/비활성화
     * 
     * 활성화 시 이전 프레임과 히스토그램을 비교하여
     * 유사도가 높으면 (>0.95) 프레임을 스킵합니다.
     * 
     * @param enable true면 활성화, false면 비활성화
     */
    void SetChangeDetection(bool enable);

    /**
     * @brief 캡처 통계 가져오기
     * 
     * @return CaptureStatistics 구조체
     */
    CaptureStatistics GetStatistics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace toriyomi::capture
