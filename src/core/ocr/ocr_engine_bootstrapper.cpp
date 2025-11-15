#include "ocr_engine_bootstrapper.h"

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include "paddle_ocr_wrapper.h"
#include "tesseract_wrapper.h"
#include <spdlog/spdlog.h>
#include <utility>

namespace toriyomi {
namespace ocr {
namespace {

std::vector<std::string> BuildDefaultTessdataSearchPaths() {
    return {
        "C:/vcpkg/installed/x64-windows/share/tessdata",
        "C:/Program Files/Tesseract-OCR/tessdata",
        "./tessdata",
        "../tessdata"
    };
}

const char* ToString(OcrEngineType type) {
    switch (type) {
        case OcrEngineType::Tesseract:
            return "Tesseract";
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
    : config_(std::move(config)) {
    if (config_.tessdataSearchPaths.empty()) {
        config_.tessdataSearchPaths = BuildDefaultTessdataSearchPaths();
    }
}

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

        if (type == OcrEngineType::PaddleOCR && config_.allowTesseractFallback) {
            SPDLOG_WARN("PaddleOCR init failed - attempting Tesseract fallback");
            auto fallback = CreateEngine(OcrEngineType::Tesseract);
            if (fallback && InitializeEngine(OcrEngineType::Tesseract, fallback)) {
                return fallback;
            }
        }

        return nullptr;
    }

    return engine;
}

bool OcrEngineBootstrapper::InitializeEngine(OcrEngineType type, const std::shared_ptr<IOcrEngine>& engine) const {
    if (!engine) {
        return false;
    }

    switch (type) {
        case OcrEngineType::Tesseract:
            return InitializeTesseract(engine);
        case OcrEngineType::PaddleOCR:
            return InitializePaddleOcr(engine);
        case OcrEngineType::EasyOCR:
            SPDLOG_WARN("EasyOCR engine is not implemented yet");
            return false;
        default:
            return false;
    }
}

bool OcrEngineBootstrapper::InitializeTesseract(const std::shared_ptr<IOcrEngine>& engine) const {
    for (const auto& path : config_.tessdataSearchPaths) {
        if (path.empty()) {
            continue;
        }

        SPDLOG_INFO("Trying Tesseract initialization: {}", path);
        if (engine->Initialize(path, config_.tessLanguage)) {
            SPDLOG_INFO("Tesseract initialized: {}", path);
            return true;
        }
        SPDLOG_WARN("Tesseract initialization failed: {}", path);
    }

    SPDLOG_ERROR("No usable tessdata directory was found");
    return false;
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
