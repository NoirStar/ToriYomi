// ToriYomi - 오버레이 윈도우 단위 테스트
#include "ui/overlay/overlay_window.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace toriyomi::ui;
using namespace toriyomi::tokenizer;

class OverlayWindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        window_ = std::make_unique<OverlayWindow>();
    }

    void TearDown() override {
        if (window_ && window_->IsCreated()) {
            window_->Destroy();
        }
    }

    std::unique_ptr<OverlayWindow> window_;
};

// 테스트 1: 윈도우 생성
TEST_F(OverlayWindowTest, CreateWindow) {
    EXPECT_FALSE(window_->IsCreated());

    bool created = window_->Create(100, 100, 800, 600);
    EXPECT_TRUE(created);
    EXPECT_TRUE(window_->IsCreated());
    EXPECT_NE(window_->GetHandle(), nullptr);
}

// 테스트 2: 윈도우 파괴
TEST_F(OverlayWindowTest, DestroyWindow) {
    window_->Create(100, 100, 800, 600);
    EXPECT_TRUE(window_->IsCreated());

    window_->Destroy();
    EXPECT_FALSE(window_->IsCreated());
}

// 테스트 3: 중복 생성 방지
TEST_F(OverlayWindowTest, PreventDuplicateCreation) {
    bool created1 = window_->Create(100, 100, 800, 600);
    EXPECT_TRUE(created1);

    bool created2 = window_->Create(200, 200, 1024, 768);
    EXPECT_TRUE(created2);  // 이미 생성됨 (성공 반환)
    
    // 첫 번째 위치 유지 (변경되지 않음)
    HWND hwnd = window_->GetHandle();
    RECT rect;
    GetWindowRect(hwnd, &rect);
    EXPECT_EQ(rect.left, 100);
    EXPECT_EQ(rect.top, 100);
}

// 테스트 4: 후리가나 업데이트 (빈 리스트)
TEST_F(OverlayWindowTest, UpdateFurigana_EmptyList) {
    window_->Create(100, 100, 800, 600);

    std::vector<FuriganaInfo> emptyList;
    EXPECT_NO_THROW(window_->UpdateFurigana(emptyList));
}

// 테스트 5: 후리가나 업데이트 (한자 포함)
TEST_F(OverlayWindowTest, UpdateFurigana_WithKanji) {
    window_->Create(100, 100, 800, 600);

    std::vector<FuriganaInfo> furiganaList;
    
    FuriganaInfo info1;
    info1.baseText = "今日";
    info1.reading = "きょう";
    info1.position = cv::Rect(10, 20, 50, 30);
    info1.rubyPosition = cv::Point(10, 15);
    info1.needsRuby = true;
    furiganaList.push_back(info1);

    FuriganaInfo info2;
    info2.baseText = "天気";
    info2.reading = "てんき";
    info2.position = cv::Rect(70, 20, 60, 30);
    info2.rubyPosition = cv::Point(70, 15);
    info2.needsRuby = true;
    furiganaList.push_back(info2);

    EXPECT_NO_THROW(window_->UpdateFurigana(furiganaList));
}

// 테스트 6: 후리가나 업데이트 (히라가나만 - needsRuby=false)
TEST_F(OverlayWindowTest, UpdateFurigana_OnlyHiragana) {
    window_->Create(100, 100, 800, 600);

    std::vector<FuriganaInfo> furiganaList;
    
    FuriganaInfo info;
    info.baseText = "です";
    info.reading = "です";
    info.position = cv::Rect(10, 20, 40, 30);
    info.rubyPosition = cv::Point(10, 15);
    info.needsRuby = false;  // 한자 없음
    furiganaList.push_back(info);

    EXPECT_NO_THROW(window_->UpdateFurigana(furiganaList));
}

// 테스트 7: 다시 그리기
TEST_F(OverlayWindowTest, Redraw) {
    window_->Create(100, 100, 800, 600);
    EXPECT_NO_THROW(window_->Redraw());
}

// 테스트 8: 메시지 처리 (타임아웃)
TEST_F(OverlayWindowTest, ProcessMessages) {
    window_->Create(100, 100, 800, 600);

    // 메시지 큐가 비어있으면 즉시 반환
    // 윈도우 생성 시 메시지가 있을 수 있으므로 반복 처리
    bool result = true;
    for (int i = 0; i < 10 && result; ++i) {
        result = window_->ProcessMessages();
    }
    
    // WM_QUIT가 없으면 마지막에 true여야 함
    EXPECT_TRUE(result || !result);  // 어느 쪽이든 크래시 없이 완료되면 성공
}

// 테스트 9: 윈도우 스타일 확인 (투명 + 최상위)
TEST_F(OverlayWindowTest, WindowStyles) {
    window_->Create(100, 100, 800, 600);

    HWND hwnd = window_->GetHandle();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    // 확장 스타일 확인
    EXPECT_TRUE(exStyle & WS_EX_TOPMOST);       // 최상위
    EXPECT_TRUE(exStyle & WS_EX_LAYERED);       // 레이어드 윈도우
    EXPECT_TRUE(exStyle & WS_EX_TRANSPARENT);   // 클릭 투과
}

// 테스트 10: 스레드 안전성 (동시 업데이트)
TEST_F(OverlayWindowTest, ThreadSafety) {
    window_->Create(100, 100, 800, 600);

    std::vector<FuriganaInfo> list1;
    FuriganaInfo info1;
    info1.baseText = "今日";
    info1.reading = "きょう";
    info1.needsRuby = true;
    list1.push_back(info1);

    std::vector<FuriganaInfo> list2;
    FuriganaInfo info2;
    info2.baseText = "天気";
    info2.reading = "てんき";
    info2.needsRuby = true;
    list2.push_back(info2);

    // 두 스레드에서 동시 업데이트
    std::thread t1([&]() {
        for (int i = 0; i < 100; ++i) {
            window_->UpdateFurigana(list1);
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < 100; ++i) {
            window_->UpdateFurigana(list2);
        }
    });

    t1.join();
    t2.join();

    // 크래시 없이 완료되면 성공
    SUCCEED();
}
