// ToriYomi - 후리가나 매퍼
// 토큰의 읽기 정보를 원본 텍스트에 매핑하여 후리가나 렌더링 준비

#pragma once

#include "japanese_tokenizer.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace toriyomi {
namespace tokenizer {

/**
 * @brief 후리가나 정보
 * 
 * 한자 위에 표시될 읽기 정보
 */
struct FuriganaInfo {
    std::string baseText;      // 원본 텍스트 (예: "今日")
    std::string reading;       // 읽기 (예: "キョウ")
    cv::Rect position;         // 화면 위치 (베이스 텍스트)
    cv::Point rubyPosition;    // 루비 텍스트 위치 (읽기 표시 위치)
    bool needsRuby;            // 후리가나 필요 여부 (한자 포함 시 true)
};

/**
 * @brief 후리가나 매퍼 클래스
 * 
 * 토큰의 읽기 정보를 후리가나로 변환하고 렌더링 위치를 계산
 */
class FuriganaMapper {
public:
    FuriganaMapper();
    ~FuriganaMapper();

    /**
     * @brief 토큰 리스트를 후리가나 정보로 변환
     * 
     * @param tokens 토큰 리스트
     * @return 후리가나 정보 리스트
     */
    std::vector<FuriganaInfo> MapTokensToFurigana(const std::vector<Token>& tokens);

    /**
     * @brief 단일 토큰을 후리가나 정보로 변환
     * 
     * @param token 토큰
     * @return 후리가나 정보
     */
    FuriganaInfo MapTokenToFurigana(const Token& token);

    /**
     * @brief 텍스트에 한자가 포함되어 있는지 확인
     * 
     * @param text UTF-8 문자열
     * @return 한자 포함 여부
     */
    static bool ContainsKanji(const std::string& text);

    /**
     * @brief 가타카나를 히라가나로 변환
     * 
     * @param katakana 가타카나 문자열
     * @return 히라가나 문자열
     */
    static std::string KatakanaToHiragana(const std::string& katakana);

    /**
     * @brief 루비 텍스트 위치 계산
     * 
     * @param basePosition 베이스 텍스트 위치
     * @param rubyOffset 루비 텍스트 오프셋 (기본값: 베이스 텍스트 위 5px)
     * @return 루비 텍스트 위치
     */
    static cv::Point CalculateRubyPosition(const cv::Rect& basePosition, int rubyOffset = 5);

    /**
     * @brief 후리가나 표시 간격 설정
     * 
     * @param offset 루비 텍스트 오프셋 (픽셀)
     */
    void SetRubyOffset(int offset);

    /**
     * @brief 현재 루비 오프셋 가져오기
     * 
     * @return 루비 오프셋 (픽셀)
     */
    int GetRubyOffset() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace tokenizer
} // namespace toriyomi
