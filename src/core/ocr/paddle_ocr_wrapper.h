#pragma once

#include "ocr_engine.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace toriyomi {
namespace ocr {

/**
 * @brief PaddleOCR cpp_infer 백엔드를 IOcrEngine 인터페이스로 감싼 구현체입니다.
 *        프로젝트가 항상 Paddle Inference SDK를 링크하므로 조건부 빌드는 더 이상
 *        사용하지 않습니다.
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

    class Runtime;
    std::unique_ptr<Runtime> runtime_;
};

}  // namespace ocr
}  // namespace toriyomi
