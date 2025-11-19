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
 * @brief OCR 엔진 추상 인터페이스
 * 
 * 다양한 OCR 엔진(PaddleOCR, EasyOCR 등)을
 * 교체 가능하도록 하는 공통 인터페이스.
 */
class IOcrEngine {
public:
    virtual ~IOcrEngine() = default;

    /**
     * @brief OCR 엔진 초기화
     * 
     * @param configPath 설정 파일 또는 모델 경로
     * @param language 인식 언어 (예: "jpn", "ja" 등)
     * @return 초기화 성공 여부
     */
    virtual bool Initialize(const std::string& configPath, const std::string& language = "jpn") = 0;

    /**
     * @brief 이미지에서 텍스트 인식
     * 
     * @param image 입력 이미지 (BGR 형식, CV_8UC3)
     * @return 인식된 텍스트 세그먼트 목록
     */
    virtual std::vector<TextSegment> RecognizeText(const cv::Mat& image) = 0;

    /**
     * @brief OCR 엔진 종료 및 리소스 해제
     */
    virtual void Shutdown() = 0;

    /**
     * @brief 초기화 상태 확인
     * 
     * @return 초기화 완료 여부
     */
    virtual bool IsInitialized() const = 0;

    /**
     * @brief OCR 엔진 이름 반환
     * 
    * @return 엔진 이름 (예: "PaddleOCR")
     */
    virtual std::string GetEngineName() const = 0;
};

/**
 * @brief OCR 엔진 타입 열거형
 */
enum class OcrEngineType {
    PaddleOCR,    // PaddleOCR (기본 엔진)
    EasyOCR       // EasyOCR (미래 구현)
};

/**
 * @brief OCR 엔진 팩토리
 * 
 * 런타임에 다른 OCR 엔진을 생성할 수 있는 팩토리 클래스
 */
class OcrEngineFactory {
public:
    /**
     * @brief 지정된 타입의 OCR 엔진 생성
     * 
     * @param type OCR 엔진 타입
     * @return OCR 엔진 인스턴스 (생성 실패 시 nullptr)
     */
    static std::unique_ptr<IOcrEngine> CreateEngine(OcrEngineType type);
};

}  // namespace ocr
}  // namespace toriyomi
