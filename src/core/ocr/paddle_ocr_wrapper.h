#pragma once

#include "core/ocr/paddle/paddle_ocr_options.h"
#include "ocr_engine.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace toriyomi {
namespace ocr {

/**
 * @brief PaddleOCR cpp_infer 백엔드를 IOcrEngine 인터페이스로 감싼 구현체입니다.
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

    /**
     * @brief 고급 Paddle 옵션으로 직접 초기화 (Bootstrapper 전용)
     */
    bool InitializeWithOptions(const PaddleOcrOptions& options);

private:
    std::vector<TextSegment> RunInference(const cv::Mat& image);
    void ResetRuntimeLocked();

    mutable std::mutex runtimeMutex_;
    bool initialized_ = false;
    std::string lastError_;
    bool hasActiveOptions_ = false;
    PaddleOcrOptions activeOptions_;

    class Runtime;
    std::unique_ptr<Runtime> runtime_;
};

}  // namespace ocr
}  // namespace toriyomi
