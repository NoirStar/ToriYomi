#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/binary.h>

#include "core/ocr/ocr_engine.h"
#include "core/ocr/ocr_engine_bootstrapper.h"

namespace {
namespace fs = std::filesystem;
using toriyomi::ocr::OcrBootstrapConfig;
using toriyomi::ocr::OcrEngineBootstrapper;
using toriyomi::ocr::TextSegment;

constexpr const char* kBase64FixturePath = TORIYOMI_UI_SCREENSHOT_BASE64_PATH;

std::string LoadBase64Fixture() {
    std::ifstream stream{kBase64FixturePath, std::ios::binary};
    if (!stream) {
        GTEST_SKIP() << "Failed to open base64 fixture at " << kBase64FixturePath;
    }

    std::string contents{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    contents.erase(std::remove_if(contents.begin(), contents.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), contents.end());
    return contents;
}

cv::Mat DecodeBase64ToImage(const std::string& encoded) {
    const auto buffer = YAML::DecodeBase64(encoded);
    std::vector<uint8_t> bytes(buffer.begin(), buffer.end());
    cv::Mat rawData(1, static_cast<int>(bytes.size()), CV_8UC1, bytes.data());
    return cv::imdecode(rawData, cv::IMREAD_COLOR);
}

fs::path ResolveArtifactDirectory() {
    const fs::path base = fs::current_path();
    auto artifactDir = base / "artifacts" / "paddle_ocr_integration";
    fs::create_directories(artifactDir);
    return artifactDir;
}

void WriteJsonSnapshot(const fs::path& path, const std::vector<TextSegment>& segments, const cv::Size& size, const std::string& engineName) {
    nlohmann::json snapshot;
    snapshot["engine_name"] = engineName;
    snapshot["image_width"] = size.width;
    snapshot["image_height"] = size.height;
    snapshot["segment_count"] = segments.size();

    for (const auto& segment : segments) {
        snapshot["segments"].push_back({
            {"text", segment.text},
            {"confidence", segment.confidence},
            {"bbox", {
                {"x", segment.boundingBox.x},
                {"y", segment.boundingBox.y},
                {"width", segment.boundingBox.width},
                {"height", segment.boundingBox.height}
            }}
        });
    }

    std::ofstream out{path, std::ios::binary};
    out << snapshot.dump(2);
}

void RenderAnnotatedImage(const fs::path& path, const cv::Mat& image, const std::vector<TextSegment>& segments) {
    cv::Mat annotated = image.clone();
    const cv::Scalar boxColor(0, 255, 0);

    for (const auto& segment : segments) {
        cv::rectangle(annotated, segment.boundingBox, boxColor, 2);
        cv::putText(annotated, segment.text, segment.boundingBox.tl() + cv::Point(0, -4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, boxColor, 1, cv::LINE_AA);
    }

    cv::imwrite(path.string(), annotated);
}

}  // namespace

TEST(PaddleOcrIntegrationTest, GeneratesSnapshotArtifacts) {
    std::string modelsPath = [] {
        if (const char* env = std::getenv("TORIYOMI_PADDLE_TEST_MODELS")) {
            return std::string{env};
        }
        return std::string{"./models/paddleocr"};
    }();

    if (!fs::exists(modelsPath)) {
        GTEST_SKIP() << "Model directory not found: " << modelsPath;
    }

    const auto base64Image = LoadBase64Fixture();
    const auto decoded = DecodeBase64ToImage(base64Image);
    ASSERT_FALSE(decoded.empty()) << "Failed to decode sample image";

    OcrBootstrapConfig config;
    config.paddleModelDirectory = modelsPath;
    config.paddleLanguage = "jpn";

    OcrEngineBootstrapper bootstrap(config);
    auto engine = bootstrap.CreateAndInitialize();
    ASSERT_NE(engine, nullptr);
    ASSERT_TRUE(engine->IsInitialized());

    const auto segments = engine->RecognizeText(decoded);
    ASSERT_FALSE(segments.empty()) << "No OCR segments generated";

    const auto artifactDir = ResolveArtifactDirectory();
    const auto jsonPath = artifactDir / "paddle_ocr_snapshot.json";
    const auto imagePath = artifactDir / "paddle_ocr_snapshot.png";

    WriteJsonSnapshot(jsonPath, segments, decoded.size(), engine->GetEngineName());
    RenderAnnotatedImage(imagePath, decoded, segments);

    ASSERT_TRUE(fs::exists(jsonPath));
    ASSERT_TRUE(fs::exists(imagePath));
    ASSERT_GT(fs::file_size(jsonPath), 0u);
    ASSERT_GT(fs::file_size(imagePath), 0u);
}
