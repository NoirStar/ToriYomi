// ToriYomi - 일본어 토크나이저 구현
// MeCab을 사용한 형태소 분석

#include "japanese_tokenizer.h"
#include <mecab.h>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace toriyomi {
namespace tokenizer {

// Pimpl 구현
class JapaneseTokenizer::Impl {
public:
    MeCab::Tagger* tagger = nullptr;
    bool initialized = false;

    /**
     * @brief 텍스트 길이 기반으로 boundingBox를 분할
     * 
     * OCR 결과의 전체 boundingBox를 각 토큰의 문자 수에 비례하여 분할
     */
    cv::Rect CalculateTokenBoundingBox(const cv::Rect& totalBox, 
                                        int textLength,
                                        int tokenStartPos,
                                        int tokenLength) {
        if (textLength == 0 || tokenLength == 0) {
            return cv::Rect(0, 0, 0, 0);
        }

        // 문자당 평균 너비
        float charWidth = static_cast<float>(totalBox.width) / textLength;
        
        // 토큰의 시작 X 좌표
        int startX = totalBox.x + static_cast<int>(charWidth * tokenStartPos);
        
        // 토큰의 너비
        int width = static_cast<int>(charWidth * tokenLength);
        
        return cv::Rect(startX, totalBox.y, width, totalBox.height);
    }
};

JapaneseTokenizer::JapaneseTokenizer()
    : pImpl_(std::make_unique<Impl>()) {
}

JapaneseTokenizer::~JapaneseTokenizer() {
    Shutdown();
}

bool JapaneseTokenizer::Initialize(const std::string& dicPath) {
    if (pImpl_->initialized) {
        Shutdown();
    }

    // 사전 경로 자동 탐색
    std::vector<std::string> searchPaths;
    
    if (!dicPath.empty()) {
        searchPaths.push_back(dicPath);
    }
    
    // 기본 탐색 경로들
    searchPaths.push_back("C:\\Program Files\\MeCab\\dic\\ipadic");  // 시스템 설치
    searchPaths.push_back(".\\dic\\ipadic");                          // 번들 배포
    searchPaths.push_back(".\\mecab\\dic\\ipadic");
    
    // 환경 변수 확인
    const char* mecabrc = std::getenv("MECABRC");
    if (mecabrc) {
        searchPaths.push_back(mecabrc);
    }

    // MeCab Tagger 생성 시도
    // 참고: argc/argv 형식 사용 (공백이 있는 경로 지원)
    for (const auto& path : searchPaths) {
        try {
            // argv 형식으로 인자 전달
            int argc = 3;
            const char* argv[] = {"mecab", "-d", path.c_str()};
            pImpl_->tagger = MeCab::createTagger(argc, const_cast<char**>(argv));
            
            if (pImpl_->tagger) {
                pImpl_->initialized = true;
                return true;
            }
        } catch (...) {
            // 다음 경로 시도
            continue;
        }
    }

    // 모든 경로 실패
    return false;
}

std::vector<Token> JapaneseTokenizer::Tokenize(const std::string& text) {
    std::vector<Token> tokens;

    if (!pImpl_->initialized || text.empty()) {
        return tokens;
    }

    // MeCab으로 형태소 분석
    const MeCab::Node* node = pImpl_->tagger->parseToNode(text.c_str());

    if (!node) {
        return tokens;
    }

    // 각 형태소를 Token으로 변환
    for (; node; node = node->next) {
        // BOS(문장 시작), EOS(문장 끝) 노드는 스킵
        if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) {
            continue;
        }

        Token token;
        
        // 표면형 (실제 나타난 형태)
        token.surface = std::string(node->surface, node->length);

        // feature 파싱: "품사,품사세분류1,품사세분류2,품사세분류3,활용형,활용형세분류,기본형,읽기,발음"
        std::string feature = node->feature ? node->feature : "";
        
        size_t pos = 0;
        int fieldIndex = 0;
        std::string field;
        
        while ((pos = feature.find(',')) != std::string::npos) {
            field = feature.substr(0, pos);
            
            switch (fieldIndex) {
                case 0:  // 품사
                    token.partOfSpeech = field;
                    break;
                case 6:  // 기본형
                    token.baseForm = field;
                    break;
                case 7:  // 읽기
                    token.reading = field;
                    break;
            }
            
            feature.erase(0, pos + 1);
            fieldIndex++;
        }

        // 마지막 필드 처리 (발음)
        if (fieldIndex == 7 && !feature.empty()) {
            token.reading = feature;
        }

        // 읽기 정보가 없으면 표면형 사용
        if (token.reading.empty() || token.reading == "*") {
            token.reading = token.surface;
        }

        // 기본형이 없으면 표면형 사용
        if (token.baseForm.empty() || token.baseForm == "*") {
            token.baseForm = token.surface;
        }

        tokens.push_back(token);
    }

    return tokens;
}

std::vector<Token> JapaneseTokenizer::TokenizeWithPosition(const ocr::TextSegment& segment) {
    // 텍스트를 형태소 분석
    auto tokens = Tokenize(segment.text);

    // 각 토큰에 위치 정보 추가
    int currentPos = 0;
    int textLength = segment.text.length();

    for (auto& token : tokens) {
        // 토큰의 boundingBox 계산
        token.boundingBox = pImpl_->CalculateTokenBoundingBox(
            segment.boundingBox,
            textLength,
            currentPos,
            token.surface.length()
        );

        // OCR 신뢰도 상속
        token.confidence = segment.confidence;

        currentPos += token.surface.length();
    }

    return tokens;
}

std::vector<Token> JapaneseTokenizer::TokenizeBatch(const std::vector<ocr::TextSegment>& segments) {
    std::vector<Token> allTokens;

    for (const auto& segment : segments) {
        auto tokens = TokenizeWithPosition(segment);
        allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());
    }

    return allTokens;
}

bool JapaneseTokenizer::IsInitialized() const {
    return pImpl_->initialized;
}

void JapaneseTokenizer::Shutdown() {
    if (pImpl_->initialized && pImpl_->tagger) {
        delete pImpl_->tagger;
        pImpl_->tagger = nullptr;
        pImpl_->initialized = false;
    }
}

}  // namespace tokenizer
}  // namespace toriyomi
