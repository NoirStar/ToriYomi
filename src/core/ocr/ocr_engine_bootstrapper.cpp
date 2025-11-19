#include "ocr_engine_bootstrapper.h"

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include "paddle_ocr_wrapper.h"
#include <spdlog/spdlog.h>
#include <utility>

namespace toriyomi {
namespace ocr {
namespace {

const char* ToString(OcrEngineType type) {
    switch (type) {
        case OcrEngineType::PaddleOCR:
            return "PaddleOCR";
        case OcrEngineType::EasyOCR:
            return "EasyOCR";
        default:
            return "Unknown";
    }
}

}  // namespace

OcrEngineBootstrapper::OcrEngineBootstrapper(OcrBootstrapConfig config)
    : config_(std::move(config)) {}

void OcrEngineBootstrapper::SetPreferredEngine(OcrEngineType type) {
    preferredType_ = type;
}

OcrEngineType OcrEngineBootstrapper::GetPreferredEngine() const {
    return preferredType_;
}

std::shared_ptr<IOcrEngine> OcrEngineBootstrapper::CreateEngine(OcrEngineType type) const {
    auto engine = OcrEngineFactory::CreateEngine(type);
    if (!engine) {
    SPDLOG_ERROR("Failed to create OCR engine (type={})", ToString(type));
        return nullptr;
    }
    return std::shared_ptr<IOcrEngine>(std::move(engine));
}

std::shared_ptr<IOcrEngine> OcrEngineBootstrapper::CreateAndInitialize() {
    return CreateAndInitialize(preferredType_);
}

std::shared_ptr<IOcrEngine> OcrEngineBootstrapper::CreateAndInitialize(OcrEngineType type) {
    auto engine = CreateEngine(type);
    if (!engine) {
        return nullptr;
    }

    if (!InitializeEngine(type, engine)) {
        SPDLOG_ERROR("{} engine initialization failed", ToString(type));
        return nullptr;
    }

    return engine;
}

bool OcrEngineBootstrapper::InitializeEngine(OcrEngineType type, const std::shared_ptr<IOcrEngine>& engine) const {
    if (!engine) {
        return false;
    }

    switch (type) {
        case OcrEngineType::PaddleOCR:
            return InitializePaddleOcr(engine);
        case OcrEngineType::EasyOCR:
            SPDLOG_WARN("EasyOCR engine is not implemented yet");
            return false;
        default:
            return false;
    }
}

bool OcrEngineBootstrapper::InitializePaddleOcr(const std::shared_ptr<IOcrEngine>& engine) const {
    if (config_.paddleModelDirectory.empty()) {
        SPDLOG_WARN("PaddleOCR model directory is missing");
        return false;
    }

    if (engine->Initialize(config_.paddleModelDirectory, config_.paddleLanguage)) {
        SPDLOG_INFO("PaddleOCR initialized: {}", config_.paddleModelDirectory);
        return true;
    }

    SPDLOG_ERROR("PaddleOCR initialization failed: {}", config_.paddleModelDirectory);

    if (auto* paddle = dynamic_cast<PaddleOcrWrapper*>(engine.get())) {
        SPDLOG_ERROR("PaddleOCR detailed error: {}", paddle->GetLastError());
    }

    return false;
}

}  // namespace ocr
}  // namespace toriyomi
