#include "paddle_ocr_wrapper.h"

#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <thread>

#include "paddleocr/src/pipelines/ocr/pipeline.h"
#include "paddleocr/src/utils/utility.h"

namespace toriyomi {
namespace ocr {

namespace {
constexpr float kDefaultConfidenceScale = 100.0f;
void ClampRect(cv::Rect& rect, const cv::Size& bounds);
namespace fs = std::filesystem;

std::string NormalizeLanguageCode(const std::string& language) {
    std::string lowered;
    lowered.reserve(language.size());
    for (char ch : language) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (lowered == "ja" || lowered == "jp" || lowered == "japanese") {
        return "japan";
    }
    if (lowered == "ko" || lowered == "kr" || lowered == "korean") {
        return "korean";
    }
    if (lowered == "en" || lowered == "eng" || lowered == "english") {
        return "en";
    }
    if (lowered == "zh-tw" || lowered == "zh_tw" || lowered == "traditional" ||
        lowered == "cht" || lowered == "chinese_cht") {
        return "chinese_cht";
    }
    if (lowered == "zh" || lowered == "zh-cn" || lowered == "ch" ||
        lowered == "chinese" || lowered == "simplified") {
        return "ch";
    }
    if (lowered == "ru" || lowered == "rus" || lowered == "russian") {
        return "ru";
    }
    return lowered.empty() ? std::string("ch") : lowered;
}
}

class PaddleOcrWrapper::Runtime {
public:
    Runtime() = default;
    ~Runtime() = default;

    bool Initialize(const PaddleOcrOptions& options);
    bool Predict(const cv::Mat& image, std::vector<TextSegment>& segments);

private:
    std::unique_ptr<_OCRPipeline> pipeline_;
};

bool PaddleOcrWrapper::Runtime::Initialize(const PaddleOcrOptions& options) {
    const fs::path detPath = options.detModelDir;
    const fs::path recPath = options.recModelDir;

    if (detPath.empty() || !fs::exists(detPath)) {
        SPDLOG_ERROR("PaddleOCR det 모델 디렉터리를 찾을 수 없습니다: {}", detPath.string());
        return false;
    }
    if (recPath.empty() || !fs::exists(recPath)) {
        SPDLOG_ERROR("PaddleOCR rec 모델 디렉터리를 찾을 수 없습니다: {}", recPath.string());
        return false;
    }

    auto deviceToString = [](PaddleDeviceType device) {
        switch (device) {
            case PaddleDeviceType::GPU:
                return std::string{"gpu"};
            case PaddleDeviceType::DirectML:
                return std::string{"dml"};
            case PaddleDeviceType::CPU:
            default:
                return std::string{"cpu"};
        }
    };

    OCRPipelineParams params;
    params.text_detection_model_dir = detPath.string();
    params.text_recognition_model_dir = recPath.string();
    params.use_doc_orientation_classify = options.enableDocOrientation;
    params.use_doc_unwarping = false;
    params.use_textline_orientation = options.enableTextlineOrientation;
    params.text_recognition_batch_size = std::max(1, options.recBatchSize);
    params.lang = NormalizeLanguageCode(options.language);
    params.device = deviceToString(options.device);
    params.enable_mkldnn = options.enableMkldnn && Utility::IsMkldnnAvailable();
    params.cpu_threads = std::max(1, options.cpuThreads);
    params.thread_num = 1;

    if (options.enableCls && !options.clsModelDir.empty()) {
        params.textline_orientation_model_dir = options.clsModelDir.string();
        params.textline_orientation_batch_size = options.recBatchSize;
    }

    try {
        pipeline_ = std::make_unique<_OCRPipeline>(params);
    } catch (const std::exception& ex) {
        SPDLOG_ERROR("PaddleOCR 파이프라인 초기화 실패: {}", ex.what());
        pipeline_.reset();
        return false;
    }

    SPDLOG_INFO("PaddleOCR cpp_infer 파이프라인 초기화 완료 (언어: {})", params.lang.value_or("ch"));
    return static_cast<bool>(pipeline_);
}

bool PaddleOcrWrapper::Runtime::Predict(const cv::Mat& image, std::vector<TextSegment>& segments) {
    if (!pipeline_) {
        return false;
    }
    std::vector<cv::Mat> inputs = {image};
    (void)pipeline_->Predict(inputs);
    const auto pipeline_results = pipeline_->PipelineResult();
    segments.clear();
    if (pipeline_results.empty()) {
        return true;
    }
    const auto& result = pipeline_results.front();
    const size_t count = result.rec_texts.size();
    segments.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        TextSegment segment;
        segment.text = result.rec_texts[i];
        if (i < result.rec_scores.size()) {
            segment.confidence = result.rec_scores[i] * kDefaultConfidenceScale;
        } else {
            segment.confidence = 0.0f;
        }

        cv::Rect bbox;
        if (i < result.rec_polys.size() && !result.rec_polys[i].empty()) {
            bbox = cv::boundingRect(result.rec_polys[i]);
        } else if (i < result.rec_boxes.size()) {
            const auto& box = result.rec_boxes[i];
            const int left = static_cast<int>(box[0]);
            const int top = static_cast<int>(box[1]);
            const int right = static_cast<int>(box[2]);
            const int bottom = static_cast<int>(box[3]);
            bbox = cv::Rect(left, top, std::max(1, right - left), std::max(1, bottom - top));
        }
        if (bbox.width > 0 && bbox.height > 0) {
            ClampRect(bbox, image.size());
            segment.boundingBox = bbox;
        }

        segments.push_back(std::move(segment));
    }
    return true;
}

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
    PaddleOcrOptions options = PaddleOcrOptions::FromModelRoot(modelDir, language);
    return InitializeWithOptions(options);
}

bool PaddleOcrWrapper::InitializeWithOptions(const PaddleOcrOptions& options) {
    std::lock_guard<std::mutex> guard(runtimeMutex_);
    ResetRuntimeLocked();

    PaddleOcrOptions normalized = options;
    if (normalized.language.empty()) {
        normalized.language = "jpn";
    }
    if (normalized.cpuThreads <= 0) {
        const unsigned concurrency = std::thread::hardware_concurrency();
        normalized.cpuThreads = concurrency == 0 ? 4 : static_cast<int>(concurrency);
    }
    if (normalized.recBatchSize <= 0) {
        normalized.recBatchSize = 1;
    }

    if (normalized.detModelDir.empty() || normalized.recModelDir.empty()) {
        lastError_ = "PaddleOCR 모델 경로가 올바르지 않습니다";
        SPDLOG_WARN("{}", lastError_);
        return false;
    }

    runtime_ = std::make_unique<Runtime>();
    if (!runtime_->Initialize(normalized)) {
        runtime_.reset();
        lastError_ = "PaddleOCR 런타임 초기화 실패";
        SPDLOG_ERROR("{}", lastError_);
        return false;
    }

    activeOptions_ = normalized;
    hasActiveOptions_ = true;
    initialized_ = true;
    lastError_.clear();
    return true;
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

    if (!runtime_) {
        lastError_ = "PaddleOCR 런타임이 준비되지 않았습니다";
        return segments;
    }

    if (!runtime_->Predict(image, segments)) {
        lastError_ = "PaddleOCR 추론 호출 실패";
        segments.clear();
        return segments;
    }
    return segments;
}

void PaddleOcrWrapper::ResetRuntimeLocked() {
    runtime_.reset();
    initialized_ = false;
    lastError_.clear();
}

}  // namespace ocr
}  // namespace toriyomi
