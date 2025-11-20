#include "core/ocr/paddle/paddle_ocr_options.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <thread>

#include <nlohmann/json.hpp>

namespace toriyomi {
namespace ocr {
namespace {
constexpr const char* kDefaultDetDir = "det";
constexpr const char* kDefaultRecDir = "rec";

int ResolveCpuThreads(int requested) {
    if (requested > 0) {
        return requested;
    }
    const unsigned concurrency = std::thread::hardware_concurrency();
    return concurrency == 0 ? 4 : static_cast<int>(concurrency);
}

std::string NormalizeLanguage(std::string language) {
    std::transform(language.begin(), language.end(), language.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (language.empty()) {
        return "jpn";
    }
    return language;
}

}  // namespace

PaddleOcrOptions PaddleOcrOptions::FromModelRoot(const std::filesystem::path& root,
                                                 const std::string& language) {
    PaddleOcrOptions options;
    options.detModelDir = root / kDefaultDetDir;
    options.recModelDir = root / kDefaultRecDir;
    options.language = NormalizeLanguage(language);
    options.cpuThreads = ResolveCpuThreads(0);
    return options;
}

std::optional<PaddleOcrOptions> PaddleOcrOptions::FromJsonFile(const std::filesystem::path& jsonPath,
                                                               std::string& errorMessage) {
    std::ifstream stream(jsonPath);
    if (!stream) {
        errorMessage = "Failed to open Paddle OCR config: " + jsonPath.string();
        return std::nullopt;
    }

    nlohmann::json doc;
    try {
        stream >> doc;
    } catch (const std::exception& ex) {
        errorMessage = std::string{"Invalid Paddle OCR config JSON: "} + ex.what();
        return std::nullopt;
    }

    PaddleOcrOptions opts;
    auto get_optional_string = [&](const char* key) -> std::optional<std::string> {
        if (!doc.contains(key)) {
            return std::nullopt;
        }
        return doc[key].get<std::string>();
    };

    if (auto det = get_optional_string("det_model")) {
        opts.detModelDir = *det;
    }
    if (auto rec = get_optional_string("rec_model")) {
        opts.recModelDir = *rec;
    }
    if (auto cls = get_optional_string("cls_model")) {
        opts.clsModelDir = *cls;
    }
    if (auto label = get_optional_string("label_path")) {
        opts.labelPath = *label;
    }
    if (auto lang = get_optional_string("lang")) {
        opts.language = NormalizeLanguage(*lang);
    }

    const auto deviceStr = get_optional_string("device");
    if (deviceStr) {
        if (*deviceStr == "gpu") {
            opts.device = PaddleDeviceType::GPU;
        } else if (*deviceStr == "dml" || *deviceStr == "directml") {
            opts.device = PaddleDeviceType::DirectML;
        } else {
            opts.device = PaddleDeviceType::CPU;
        }
    }

    if (doc.contains("gpu_id")) {
        opts.gpuId = doc["gpu_id"].get<int>();
    }
    if (doc.contains("enable_mkldnn")) {
        opts.enableMkldnn = doc["enable_mkldnn"].get<bool>();
    }
    if (doc.contains("cpu_threads")) {
        opts.cpuThreads = ResolveCpuThreads(doc["cpu_threads"].get<int>());
    } else {
        opts.cpuThreads = ResolveCpuThreads(0);
    }
    if (doc.contains("rec_batch_size")) {
        opts.recBatchSize = std::max(1, doc["rec_batch_size"].get<int>());
    }
    if (doc.contains("enable_cls")) {
        opts.enableCls = doc["enable_cls"].get<bool>();
    }
    if (doc.contains("enable_doc_orientation")) {
        opts.enableDocOrientation = doc["enable_doc_orientation"].get<bool>();
    }
    if (doc.contains("enable_textline_orientation")) {
        opts.enableTextlineOrientation = doc["enable_textline_orientation"].get<bool>();
    }

    if (opts.detModelDir.empty() || opts.recModelDir.empty()) {
        errorMessage = "Paddle OCR config must contain det_model and rec_model";
        return std::nullopt;
    }

    return opts;
}

}  // namespace ocr
}  // namespace toriyomi
