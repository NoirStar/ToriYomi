// ToriYomi - PaddleOCR 래퍼 단위 테스트
// PaddleOcrWrapper가 기본 엔진으로 동작하도록 하는 회귀 테스트

#include "core/ocr/paddle_ocr_wrapper.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

using namespace toriyomi::ocr;

namespace {

cv::Mat CreateSyntheticImage(int width = 64, int height = 32) {
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(255, 255, 255));
    const std::string kText = "paddle";
    const cv::Point origin(4, height - 8);
    cv::putText(image, kText, origin, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
    return image;
}

}  // namespace

TEST(PaddleOcrWrapperTest, ReportsEngineName) {
    PaddleOcrWrapper wrapper;
    EXPECT_EQ(wrapper.GetEngineName(), "PaddleOCR");
}

TEST(PaddleOcrWrapperTest, InitializeFailsWithEmptyModelDir) {
    PaddleOcrWrapper wrapper;
    EXPECT_FALSE(wrapper.Initialize("", "jpn"));
    EXPECT_FALSE(wrapper.IsInitialized());
    EXPECT_FALSE(wrapper.GetLastError().empty());
}

TEST(PaddleOcrWrapperTest, RecognizeWithoutInitializeReturnsEmpty) {
    PaddleOcrWrapper wrapper;
    cv::Mat image = CreateSyntheticImage();
    auto results = wrapper.RecognizeText(image);
    EXPECT_TRUE(results.empty());
}

TEST(PaddleOcrWrapperTest, InitializeFailsForMissingModels) {
    PaddleOcrWrapper wrapper;
    EXPECT_FALSE(wrapper.Initialize("Z:/missing/paddle/models", "jpn"));
    EXPECT_FALSE(wrapper.IsInitialized());
}
