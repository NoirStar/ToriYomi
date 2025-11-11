#pragma once

#include "ocr_engine.h"
#include <memory>

namespace toriyomi {
namespace ocr {

/**
 * @brief Tesseract OCR 엔진 구현체
 * 
 * IOcrEngine 인터페이스를 구현한 Tesseract 래퍼.
 * 일본어 텍스트 인식을 위한 Tesseract API 래핑.
 * Pimpl 패턴으로 Tesseract 의존성을 숨김.
 */
class TesseractWrapper : public IOcrEngine {
public:
    /**
     * @brief TesseractWrapper 생성
     */
    TesseractWrapper();

    /**
     * @brief TesseractWrapper 소멸
     */
    ~TesseractWrapper();

    // 복사 방지
    TesseractWrapper(const TesseractWrapper&) = delete;
    TesseractWrapper& operator=(const TesseractWrapper&) = delete;

    // IOcrEngine 인터페이스 구현
    bool Initialize(const std::string& tessdataPath, const std::string& language = "jpn") override;
    std::vector<TextSegment> RecognizeText(const cv::Mat& image) override;
    void Shutdown() override;
    bool IsInitialized() const override;
    std::string GetEngineName() const override;

private:
    class Impl;  // Pimpl 패턴
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace ocr
}  // namespace toriyomi
