// ToriYomi - Tesseract 래퍼 단위 테스트
// TesseractWrapper 클래스 및 OCR 인터페이스 테스트

#include "core/ocr/ocr_engine.h"
#include "core/ocr/tesseract_wrapper.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <filesystem>

using namespace toriyomi::ocr;
namespace fs = std::filesystem;

class TesseractWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // tessdata 경로 확인 (일반적인 vcpkg 설치 경로)
        tessdataPath_ = "C:/vcpkg/installed/x64-windows/share/tessdata";
        
        // 경로가 존재하지 않으면 테스트 스킵
        if (!fs::exists(tessdataPath_)) {
            GTEST_SKIP() << "tessdata not found at: " << tessdataPath_;
        }

        // jpn.traineddata 파일 확인
        std::string jpnData = tessdataPath_ + "/jpn.traineddata";
        if (!fs::exists(jpnData)) {
            GTEST_SKIP() << "Japanese trained data not found: " << jpnData;
        }
    }

    std::string tessdataPath_;
};

// 테스트 1: 초기화 성공
TEST_F(TesseractWrapperTest, Initialize) {
    TesseractWrapper wrapper;
    EXPECT_FALSE(wrapper.IsInitialized());

    bool result = wrapper.Initialize(tessdataPath_, "jpn");
    EXPECT_TRUE(result);
    EXPECT_TRUE(wrapper.IsInitialized());

    wrapper.Shutdown();
    EXPECT_FALSE(wrapper.IsInitialized());
}

// 테스트 2: 잘못된 경로로 초기화 실패
TEST_F(TesseractWrapperTest, InitializeInvalidPath) {
    TesseractWrapper wrapper;
    
    bool result = wrapper.Initialize("invalid_path", "jpn");
    EXPECT_FALSE(result);
    EXPECT_FALSE(wrapper.IsInitialized());
}

// 테스트 3: 초기화 없이 인식 시도
TEST_F(TesseractWrapperTest, RecognizeWithoutInitialize) {
    TesseractWrapper wrapper;
    
    cv::Mat testImage = cv::Mat::zeros(100, 100, CV_8UC3);
    auto results = wrapper.RecognizeText(testImage);
    
    EXPECT_TRUE(results.empty());
}

// 테스트 4: 빈 이미지 인식
TEST_F(TesseractWrapperTest, RecognizeEmptyImage) {
    TesseractWrapper wrapper;
    ASSERT_TRUE(wrapper.Initialize(tessdataPath_, "jpn"));
    
    cv::Mat emptyImage;
    auto results = wrapper.RecognizeText(emptyImage);
    
    EXPECT_TRUE(results.empty());
}

// 테스트 5: 단순 텍스트 이미지 인식
TEST_F(TesseractWrapperTest, RecognizeSimpleText) {
    TesseractWrapper wrapper;
    ASSERT_TRUE(wrapper.Initialize(tessdataPath_, "jpn"));

    // 흰 배경에 검은 텍스트 생성
    cv::Mat image(100, 300, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::putText(image, "Test", cv::Point(50, 60), 
                cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 0, 0), 2);

    auto results = wrapper.RecognizeText(image);

    // 최소한 하나의 결과가 있어야 함 (신뢰도에 따라 다를 수 있음)
    // 영문자는 jpn 모델에서 인식률이 낮을 수 있음
    std::cout << "Recognized " << results.size() << " text segments" << std::endl;
    
    for (const auto& segment : results) {
        std::cout << "Text: " << segment.text 
                  << ", Confidence: " << segment.confidence 
                  << ", Box: [" << segment.boundingBox.x << ","
                  << segment.boundingBox.y << ","
                  << segment.boundingBox.width << ","
                  << segment.boundingBox.height << "]" << std::endl;
    }
}

// 테스트 6: 일본어 텍스트 이미지 인식
TEST_F(TesseractWrapperTest, RecognizeJapaneseText) {
    TesseractWrapper wrapper;
    ASSERT_TRUE(wrapper.Initialize(tessdataPath_, "jpn"));

    // 흰 배경에 일본어 텍스트 생성
    // OpenCV의 putText는 한글/일본어를 지원하지 않으므로
    // 실제 게임 화면 캡처 테스트는 통합 테스트에서 수행
    cv::Mat image(200, 400, CV_8UC3, cv::Scalar(255, 255, 255));
    
    // 간단한 도형으로 텍스트 영역 시뮬레이션
    cv::rectangle(image, cv::Rect(50, 50, 100, 50), cv::Scalar(0, 0, 0), -1);
    
    auto results = wrapper.RecognizeText(image);
    
    // 결과 출력 (실제 텍스트 인식은 실제 일본어 이미지가 필요)
    std::cout << "Japanese test: Recognized " << results.size() << " segments" << std::endl;
}

// 테스트 7: 여러 번 초기화
TEST_F(TesseractWrapperTest, MultipleInitialize) {
    TesseractWrapper wrapper;
    
    EXPECT_TRUE(wrapper.Initialize(tessdataPath_, "jpn"));
    EXPECT_TRUE(wrapper.IsInitialized());
    
    // 재초기화
    EXPECT_TRUE(wrapper.Initialize(tessdataPath_, "jpn"));
    EXPECT_TRUE(wrapper.IsInitialized());
    
    wrapper.Shutdown();
}

// 테스트 8: TextSegment 구조체 확인
TEST_F(TesseractWrapperTest, TextSegmentStructure) {
    TextSegment segment;
    segment.text = "テスト";
    segment.boundingBox = cv::Rect(10, 20, 100, 50);
    segment.confidence = 95.5f;
    
    EXPECT_EQ(segment.text, "テスト");
    EXPECT_EQ(segment.boundingBox.x, 10);
    EXPECT_EQ(segment.boundingBox.y, 20);
    EXPECT_EQ(segment.boundingBox.width, 100);
    EXPECT_EQ(segment.boundingBox.height, 50);
    EXPECT_FLOAT_EQ(segment.confidence, 95.5f);
}

// 테스트 9: 팩토리 패턴으로 엔진 생성
TEST_F(TesseractWrapperTest, FactoryCreateEngine) {
    auto engine = OcrEngineFactory::CreateEngine(OcrEngineType::Tesseract);
    
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->GetEngineName(), "Tesseract");
    EXPECT_FALSE(engine->IsInitialized());
    
    bool result = engine->Initialize(tessdataPath_, "jpn");
    EXPECT_TRUE(result);
    EXPECT_TRUE(engine->IsInitialized());
    
    engine->Shutdown();
}

// 테스트 10: 인터페이스를 통한 다형성 사용
TEST_F(TesseractWrapperTest, PolymorphicUsage) {
    std::unique_ptr<IOcrEngine> engine = std::make_unique<TesseractWrapper>();
    
    EXPECT_EQ(engine->GetEngineName(), "Tesseract");
    
    bool result = engine->Initialize(tessdataPath_, "jpn");
    EXPECT_TRUE(result);
    
    cv::Mat testImage(100, 300, CV_8UC3, cv::Scalar(255, 255, 255));
    auto results = engine->RecognizeText(testImage);
    
    std::cout << "Polymorphic test: Recognized " << results.size() << " segments" << std::endl;
    
    engine->Shutdown();
}
