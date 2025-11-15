#include "paddle_ocr_wrapper.h"

#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <limits>
#include <thread>

#ifdef TORIYOMI_ENABLE_PADDLEOCR
#include <fastdeploy/vision.h>
#endif

namespace toriyomi {
namespace ocr {

namespace {
constexpr float kDefaultConfidenceScale = 100.0f;
}

#ifdef TORIYOMI_ENABLE_PADDLEOCR
class PaddleOcrWrapper::Runtime {
public:
    Runtime() = default;
    ~Runtime() = default;

    bool Initialize(const std::string& modelDir, const std::string& language) {
        const std::string detModelDir = modelDir + "/det";
        const std::string recModelDir = modelDir + "/rec";
        const std::string clsModelDir = modelDir + "/cls";
        const std::string labelPath = modelDir + "/ppocr_keys_v1.txt";

        fastdeploy::RuntimeOption option;
        option.UseCpu();
        option.SetCpuThreadNum(std::max(2u, std::thread::hardware_concurrency()));

        try {
            pipeline_ = std::make_unique<fastdeploy::vision::ocr::PPOCRv4>(
                detModelDir, recModelDir, clsModelDir, labelPath, option);
        } catch (const std::exception& ex) {
            SPDLOG_ERROR("PaddleOCR 초기화 실패: {}", ex.what());
            pipeline_.reset();
            return false;
        }

        SPDLOG_INFO("PaddleOCR 모델 초기화 완료 (언어: {})", language);
        return pipeline_ != nullptr;
    }

    bool Predict(const cv::Mat& image, fastdeploy::vision::OCRResult& result) {
        if (!pipeline_) {
            return false;
        }
        return pipeline_->Predict(image, &result);
    }

private:
    std::unique_ptr<fastdeploy::vision::ocr::PPOCRv4> pipeline_;
};
#endif

namespace {

void ClampRect(cv::Rect& rect, const cv::Size& bounds) {
    if (bounds.width <= 0 || bounds.height <= 0) {
        return;
    }

    const int x = std::clamp(rect.x, 0, std::max(0, bounds.width - 1));
    const int y = std::clamp(rect.y, 0, std::max(0, bounds.height - 1));
    const int maxWidth = bounds.width - x;
    const int maxHeight = bounds.height - y;
    rect.width = std::clamp(rect.width, 1, maxWidth);
    rect.height = std::clamp(rect.height, 1, maxHeight);
    rect.x = x;
    rect.y = y;
}

}  // namespace

PaddleOcrWrapper::PaddleOcrWrapper() = default;
PaddleOcrWrapper::~PaddleOcrWrapper() {
    Shutdown();
}

bool PaddleOcrWrapper::Initialize(const std::string& modelDir, const std::string& language) {
    std::lock_guard<std::mutex> guard(runtimeMutex_);
    ResetRuntimeLocked();

    modelDirectory_ = modelDir;
    language_ = language;

    if (modelDirectory_.empty()) {
        lastError_ = "PaddleOCR 모델 경로가 비어 있습니다";
        SPDLOG_WARN("{}", lastError_);
        return false;
    }

#ifdef TORIYOMI_ENABLE_PADDLEOCR
    runtime_ = std::make_unique<Runtime>();
    if (!runtime_->Initialize(modelDirectory_, language_)) {
        runtime_.reset();
        lastError_ = "PaddleOCR 런타임 초기화 실패";
        SPDLOG_ERROR("{}", lastError_);
        return false;
    }

    initialized_ = true;
    lastError_.clear();
    return true;
#else
    lastError_ = "PaddleOCR 지원이 비활성화되어 있습니다. CMake 옵션 TORIYOMI_ENABLE_PADDLEOCR를 켜세요.";
    SPDLOG_WARN("{}", lastError_);
    initialized_ = false;
    return false;
#endif
}

std::vector<TextSegment> PaddleOcrWrapper::RecognizeText(const cv::Mat& image) {
    std::lock_guard<std::mutex> guard(runtimeMutex_);

    if (!initialized_) {
        return {};
    }

    if (image.empty()) {
        lastError_ = "입력 이미지가 비어 있습니다";
        return {};
    }

    return RunInference(image);
}

void PaddleOcrWrapper::Shutdown() {
    std::lock_guard<std::mutex> guard(runtimeMutex_);
    ResetRuntimeLocked();
}

bool PaddleOcrWrapper::IsInitialized() const {
    return initialized_;
}

std::string PaddleOcrWrapper::GetEngineName() const {
    return "PaddleOCR";
}

std::string PaddleOcrWrapper::GetLastError() const {
    std::lock_guard<std::mutex> guard(runtimeMutex_);
    return lastError_;
}

std::vector<TextSegment> PaddleOcrWrapper::RunInference(const cv::Mat& image) {
    std::vector<TextSegment> segments;

#ifndef TORIYOMI_ENABLE_PADDLEOCR
    (void)image;
    return segments;
#else
    if (!runtime_) {
        lastError_ = "PaddleOCR 런타임이 준비되지 않았습니다";
        return segments;
    }

    fastdeploy::vision::OCRResult fdResult;
    if (!runtime_->Predict(image, fdResult)) {
        lastError_ = "PaddleOCR 추론 호출 실패";
        return segments;
    }

    const size_t textCount = fdResult.text.size();
    segments.reserve(textCount);

    for (size_t idx = 0; idx < textCount; ++idx) {
        TextSegment segment;
        segment.text = fdResult.text[idx];
        if (idx < fdResult.score.size()) {
            segment.confidence = fdResult.score[idx] * kDefaultConfidenceScale;
        } else {
            segment.confidence = 0.0f;
        }

        if (idx < fdResult.boxes.size()) {
            const auto& box = fdResult.boxes[idx];
            if (box.size() >= 8) {
                float minX = std::numeric_limits<float>::max();
                float minY = std::numeric_limits<float>::max();
                float maxX = std::numeric_limits<float>::lowest();
                float maxY = std::numeric_limits<float>::lowest();

                for (size_t i = 0; i + 1 < box.size(); i += 2) {
                    minX = std::min(minX, box[i]);
                    minY = std::min(minY, box[i + 1]);
                    maxX = std::max(maxX, box[i]);
                    maxY = std::max(maxY, box[i + 1]);
                }

                const int width = static_cast<int>(std::max(1.0f, maxX - minX));
                const int height = static_cast<int>(std::max(1.0f, maxY - minY));
                segment.boundingBox = cv::Rect(static_cast<int>(minX), static_cast<int>(minY), width, height);
                ClampRect(segment.boundingBox, image.size());
            }
        }

        segments.push_back(std::move(segment));
    }

    return segments;
#endif
}

void PaddleOcrWrapper::ResetRuntimeLocked() {
#ifdef TORIYOMI_ENABLE_PADDLEOCR
    runtime_.reset();
#endif
    initialized_ = false;
    lastError_.clear();
}

}  // namespace ocr
}  // namespace toriyomi
