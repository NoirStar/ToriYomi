// ToriYomi - OCR 스레드 단위 테스트
// OcrThread와 IOcrEngine 통합 테스트

#include "core/ocr/ocr_thread.h"
#include "core/ocr/ocr_engine.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>

using namespace toriyomi;
using namespace toriyomi::ocr;

// Mock OCR 엔진 (테스트용)
class MockOcrEngine : public IOcrEngine {
public:
    bool initialized_ = false;
    int recognizeCallCount_ = 0;

    bool Initialize(const std::string& configPath, const std::string& language) override {
        initialized_ = true;
        return true;
    }

    std::vector<TextSegment> RecognizeText(const cv::Mat& image) override {
        recognizeCallCount_++;
        
        // 간단한 Mock 결과 반환
        std::vector<TextSegment> results;
        
        if (!image.empty()) {
            TextSegment segment;
            segment.text = "MockText";
            segment.boundingBox = cv::Rect(10, 10, 100, 30);
            segment.confidence = 95.0f;
            results.push_back(segment);
        }
        
        return results;
    }

    void Shutdown() override {
        initialized_ = false;
    }

    bool IsInitialized() const override {
        return initialized_;
    }

    std::string GetEngineName() const override {
        return "MockEngine";
    }
};

class OcrThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        frameQueue_ = std::make_shared<FrameQueue>(5);
        
        // Mock 엔진 생성 및 초기화
        auto mockEngine = std::make_unique<MockOcrEngine>();
        mockEngine->Initialize("", "");
        mockEnginePtr_ = mockEngine.get();  // 테스트를 위한 포인터 보관
        
        ocrThread_ = std::make_unique<OcrThread>(frameQueue_, std::move(mockEngine));
    }

    void TearDown() override {
        if (ocrThread_) {
            ocrThread_->Stop();
        }
    }

    std::shared_ptr<FrameQueue> frameQueue_;
    std::unique_ptr<OcrThread> ocrThread_;
    MockOcrEngine* mockEnginePtr_ = nullptr;  // 테스트 검증용
};

// 테스트 1: OCR 스레드 시작 및 정지
TEST_F(OcrThreadTest, StartAndStop) {
    EXPECT_FALSE(ocrThread_->IsRunning());
    
    bool started = ocrThread_->Start();
    EXPECT_TRUE(started);
    EXPECT_TRUE(ocrThread_->IsRunning());
    
    ocrThread_->Stop();
    EXPECT_FALSE(ocrThread_->IsRunning());
}

// 테스트 2: 초기화되지 않은 엔진으로 시작 실패
TEST_F(OcrThreadTest, StartWithoutInitializedEngine) {
    auto uninitEngine = std::make_unique<MockOcrEngine>();
    // Initialize 호출 안 함
    
    auto thread = std::make_unique<OcrThread>(frameQueue_, std::move(uninitEngine));
    
    bool started = thread->Start();
    EXPECT_FALSE(started);
    EXPECT_FALSE(thread->IsRunning());
}

// 테스트 3: FrameQueue에서 프레임 소비
TEST_F(OcrThreadTest, ConsumeFramesFromQueue) {
    ASSERT_TRUE(ocrThread_->Start());
    
    // 테스트 프레임 3개 추가
    for (int i = 0; i < 3; ++i) {
        cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
        frameQueue_->Push(frame);
    }
    
    // OCR 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Mock 엔진이 호출되었는지 확인
    EXPECT_GT(mockEnginePtr_->recognizeCallCount_, 0);
    
    // 큐가 비었는지 확인
    EXPECT_EQ(frameQueue_->Size(), 0);
    
    ocrThread_->Stop();
}

// 테스트 4: OCR 결과 반환
TEST_F(OcrThreadTest, GetLatestResults) {
    ASSERT_TRUE(ocrThread_->Start());
    
    // 테스트 프레임 추가
    cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
    frameQueue_->Push(frame);
    
    // OCR 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 결과 확인
    auto results = ocrThread_->GetLatestResults();
    EXPECT_GT(results.size(), 0);
    
    if (!results.empty()) {
        EXPECT_EQ(results[0].text, "MockText");
        EXPECT_FLOAT_EQ(results[0].confidence, 95.0f);
    }
    
    ocrThread_->Stop();
}

// 테스트 5: 통계 확인
TEST_F(OcrThreadTest, GetStatistics) {
    ASSERT_TRUE(ocrThread_->Start());
    
    // 여러 프레임 처리
    for (int i = 0; i < 5; ++i) {
        cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
        frameQueue_->Push(frame);
    }
    
    // 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    auto stats = ocrThread_->GetStatistics();
    
    // 프레임이 처리되었어야 함
    EXPECT_GT(stats.totalFramesProcessed, 0);
    EXPECT_EQ(stats.engineName, "MockEngine");
    EXPECT_GT(stats.totalTextSegments, 0);
    
    std::cout << "Processed: " << stats.totalFramesProcessed 
              << " frames, FPS: " << stats.currentFps
              << ", Segments: " << stats.totalTextSegments
              << ", Engine: " << stats.engineName << std::endl;
    
    ocrThread_->Stop();
}

// 테스트 6: 중복 시작 방지
TEST_F(OcrThreadTest, PreventDuplicateStart) {
    ASSERT_TRUE(ocrThread_->Start());
    EXPECT_TRUE(ocrThread_->IsRunning());
    
    // 두 번째 시작 시도는 실패해야 함
    bool secondStart = ocrThread_->Start();
    EXPECT_FALSE(secondStart);
    
    ocrThread_->Stop();
}

// 테스트 7: 빈 프레임 처리
TEST_F(OcrThreadTest, HandleEmptyFrame) {
    ASSERT_TRUE(ocrThread_->Start());
    
    // 빈 프레임 추가
    cv::Mat emptyFrame;
    frameQueue_->Push(emptyFrame);
    
    // 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 크래시 없이 처리되어야 함
    auto stats = ocrThread_->GetStatistics();
    EXPECT_GE(stats.totalFramesProcessed, 1);
    
    ocrThread_->Stop();
}

// 테스트 8: 스레드 안전성 - 동시 접근
TEST_F(OcrThreadTest, ThreadSafety) {
    ASSERT_TRUE(ocrThread_->Start());
    
    // 프레임 추가 스레드
    std::thread producer([this]() {
        for (int i = 0; i < 10; ++i) {
            cv::Mat frame(50, 50, CV_8UC3, cv::Scalar(128, 128, 128));
            frameQueue_->Push(frame);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // 결과 읽기 스레드
    std::thread consumer([this]() {
        for (int i = 0; i < 10; ++i) {
            auto results = ocrThread_->GetLatestResults();
            auto stats = ocrThread_->GetStatistics();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    producer.join();
    consumer.join();
    
    // 크래시 없이 완료되어야 함
    EXPECT_TRUE(ocrThread_->IsRunning());
    
    ocrThread_->Stop();
}
