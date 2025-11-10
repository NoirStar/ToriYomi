#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <memory>

namespace toriyomi {
namespace ocr {

/**
 * @brief 화면에서 인식된 텍스트의 위치와 내용
 */
struct TextSegment {
    std::string text;        // 인식된 텍스트 (UTF-8)
    cv::Rect boundingBox;    // 텍스트 영역 좌표 (x, y, width, height)
    float confidence;        // 인식 신뢰도 (0.0 ~ 100.0)
};

/**
 * @brief Tesseract OCR 엔진 래퍼
 * 
 * 일본어 텍스트 인식을 위한 Tesseract API 래퍼 클래스.
 * Pimpl 패턴으로 Tesseract 의존성을 숨김.
 */
class TesseractWrapper {
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

    /**
     * @brief Tesseract 엔진 초기화
     * 
     * @param tessdataPath tessdata 디렉토리 경로 (일본어 학습 데이터 위치)
     * @param language 사용할 언어 (기본값: "jpn" - 일본어)
     * @return 초기화 성공 여부
     * 
     * @note tessdataPath는 jpn.traineddata 파일이 있는 디렉토리여야 함
     */
    bool Initialize(const std::string& tessdataPath, const std::string& language = "jpn");

    /**
     * @brief 이미지에서 텍스트 인식
     * 
     * @param image 입력 이미지 (BGR 형식, CV_8UC3)
     * @return 인식된 텍스트 세그먼트 목록
     * 
     * @note 신뢰도가 낮은 결과는 자동으로 필터링됨 (threshold: 50.0)
     */
    std::vector<TextSegment> RecognizeText(const cv::Mat& image);

    /**
     * @brief Tesseract 엔진 종료 및 리소스 해제
     */
    void Shutdown();

    /**
     * @brief 초기화 상태 확인
     * 
     * @return 초기화 완료 여부
     */
    bool IsInitialized() const;

private:
    class Impl;  // Pimpl 패턴
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace ocr
}  // namespace toriyomi
