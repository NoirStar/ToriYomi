// ToriYomi - 일본어 토크나이저 단위 테스트
// JapaneseTokenizer 클래스 테스트

#include "core/tokenizer/japanese_tokenizer.h"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

using namespace toriyomi::tokenizer;
using namespace toriyomi::ocr;

class JapaneseTokenizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tokenizer_ = std::make_unique<JapaneseTokenizer>();
    }

    void TearDown() override {
        if (tokenizer_) {
            tokenizer_->Shutdown();
        }
    }

    std::unique_ptr<JapaneseTokenizer> tokenizer_;
};

// 테스트 1: 초기화
TEST_F(JapaneseTokenizerTest, Initialize) {
    EXPECT_FALSE(tokenizer_->IsInitialized());
    
    bool result = tokenizer_->Initialize();
    EXPECT_TRUE(result);
    EXPECT_TRUE(tokenizer_->IsInitialized());
    
    tokenizer_->Shutdown();
    EXPECT_FALSE(tokenizer_->IsInitialized());
}

// 테스트 2: 간단한 일본어 텍스트 분석
TEST_F(JapaneseTokenizerTest, TokenizeSimpleJapanese) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    // "今日は良い天気です" (오늘은 좋은 날씨입니다)
    std::string text = "今日は良い天気です";
    auto tokens = tokenizer_->Tokenize(text);
    
    EXPECT_GT(tokens.size(), 0);
    
    std::cout << "Tokenized '" << text << "' into " << tokens.size() << " tokens:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "  Surface: " << token.surface 
                  << ", Reading: " << token.reading
                  << ", POS: " << token.partOfSpeech << std::endl;
    }
}

// 테스트 3: 히라가나 텍스트 분석
TEST_F(JapaneseTokenizerTest, TokenizeHiragana) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    std::string text = "こんにちは";  // 안녕하세요
    auto tokens = tokenizer_->Tokenize(text);
    
    EXPECT_GT(tokens.size(), 0);
    
    // 히라가나는 읽기가 자기 자신이어야 함
    for (const auto& token : tokens) {
        EXPECT_FALSE(token.surface.empty());
        EXPECT_FALSE(token.reading.empty());
    }
}

// 테스트 4: 가타카나 텍스트 분석
TEST_F(JapaneseTokenizerTest, TokenizeKatakana) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    std::string text = "コンピュータ";  // 컴퓨터
    auto tokens = tokenizer_->Tokenize(text);
    
    EXPECT_GT(tokens.size(), 0);
    
    std::cout << "Katakana tokenization:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "  " << token.surface << " -> " << token.reading << std::endl;
    }
}

// 테스트 5: 혼합 텍스트 분석 (한자+히라가나)
TEST_F(JapaneseTokenizerTest, TokenizeMixedText) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    std::string text = "私は学生です";  // 나는 학생입니다
    auto tokens = tokenizer_->Tokenize(text);
    
    EXPECT_GT(tokens.size(), 3);  // 최소 3개 이상의 토큰
    
    // 각 토큰이 읽기 정보를 가져야 함
    for (const auto& token : tokens) {
        EXPECT_FALSE(token.surface.empty());
        EXPECT_FALSE(token.reading.empty());
    }
}

// 테스트 6: 빈 텍스트 처리
TEST_F(JapaneseTokenizerTest, TokenizeEmptyText) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    auto tokens = tokenizer_->Tokenize("");
    EXPECT_EQ(tokens.size(), 0);
}

// 테스트 7: 초기화 없이 분석 시도
TEST_F(JapaneseTokenizerTest, TokenizeWithoutInitialize) {
    auto tokens = tokenizer_->Tokenize("テスト");
    EXPECT_EQ(tokens.size(), 0);
}

// 테스트 8: OCR 결과와 함께 토큰화 (위치 정보 포함)
TEST_F(JapaneseTokenizerTest, TokenizeWithPosition) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    TextSegment segment;
    segment.text = "今日は晴れ";  // 오늘은 맑음
    segment.boundingBox = cv::Rect(100, 50, 200, 30);
    segment.confidence = 90.0f;
    
    auto tokens = tokenizer_->TokenizeWithPosition(segment);
    
    EXPECT_GT(tokens.size(), 0);
    
    // 각 토큰이 boundingBox를 가져야 함
    for (const auto& token : tokens) {
        EXPECT_GT(token.boundingBox.width, 0);
        EXPECT_GT(token.boundingBox.height, 0);
        EXPECT_FLOAT_EQ(token.confidence, 90.0f);
        
        std::cout << "Token: " << token.surface 
                  << " at [" << token.boundingBox.x << "," << token.boundingBox.y
                  << "," << token.boundingBox.width << "," << token.boundingBox.height << "]"
                  << " confidence: " << token.confidence << std::endl;
    }
}

// 테스트 9: 여러 OCR 결과 일괄 처리
TEST_F(JapaneseTokenizerTest, TokenizeBatch) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    std::vector<TextSegment> segments;
    
    TextSegment seg1;
    seg1.text = "今日";
    seg1.boundingBox = cv::Rect(10, 10, 50, 20);
    seg1.confidence = 95.0f;
    segments.push_back(seg1);
    
    TextSegment seg2;
    seg2.text = "明日";
    seg2.boundingBox = cv::Rect(70, 10, 50, 20);
    seg2.confidence = 92.0f;
    segments.push_back(seg2);
    
    auto allTokens = tokenizer_->TokenizeBatch(segments);
    
    EXPECT_GT(allTokens.size(), 0);
    
    std::cout << "Batch tokenization: " << allTokens.size() << " tokens" << std::endl;
    for (const auto& token : allTokens) {
        std::cout << "  " << token.surface << " (" << token.reading << ")" << std::endl;
    }
}

// 테스트 10: Token 구조체 필드 확인
TEST_F(JapaneseTokenizerTest, TokenStructureFields) {
    ASSERT_TRUE(tokenizer_->Initialize());
    
    std::string text = "食べる";  // 먹다
    auto tokens = tokenizer_->Tokenize(text);
    
    ASSERT_GT(tokens.size(), 0);
    
    const auto& token = tokens[0];
    
    // 모든 필드가 설정되어야 함
    EXPECT_FALSE(token.surface.empty());
    EXPECT_FALSE(token.reading.empty());
    EXPECT_FALSE(token.baseForm.empty());
    EXPECT_FALSE(token.partOfSpeech.empty());
}

// 테스트 11: 재초기화
TEST_F(JapaneseTokenizerTest, Reinitialize) {
    EXPECT_TRUE(tokenizer_->Initialize());
    EXPECT_TRUE(tokenizer_->IsInitialized());
    
    // 재초기화
    EXPECT_TRUE(tokenizer_->Initialize());
    EXPECT_TRUE(tokenizer_->IsInitialized());
    
    // 여전히 작동해야 함
    auto tokens = tokenizer_->Tokenize("テスト");
    EXPECT_GT(tokens.size(), 0);
}
