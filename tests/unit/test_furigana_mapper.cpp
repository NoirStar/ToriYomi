// ToriYomi - 후리가나 매퍼 단위 테스트

#include "core/tokenizer/furigana_mapper.h"
#include "core/tokenizer/japanese_tokenizer.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

using namespace toriyomi::tokenizer;

class FuriganaMapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        mapper_ = std::make_unique<FuriganaMapper>();
    }

    std::unique_ptr<FuriganaMapper> mapper_;
};

// 테스트 1: 한자 검출
TEST_F(FuriganaMapperTest, ContainsKanji_WithKanji) {
    EXPECT_TRUE(FuriganaMapper::ContainsKanji("今日"));      // 한자
    EXPECT_TRUE(FuriganaMapper::ContainsKanji("天気"));      // 한자
    EXPECT_TRUE(FuriganaMapper::ContainsKanji("今日は"));    // 한자 + 히라가나
    EXPECT_TRUE(FuriganaMapper::ContainsKanji("良い天気"));  // 히라가나 + 한자
}

// 테스트 2: 한자 없음
TEST_F(FuriganaMapperTest, ContainsKanji_WithoutKanji) {
    EXPECT_FALSE(FuriganaMapper::ContainsKanji("こんにちは"));  // 히라가나
    EXPECT_FALSE(FuriganaMapper::ContainsKanji("カタカナ"));    // 가타카나
    EXPECT_FALSE(FuriganaMapper::ContainsKanji("です"));        // 히라가나
    EXPECT_FALSE(FuriganaMapper::ContainsKanji(""));            // 빈 문자열
}

// 테스트 3: 단일 토큰 매핑 (한자 포함)
TEST_F(FuriganaMapperTest, MapTokenToFurigana_WithKanji) {
    Token token;
    token.surface = "今日";
    token.reading = "キョウ";  // MeCab은 가타카나로 반환
    token.boundingBox = cv::Rect(100, 50, 80, 30);
    token.confidence = 95.0f;

    auto info = mapper_->MapTokenToFurigana(token);

    EXPECT_EQ(info.baseText, "今日");
    EXPECT_EQ(info.reading, "きょう");  // 히라가나로 변환되어야 함
    EXPECT_EQ(info.position, cv::Rect(100, 50, 80, 30));
    EXPECT_TRUE(info.needsRuby);  // 한자가 있으므로 후리가나 필요
    EXPECT_EQ(info.rubyPosition.x, 100);
    EXPECT_EQ(info.rubyPosition.y, 45);  // 50 - 5 (기본 오프셋)
}

// 테스트 4: 단일 토큰 매핑 (한자 없음)
TEST_F(FuriganaMapperTest, MapTokenToFurigana_WithoutKanji) {
    Token token;
    token.surface = "です";
    token.reading = "デス";  // MeCab은 가타카나로 반환
    token.boundingBox = cv::Rect(200, 50, 40, 30);

    auto info = mapper_->MapTokenToFurigana(token);

    EXPECT_EQ(info.baseText, "です");
    EXPECT_EQ(info.reading, "です");  // 히라가나로 변환
    EXPECT_FALSE(info.needsRuby);  // 한자가 없으므로 후리가나 불필요
}

// 테스트 5: 여러 토큰 일괄 매핑
TEST_F(FuriganaMapperTest, MapTokensToFurigana) {
    std::vector<Token> tokens;

    Token token1;
    token1.surface = "今日";
    token1.reading = "キョウ";
    token1.boundingBox = cv::Rect(10, 10, 50, 20);
    tokens.push_back(token1);

    Token token2;
    token2.surface = "は";
    token2.reading = "ハ";
    token2.boundingBox = cv::Rect(70, 10, 20, 20);
    tokens.push_back(token2);

    Token token3;
    token3.surface = "晴れ";
    token3.reading = "ハレ";
    token3.boundingBox = cv::Rect(100, 10, 50, 20);
    tokens.push_back(token3);

    auto furiganaList = mapper_->MapTokensToFurigana(tokens);

    ASSERT_EQ(furiganaList.size(), 3);

    // 토큰 1: 한자
    EXPECT_TRUE(furiganaList[0].needsRuby);
    EXPECT_EQ(furiganaList[0].baseText, "今日");
    EXPECT_EQ(furiganaList[0].reading, "きょう");  // 히라가나로 변환

    // 토큰 2: 히라가나
    EXPECT_FALSE(furiganaList[1].needsRuby);
    EXPECT_EQ(furiganaList[1].baseText, "は");

    // 토큰 3: 한자
    EXPECT_TRUE(furiganaList[2].needsRuby);
    EXPECT_EQ(furiganaList[2].baseText, "晴れ");
    EXPECT_EQ(furiganaList[2].reading, "はれ");  // 히라가나로 변환
}

