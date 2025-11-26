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

    // 텍스트 감지 파라미터 (줄 분리 조정용)
    std::optional<float> detThresh;       // 감지 임계값 (기본: 0.3)
    std::optional<float> detBoxThresh;    // 박스 임계값 (기본: 0.6)
    std::optional<float> detUnclipRatio;  // 박스 확장 비율 (기본: 2.0, 낮추면 줄 분리됨)

    static PaddleOcrOptions FromModelRoot(const std::filesystem::path& root,
                                          const std::string& language);
    static std::optional<PaddleOcrOptions> FromJsonFile(const std::filesystem::path& jsonPath,
                                                        std::string& errorMessage);
};

}  // namespace ocr
}  // namespace toriyomi
