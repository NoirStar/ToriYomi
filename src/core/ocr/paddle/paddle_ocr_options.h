#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace toriyomi {
namespace ocr {

enum class PaddleDeviceType {
    CPU,
    GPU,
    DirectML
};

struct PaddleOcrOptions {
    std::filesystem::path detModelDir;
    std::filesystem::path recModelDir;
    std::filesystem::path clsModelDir;
    std::filesystem::path labelPath;
    std::optional<std::string> detModelName;
    std::optional<std::string> recModelName;
    std::optional<std::string> clsModelName;
    std::string language = "jpn";

    PaddleDeviceType device = PaddleDeviceType::CPU;
    int gpuId = 0;
    bool enableMkldnn = true;
    int cpuThreads = 0;           // 0이면 하드웨어 동시성 사용
    int recBatchSize = 1;
    bool enableCls = false;
    bool enableDocOrientation = false;
    bool enableTextlineOrientation = false;

    static PaddleOcrOptions FromModelRoot(const std::filesystem::path& root,
                                          const std::string& language);
    static std::optional<PaddleOcrOptions> FromJsonFile(const std::filesystem::path& jsonPath,
                                                        std::string& errorMessage);
};

}  // namespace ocr
}  // namespace toriyomi
