#pragma once

#include "ocr_engine.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace toriyomi {
namespace ocr {

/**
 * @brief PaddleOCR 백엔드 래퍼
 *
 * FastDeploy 기반 PaddleOCR 추론을 IOcrEngine 인터페이스로 감싼 구현체입니다.
 * TORIYOMI_ENABLE_PADDLEOCR 매크로가 비활성화된 경우에도 안전하게 동작하도록
 * 런타임 의존성을 느슨하게 유지합니다.
 */
class PaddleOcrWrapper : public IOcrEngine {
public:
    PaddleOcrWrapper();
    ~PaddleOcrWrapper() override;

    bool Initialize(const std::string& modelDir, const std::string& language = "jpn") override;
    std::vector<TextSegment> RecognizeText(const cv::Mat& image) override;
    void Shutdown() override;
    bool IsInitialized() const override;
    std::string GetEngineName() const override;

    /**
     * @brief 마지막 오류 메시지 (디버깅 용도)
     */
    std::string GetLastError() const;

private:
    std::vector<TextSegment> RunInference(const cv::Mat& image);
    void ResetRuntimeLocked();

    mutable std::mutex runtimeMutex_;
    bool initialized_ = false;
    std::string modelDirectory_;
    std::string language_ = "jpn";
    std::string lastError_;

#ifdef TORIYOMI_ENABLE_PADDLEOCR
    class Runtime;
    std::unique_ptr<Runtime> runtime_;
#endif
};

}  // namespace ocr
}  // namespace toriyomi
