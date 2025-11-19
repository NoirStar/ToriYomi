#pragma once

#include "core/capture/frame_queue.h"
#include "ocr_engine.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>

namespace toriyomi {
namespace ocr {

/**
 * @brief OCR 스레드 통계 정보
 */
struct OcrStatistics {
    uint64_t totalFramesProcessed = 0;  // 처리한 총 프레임 수
    double currentFps = 0.0;             // 현재 OCR FPS
    uint64_t totalTextSegments = 0;      // 인식한 총 텍스트 세그먼트 수
    std::string engineName;              // 사용 중인 OCR 엔진 이름
};

/**
 * @brief OCR 백그라운드 스레드
 * 
 * FrameQueue에서 프레임을 꺼내 OCR 엔진으로 텍스트를 인식하는
 * 별도의 스레드. 인식된 결과는 내부에 저장되며 GetLatestResults()로
 * 조회 가능.
 */
class OcrThread {
public:
    /**
     * @brief OcrThread 생성
     * 
     * @param frameQueue 프레임을 받아올 큐 (CaptureThread와 공유)
    * @param ocrEngine OCR 엔진 (PaddleOCR) - shared_ptr로 생명주기 공유
     */
    OcrThread(std::shared_ptr<FrameQueue> frameQueue,
              std::shared_ptr<IOcrEngine> ocrEngine);

    /**
     * @brief OcrThread 소멸
     */
    ~OcrThread();

    // 복사 방지
    OcrThread(const OcrThread&) = delete;
    OcrThread& operator=(const OcrThread&) = delete;

    /**
     * @brief OCR 스레드 시작
     * 
     * @return 시작 성공 여부
     * 
     * @note OCR 엔진이 초기화되지 않은 경우 false 반환
     */
    bool Start();

    /**
     * @brief OCR 스레드 정지
     * 
     * 백그라운드 스레드를 안전하게 종료합니다.
     */
    void Stop();

    /**
     * @brief 스레드 실행 상태 확인
     * 
     * @return 스레드 실행 중 여부
     */
    bool IsRunning() const;

    /**
     * @brief 최신 OCR 결과 가져오기
     * 
     * @return 가장 최근에 인식된 텍스트 세그먼트 목록
     * 
     * @note 스레드 안전함 (내부적으로 mutex 사용)
     */
    std::vector<TextSegment> GetLatestResults() const;

    /**
     * @brief OCR 입력 이미지에서 사용할 자르기 영역 설정
     */
    void SetCropRegion(const cv::Rect& rect);

    /**
     * @brief 자르기 영역 해제
     */
    void ClearCropRegion();

    /**
     * @brief OCR 통계 정보 가져오기
     * 
     * @return OCR 처리 통계
     */
    OcrStatistics GetStatistics() const;

private:
    /**
     * @brief OCR 처리 루프 (백그라운드 스레드)
     * 
     * FrameQueue에서 프레임을 계속 꺼내서 OCR 수행.
     * Stop() 호출 시까지 반복.
     */
    void OcrLoop();

    /**
     * @brief FPS 업데이트
     * 
     * 1초마다 현재 FPS를 계산하여 업데이트
     */
    void UpdateFps();

private:
    std::shared_ptr<FrameQueue> frameQueue_;      // 프레임 큐 (공유)
    std::shared_ptr<IOcrEngine> ocrEngine_;       // OCR 엔진 (공유 - 스레드 실행 중 삭제 방지)

    std::thread ocrThread_;                       // 백그라운드 스레드
    std::atomic<bool> running_{false};            // 스레드 실행 상태

    // 인식 결과 (스레드 안전)
    mutable std::mutex resultsMutex_;
    std::vector<TextSegment> latestResults_;

    // 통계 (스레드 안전)
    mutable std::mutex statsMutex_;
    OcrStatistics stats_;

    // FPS 계산용
    std::chrono::steady_clock::time_point lastFpsUpdate_;
    uint64_t framesProcessedSinceLastUpdate_ = 0;

    // ROI 잘라내기 정보
    mutable std::mutex cropMutex_;
    cv::Rect cropRegion_;
    bool cropEnabled_ = false;
};

}  // namespace ocr
}  // namespace toriyomi
