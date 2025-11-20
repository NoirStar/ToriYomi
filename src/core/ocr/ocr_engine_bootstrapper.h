#pragma once

#include "core/ocr/paddle/paddle_ocr_options.h"
#include "ocr_engine.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace toriyomi {
namespace ocr {

struct OcrBootstrapConfig {
    std::string paddleModelDirectory = "./models/paddleocr";
    std::string paddleLanguage = "jpn";
    std::string paddleConfigPath;                  // JSON/YAML 설정 경로 (선택)
    std::size_t paddlePipelineCount = 1;           // 앞으로 사용할 파이프라인 개수
    std::optional<PaddleOcrOptions> overrideOptions;
};

class OcrEngineBootstrapper {
public:
    explicit OcrEngineBootstrapper(OcrBootstrapConfig config = {});

    void SetPreferredEngine(OcrEngineType type);
    OcrEngineType GetPreferredEngine() const;

    std::shared_ptr<IOcrEngine> CreateEngine(OcrEngineType type) const;
    std::shared_ptr<IOcrEngine> CreateAndInitialize();
    std::shared_ptr<IOcrEngine> CreateAndInitialize(OcrEngineType type);
    bool InitializeEngine(OcrEngineType type, const std::shared_ptr<IOcrEngine>& engine) const;

private:
    bool InitializePaddleOcr(const std::shared_ptr<IOcrEngine>& engine) const;

    OcrBootstrapConfig config_;
    OcrEngineType preferredType_ = OcrEngineType::PaddleOCR;
};

}  // namespace ocr
}  // namespace toriyomi
