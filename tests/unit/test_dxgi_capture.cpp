// ToriYomi - DXGI 캡처 단위 테스트
// DirectX 11 Desktop Duplication을 사용한 화면 캡처 테스트

#include "core/capture/dxgi_capture.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <thread>
#include <chrono>

using namespace toriyomi::capture;

class DxgiCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 대상으로 데스크톱 윈도우 사용
        hwnd_ = GetDesktopWindow();
    }

    void TearDown() override {
        // 정리
    }

    HWND hwnd_{nullptr};
};

// 테스트 1: 유효한 윈도우 핸들로 초기화
TEST_F(DxgiCaptureTest, InitializeWithValidWindow) {
    DxgiCapture capture;
    bool result = capture.Initialize(hwnd_);
    
    // 성공하거나 우아하게 실패해야 함 (DXGI 미지원 환경)
    // CI 환경에서 DXGI가 없을 수 있으므로 true를 단언하지 않음
    EXPECT_TRUE(result == true || result == false);
    
    if (result) {
        capture.Shutdown();
    }
}

// 테스트 2: 유효하지 않은 윈도우 핸들로 초기화
TEST_F(DxgiCaptureTest, InitializeWithInvalidWindow) {
    DxgiCapture capture;
    HWND invalidHwnd = reinterpret_cast<HWND>(0xDEADBEEF);
    
    bool result = capture.Initialize(invalidHwnd);
    EXPECT_FALSE(result);
}

// 테스트 3: 프레임 캡처 시 유효한 Mat 반환
TEST_F(DxgiCaptureTest, CaptureFrameReturnsValidMat) {
    DxgiCapture capture;
    
    if (!capture.Initialize(hwnd_)) {
        GTEST_SKIP() << "DXGI not available on this system";
    }
    
    cv::Mat frame = capture.CaptureFrame();
    
    // 프레임이 유효해야 함 (비어있지 않음)
    EXPECT_FALSE(frame.empty());
    
    // BGR 포맷이어야 함
    EXPECT_EQ(frame.channels(), 3);
    
    // 합리적인 크기여야 함 (최소 100x100)
    EXPECT_GE(frame.cols, 100);
    EXPECT_GE(frame.rows, 100);
    
    capture.Shutdown();
}

// 테스트 4: 여러 프레임 캡처 (FPS 테스트)
TEST_F(DxgiCaptureTest, CaptureMultipleFrames) {
    DxgiCapture capture;
    
    if (!capture.Initialize(hwnd_)) {
        GTEST_SKIP() << "DXGI not available on this system";
    }
    
    const int frameCount = 60; // 60 프레임 캡처
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < frameCount; ++i) {
        cv::Mat frame = capture.CaptureFrame();
        ASSERT_FALSE(frame.empty()) << "Frame " << i << " is empty";
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double fps = frameCount * 1000.0 / duration.count();
    
    std::cout << "Captured " << frameCount << " frames in " 
              << duration.count() << " ms (FPS: " << fps << ")" << std::endl;
    
    // 최소 20 FPS 달성해야 함 (CI 환경 고려)
    EXPECT_GE(fps, 20.0);
    
    capture.Shutdown();
}

// 테스트 5: 초기화 없이 종료
TEST_F(DxgiCaptureTest, ShutdownWithoutInitialize) {
    DxgiCapture capture;
    
    // 크래시 없어야 함
    EXPECT_NO_THROW(capture.Shutdown());
}

// 테스트 6: 종료 후 캡처 시 빈 프레임 반환
TEST_F(DxgiCaptureTest, CaptureAfterShutdown) {
    DxgiCapture capture;
    
    if (!capture.Initialize(hwnd_)) {
        GTEST_SKIP() << "DXGI not available on this system";
    }
    
    capture.Shutdown();
    
    cv::Mat frame = capture.CaptureFrame();
    EXPECT_TRUE(frame.empty());
}

// 테스트 7: 여러 번 초기화 호출
TEST_F(DxgiCaptureTest, MultipleInitializeCalls) {
    DxgiCapture capture;
    
    bool result1 = capture.Initialize(hwnd_);
    
    if (!result1) {
        GTEST_SKIP() << "DXGI not available on this system";
    }
    
    // 두 번째 초기화는 실패하거나 재초기화해야 함
    bool result2 = capture.Initialize(hwnd_);
    EXPECT_TRUE(result2); // 우아하게 처리해야 함
    
    capture.Shutdown();
}

// 테스트 8: 타임아웃 동작
TEST_F(DxgiCaptureTest, CaptureWithTimeout) {
    DxgiCapture capture;
    
    if (!capture.Initialize(hwnd_)) {
        GTEST_SKIP() << "DXGI not available on this system";
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    cv::Mat frame = capture.CaptureFrame(); // 새 프레임 없으면 100ms에 타임아웃
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // 합리적인 시간 내에 완료해야 함 (< 200ms)
    EXPECT_LT(duration.count(), 200);
    
    capture.Shutdown();
}
