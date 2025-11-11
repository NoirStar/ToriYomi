// ToriYomi - 후리가나 매퍼 구현

#include "furigana_mapper.h"
#include <codecvt>
#include <locale>

namespace toriyomi {
namespace tokenizer {

// Pimpl 구현
class FuriganaMapper::Impl {
public:
    int rubyOffset = 5;  // 루비 텍스트와 베이스 텍스트 간격 (픽셀)
};

FuriganaMapper::FuriganaMapper()
    : pImpl_(std::make_unique<Impl>()) {
}

FuriganaMapper::~FuriganaMapper() = default;

std::vector<FuriganaInfo> FuriganaMapper::MapTokensToFurigana(const std::vector<Token>& tokens) {
    std::vector<FuriganaInfo> furiganaList;
    furiganaList.reserve(tokens.size());

    for (const auto& token : tokens) {
        furiganaList.push_back(MapTokenToFurigana(token));
    }

    return furiganaList;
}

FuriganaInfo FuriganaMapper::MapTokenToFurigana(const Token& token) {
    FuriganaInfo info;
    
    info.baseText = token.surface;
    // MeCab은 가타카나로 읽기를 반환하므로 히라가나로 변환
    info.reading = KatakanaToHiragana(token.reading);
    info.position = token.boundingBox;
    info.needsRuby = ContainsKanji(token.surface);
    
    if (info.needsRuby) {
        info.rubyPosition = CalculateRubyPosition(token.boundingBox, pImpl_->rubyOffset);
    } else {
        // 후리가나가 필요 없으면 루비 위치는 사용하지 않음
        info.rubyPosition = cv::Point(0, 0);
    }

    return info;
}

bool FuriganaMapper::ContainsKanji(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    // UTF-8을 UTF-32로 변환하여 유니코드 코드포인트 검사
    try {
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
        std::u32string u32str = converter.from_bytes(text);

        for (char32_t ch : u32str) {
            // 한자 유니코드 범위:
            // CJK Unified Ideographs: U+4E00 ~ U+9FFF
            // CJK Compatibility Ideographs: U+F900 ~ U+FAFF
            // CJK Unified Ideographs Extension A: U+3400 ~ U+4DBF
            if ((ch >= 0x4E00 && ch <= 0x9FFF) ||
                (ch >= 0x3400 && ch <= 0x4DBF) ||
                (ch >= 0xF900 && ch <= 0xFAFF)) {
                return true;
            }
        }
    } catch (...) {
        // 변환 실패 시 false 반환
        return false;
    }

    return false;
}

std::string FuriganaMapper::KatakanaToHiragana(const std::string& katakana) {
    if (katakana.empty()) {
        return "";
    }

    try {
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
        std::u32string u32str = converter.from_bytes(katakana);
        
        for (char32_t& ch : u32str) {
            // 가타카나 유니코드 범위: U+30A0 ~ U+30FF
            // 히라가나 유니코드 범위: U+3040 ~ U+309F
            // 오프셋: 0x60 (96)
            if (ch >= 0x30A0 && ch <= 0x30FF) {
                ch -= 0x60;  // 가타카나를 히라가나로 변환
            }
        }
        
        return converter.to_bytes(u32str);
    } catch (...) {
        // 변환 실패 시 원본 반환
        return katakana;
    }
}

cv::Point FuriganaMapper::CalculateRubyPosition(const cv::Rect& basePosition, int rubyOffset) {
    // 루비 텍스트는 베이스 텍스트 위쪽에 표시
    // x: 베이스 텍스트와 동일 (좌측 정렬)
    // y: 베이스 텍스트 y - rubyOffset
    return cv::Point(basePosition.x, basePosition.y - rubyOffset);
}

void FuriganaMapper::SetRubyOffset(int offset) {
    pImpl_->rubyOffset = offset;
}

int FuriganaMapper::GetRubyOffset() const {
    return pImpl_->rubyOffset;
}

} // namespace tokenizer
} // namespace toriyomi
