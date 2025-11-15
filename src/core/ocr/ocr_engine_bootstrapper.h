#pragma once

#include "ocr_engine.h"
#include <memory>
#include <string>
#include <vector>

namespace toriyomi {
namespace ocr {

struct OcrBootstrapConfig {
    std::vector<std::string> tessdataSearchPaths;
    std::string tessLanguage = "jpn";
    std::string paddleModelDirectory = "./models/paddleocr";
    std::string paddleLanguage = "jpn";
    bool allowTesseractFallback = true;
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
    bool InitializeTesseract(const std::shared_ptr<IOcrEngine>& engine) const;
    bool InitializePaddleOcr(const std::shared_ptr<IOcrEngine>& engine) const;

    OcrBootstrapConfig config_;
    OcrEngineType preferredType_ = OcrEngineType::PaddleOCR;
};

}  // namespace ocr
}  // namespace toriyomi
