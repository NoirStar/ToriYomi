// ToriYomi - 오버레이 스레드 단위 테스트
#include "ui/overlay/overlay_thread.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace toriyomi::ui;
using namespace toriyomi::tokenizer;

class OverlayThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        thread_ = std::make_unique<OverlayThread>();
    }

    void TearDown() override {
        if (thread_ && thread_->IsRunning()) {
            thread_->Stop();
        }
    }

    std::unique_ptr<OverlayThread> thread_;
};

// 테스트 1: 스레드 시작
TEST_F(OverlayThreadTest, StartThread) {
    EXPECT_FALSE(thread_->IsRunning());

    bool started = thread_->Start(100, 100, 800, 600);
    EXPECT_TRUE(started);
    EXPECT_TRUE(thread_->IsRunning());
}

// 테스트 2: 스레드 정지
TEST_F(OverlayThreadTest, StopThread) {
    thread_->Start(100, 100, 800, 600);
    EXPECT_TRUE(thread_->IsRunning());

    thread_->Stop();
    EXPECT_FALSE(thread_->IsRunning());
}

// 테스트 3: 중복 시작 방지
TEST_F(OverlayThreadTest, PreventDuplicateStart) {
    bool started1 = thread_->Start(100, 100, 800, 600);
    EXPECT_TRUE(started1);

    bool started2 = thread_->Start(200, 200, 1024, 768);
    EXPECT_TRUE(started2);  // 이미 실행 중 (성공 반환)
}

// 테스트 4: 후리가나 업데이트
TEST_F(OverlayThreadTest, UpdateFurigana) {
    thread_->Start(100, 100, 800, 600);

    std::vector<FuriganaInfo> furiganaList;
    
    FuriganaInfo info1;
    info1.baseText = "今日";
    info1.reading = "きょう";
    info1.position = cv::Rect(10, 20, 50, 30);
    info1.rubyPosition = cv::Point(10, 15);
    info1.needsRuby = true;
    furiganaList.push_back(info1);

    EXPECT_NO_THROW(thread_->UpdateFurigana(furiganaList));

    // 약간 대기 (렌더링 스레드가 처리할 시간)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto stats = thread_->GetStats();
    EXPECT_GT(stats.updateCount, 0);
}

// 테스트 5: 빈 후리가나 업데이트
TEST_F(OverlayThreadTest, UpdateEmptyFurigana) {
    thread_->Start(100, 100, 800, 600);

    std::vector<FuriganaInfo> emptyList;
    EXPECT_NO_THROW(thread_->UpdateFurigana(emptyList));
}

// 테스트 6: 연속 업데이트
TEST_F(OverlayThreadTest, MultipleUpdates) {
    thread_->Start(100, 100, 800, 600);

    for (int i = 0; i < 10; ++i) {
        std::vector<FuriganaInfo> furiganaList;
        
        FuriganaInfo info;
        info.baseText = "天気";
        info.reading = "てんき";
        info.needsRuby = true;
        furiganaList.push_back(info);

        thread_->UpdateFurigana(furiganaList);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto stats = thread_->GetStats();
    EXPECT_EQ(stats.updateCount, 10);
}

// 테스트 7: FPS 측정
TEST_F(OverlayThreadTest, FpsMeasurement) {
    thread_->Start(100, 100, 800, 600);

    // 1.5초 대기 (FPS 계산 안정화 시간 포함)
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    auto stats = thread_->GetStats();
    
    // 30 FPS 이상이면 성공 (윈도우 생성 오버헤드 고려)
    EXPECT_GT(stats.averageFps, 30.0);
    EXPECT_LT(stats.averageFps, 70.0);
    EXPECT_GT(stats.frameCount, 30);  // 최소 30 프레임
}

// 테스트 8: 통계 초기값
TEST_F(OverlayThreadTest, InitialStats) {
    auto stats = thread_->GetStats();
    
    EXPECT_EQ(stats.frameCount, 0);
    EXPECT_EQ(stats.updateCount, 0);
    EXPECT_EQ(stats.averageFps, 0.0);
}

// 테스트 9: 정지 후 통계 유지
TEST_F(OverlayThreadTest, StatsAfterStop) {
    thread_->Start(100, 100, 800, 600);
    
    std::vector<FuriganaInfo> furiganaList;
    FuriganaInfo info;
    info.baseText = "今日";
    info.reading = "きょう";
    info.needsRuby = true;
    furiganaList.push_back(info);
    
    thread_->UpdateFurigana(furiganaList);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto statsBefore = thread_->GetStats();
    
    thread_->Stop();
    
    auto statsAfter = thread_->GetStats();
    
    // 통계는 유지되어야 함
    EXPECT_EQ(statsAfter.frameCount, statsBefore.frameCount);
    EXPECT_EQ(statsAfter.updateCount, statsBefore.updateCount);
}

// 테스트 10: 스레드 안전성 (동시 업데이트)
TEST_F(OverlayThreadTest, ThreadSafety) {
    thread_->Start(100, 100, 800, 600);

    // 두 스레드에서 동시 업데이트
    std::thread t1([this]() {
        for (int i = 0; i < 50; ++i) {
            std::vector<FuriganaInfo> furiganaList;
            FuriganaInfo info;
            info.baseText = "今日";
            info.reading = "きょう";
            info.needsRuby = true;
            furiganaList.push_back(info);
            thread_->UpdateFurigana(furiganaList);
        }
    });

    std::thread t2([this]() {
        for (int i = 0; i < 50; ++i) {
            std::vector<FuriganaInfo> furiganaList;
            FuriganaInfo info;
            info.baseText = "天気";
            info.reading = "てんき";
            info.needsRuby = true;
            furiganaList.push_back(info);
            thread_->UpdateFurigana(furiganaList);
        }
    });

    t1.join();
    t2.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto stats = thread_->GetStats();
    EXPECT_EQ(stats.updateCount, 100);
}

// 테스트 11: 재시작
TEST_F(OverlayThreadTest, Restart) {
    // 첫 번째 실행
    thread_->Start(100, 100, 800, 600);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread_->Stop();

    // 두 번째 실행
    bool restarted = thread_->Start(200, 200, 1024, 768);
    EXPECT_TRUE(restarted);
    EXPECT_TRUE(thread_->IsRunning());
}
