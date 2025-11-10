// ToriYomi - 캡처 스레드 단위 테스트
// CaptureThread와 FrameQueue 통합 테스트

#include "core/capture/capture_thread.h"
#include "core/capture/frame_queue.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <thread>
#include <chrono>

using namespace toriyomi::capture;

class CaptureThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 대상으로 데스크톱 윈도우 사용
        hwnd_ = GetDesktopWindow();
        frameQueue_ = std::make_shared<toriyomi::FrameQueue>(5);
    }

    void TearDown() override {
        frameQueue_.reset();
    }

    HWND hwnd_{nullptr};
    std::shared_ptr<toriyomi::FrameQueue> frameQueue_;
};

// 테스트 1: 스레드 시작 및 정지
TEST_F(CaptureThreadTest, StartAndStop) {
    CaptureThread captureThread(frameQueue_);
    
    bool result = captureThread.Start(hwnd_);
    EXPECT_TRUE(result);
    EXPECT_TRUE(captureThread.IsRunning());
    
    captureThread.Stop();
    EXPECT_FALSE(captureThread.IsRunning());
}

// 테스트 2: 프레임이 큐에 푸시됨
TEST_F(CaptureThreadTest, FramesPushedToQueue) {
    CaptureThread captureThread(frameQueue_);
    
    ASSERT_TRUE(captureThread.Start(hwnd_));
    
    // 스레드가 프레임을 캡처할 시간을 줌
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 큐에 프레임이 있어야 함
    EXPECT_GT(frameQueue_->Size(), 0);
    
    // 프레임 꺼내기
    auto frame = frameQueue_->Pop(100);
    ASSERT_TRUE(frame.has_value());
    EXPECT_FALSE(frame->empty());
    
    captureThread.Stop();
}

// 테스트 3: 통계 확인
TEST_F(CaptureThreadTest, GetStatistics) {
    CaptureThread captureThread(frameQueue_);
    
    ASSERT_TRUE(captureThread.Start(hwnd_));
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // FPS 업데이트 대기
    
    auto stats = captureThread.GetStatistics();
    
    // 프레임이 캡처되었어야 함
    EXPECT_GT(stats.totalFramesCaptured, 0);
    // FPS는 1초 후부터 업데이트되므로 0 이상
    EXPECT_GE(stats.currentFps, 0.0);
    
    std::cout << "Captured: " << stats.totalFramesCaptured 
              << " frames, FPS: " << stats.currentFps 
              << ", Using: " << (stats.usingDxgi ? "DXGI" : "GDI") << std::endl;
    
    captureThread.Stop();
}

