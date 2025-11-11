#pragma once

#include "core/ocr/ocr_engine.h"
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <memory>

namespace toriyomi {
namespace tokenizer {

/**
 * @brief 일본어 토큰 (형태소)
 */
struct Token {
    std::string surface;        // 표면형 (今日)
    std::string reading;        // 읽기 (キョウ 또는 きょう)
    std::string baseForm;       // 기본형
    std::string partOfSpeech;   // 품사 (名詞, 動詞 등)
    cv::Rect boundingBox;       // 화면 상의 위치 (OCR 결과에서 계산)
    float confidence;           // 신뢰도 (OCR 결과 기반)
};

/**
 * @brief 일본어 형태소 분석기
 * 
 * MeCab을 사용하여 일본어 텍스트를 형태소 단위로 분석.
 * OCR 결과와 결합하여 각 단어의 위치와 읽기 정보를 제공.
 */
class JapaneseTokenizer {
public:
    /**
     * @brief JapaneseTokenizer 생성
     */
    JapaneseTokenizer();

    /**
     * @brief JapaneseTokenizer 소멸
     */
    ~JapaneseTokenizer();

    // 복사 방지
    JapaneseTokenizer(const JapaneseTokenizer&) = delete;
    JapaneseTokenizer& operator=(const JapaneseTokenizer&) = delete;

    /**
     * @brief MeCab 초기화
     * 
     * @param dicPath MeCab 사전 경로 (비어있으면 기본 경로 사용)
     * @return 초기화 성공 여부
     */
    bool Initialize(const std::string& dicPath = "");

    /**
     * @brief 일본어 텍스트를 토큰으로 분리
     * 
     * @param text 입력 텍스트 (UTF-8)
     * @return 토큰 목록
     */
    std::vector<Token> Tokenize(const std::string& text);

    /**
     * @brief OCR 결과를 토큰으로 분리 (위치 정보 포함)
     * 
     * TextSegment의 텍스트를 형태소 분석하고,
     * boundingBox를 기준으로 각 토큰의 대략적인 위치를 계산.
     * 
     * @param segment OCR 인식 결과
     * @return 위치 정보가 포함된 토큰 목록
     */
    std::vector<Token> TokenizeWithPosition(const ocr::TextSegment& segment);

    /**
     * @brief 여러 OCR 결과를 일괄 처리
     * 
     * @param segments OCR 인식 결과 목록
     * @return 모든 토큰의 결합된 목록
     */
    std::vector<Token> TokenizeBatch(const std::vector<ocr::TextSegment>& segments);

    /**
     * @brief 초기화 상태 확인
     * 
     * @return 초기화 완료 여부
     */
    bool IsInitialized() const;

    /**
     * @brief MeCab 종료
     */
    void Shutdown();

private:
    class Impl;  // Pimpl 패턴
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace tokenizer
}  // namespace toriyomi