// 테스트 6: 루비 위치 계산
TEST_F(FuriganaMapperTest, CalculateRubyPosition) {
    cv::Rect basePos(100, 50, 80, 30);

    // 기본 오프셋 (5픽셀)
    auto ruby1 = FuriganaMapper::CalculateRubyPosition(basePos);
    EXPECT_EQ(ruby1.x, 100);
    EXPECT_EQ(ruby1.y, 45);

    // 커스텀 오프셋 (10픽셀)
    auto ruby2 = FuriganaMapper::CalculateRubyPosition(basePos, 10);
    EXPECT_EQ(ruby2.x, 100);
    EXPECT_EQ(ruby2.y, 40);

    // 오프셋 0
    auto ruby3 = FuriganaMapper::CalculateRubyPosition(basePos, 0);
    EXPECT_EQ(ruby3.x, 100);
    EXPECT_EQ(ruby3.y, 50);
}

// 테스트 7: 루비 오프셋 설정
TEST_F(FuriganaMapperTest, SetGetRubyOffset) {
    EXPECT_EQ(mapper_->GetRubyOffset(), 5);  // 기본값

    mapper_->SetRubyOffset(10);
    EXPECT_EQ(mapper_->GetRubyOffset(), 10);

    mapper_->SetRubyOffset(20);
    EXPECT_EQ(mapper_->GetRubyOffset(), 20);
}

// 테스트 8: 루비 오프셋 적용 확인
TEST_F(FuriganaMapperTest, RubyOffsetApplied) {
    mapper_->SetRubyOffset(15);

    Token token;
    token.surface = "天気";
    token.reading = "テンキ";  // MeCab은 가타카나로 반환
    token.boundingBox = cv::Rect(100, 100, 60, 25);

    auto info = mapper_->MapTokenToFurigana(token);

    EXPECT_TRUE(info.needsRuby);
    EXPECT_EQ(info.reading, "てんき");  // 히라가나로 변환
    EXPECT_EQ(info.rubyPosition.x, 100);
    EXPECT_EQ(info.rubyPosition.y, 85);  // 100 - 15
}

// 테스트 9: 빈 토큰 리스트
TEST_F(FuriganaMapperTest, MapEmptyTokenList) {
    std::vector<Token> tokens;
    auto furiganaList = mapper_->MapTokensToFurigana(tokens);

    EXPECT_EQ(furiganaList.size(), 0);
}

// 테스트 10: 혼합 텍스트 (한자+히라가나)
TEST_F(FuriganaMapperTest, MixedKanjiHiragana) {
    EXPECT_TRUE(FuriganaMapper::ContainsKanji("食べる"));  // 한자 + 히라가나
    EXPECT_TRUE(FuriganaMapper::ContainsKanji("飲み物"));  // 한자
    EXPECT_FALSE(FuriganaMapper::ContainsKanji("たべる"));  // 히라가나만
}

// 테스트 11: 경계값 테스트
TEST_F(FuriganaMapperTest, EdgeCases) {
    Token token;
    token.surface = "";
    token.reading = "";
    token.boundingBox = cv::Rect(0, 0, 0, 0);

    auto info = mapper_->MapTokenToFurigana(token);

    EXPECT_EQ(info.baseText, "");
    EXPECT_EQ(info.reading, "");
    EXPECT_FALSE(info.needsRuby);
}
