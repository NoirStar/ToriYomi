// ToriYomi - GDI 캡처 단위 테스트
// GDI BitBlt를 사용한 화면 캡처 테스트 (DXGI 폴백용)

#include "core/capture/gdi_capture.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <thread>
#include <chrono>

using namespace toriyomi::capture;

class GdiCaptureTest : public ::testing::Test {
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
TEST_F(GdiCaptureTest, InitializeWithValidWindow) {
    GdiCapture capture;
    bool result = capture.Initialize(hwnd_);
    
    // GDI는 거의 모든 환경에서 작동해야 함
    EXPECT_TRUE(result);
    
    if (result) {
        capture.Shutdown();
    }
}

// 테스트 2: 유효하지 않은 윈도우 핸들로 초기화
TEST_F(GdiCaptureTest, InitializeWithInvalidWindow) {
    GdiCapture capture;
    HWND invalidHwnd = reinterpret_cast<HWND>(0xDEADBEEF);
    
    bool result = capture.Initialize(invalidHwnd);
    EXPECT_FALSE(result);
}

// 테스트 3: 프레임 캡처 시 유효한 Mat 반환
TEST_F(GdiCaptureTest, CaptureFrameReturnsValidMat) {
    GdiCapture capture;
    
    ASSERT_TRUE(capture.Initialize(hwnd_));
    
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
TEST_F(GdiCaptureTest, CaptureMultipleFrames) {
    GdiCapture capture;
    
    ASSERT_TRUE(capture.Initialize(hwnd_));
    
    const int frameCount = 30; // 30 프레임 캡처 (GDI는 DXGI보다 느림)
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
    
    // GDI는 느리므로 최소 10 FPS만 요구
    EXPECT_GE(fps, 10.0);
    
    capture.Shutdown();
}

// 테스트 5: 초기화 없이 종료
TEST_F(GdiCaptureTest, ShutdownWithoutInitialize) {
    GdiCapture capture;
    
    // 크래시 없어야 함
    EXPECT_NO_THROW(capture.Shutdown());
}

// 테스트 6: 종료 후 캡처 시 빈 프레임 반환
TEST_F(GdiCaptureTest, CaptureAfterShutdown) {
    GdiCapture capture;
    
    ASSERT_TRUE(capture.Initialize(hwnd_));
    
    capture.Shutdown();
    
    cv::Mat frame = capture.CaptureFrame();
    EXPECT_TRUE(frame.empty());
}

// 테스트 7: 여러 번 초기화 호출
TEST_F(GdiCaptureTest, MultipleInitializeCalls) {
    GdiCapture capture;
    
    bool result1 = capture.Initialize(hwnd_);
    ASSERT_TRUE(result1);
    
    // 두 번째 초기화도 성공해야 함 (재초기화)
    bool result2 = capture.Initialize(hwnd_);
    EXPECT_TRUE(result2);
    
    capture.Shutdown();
}

// 테스트 8: NULL 윈도우 핸들
TEST_F(GdiCaptureTest, InitializeWithNullWindow) {
    GdiCapture capture;
    
    bool result = capture.Initialize(nullptr);
    EXPECT_FALSE(result);
}

// 테스트 9: 초기화 상태 확인
TEST_F(GdiCaptureTest, IsInitializedCheck) {
    GdiCapture capture;
    
    EXPECT_FALSE(capture.IsInitialized());
    
    capture.Initialize(hwnd_);
    EXPECT_TRUE(capture.IsInitialized());
    
    capture.Shutdown();
    EXPECT_FALSE(capture.IsInitialized());
}
