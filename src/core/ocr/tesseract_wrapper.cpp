// ToriYomi - Tesseract OCR 래퍼 구현
// 일본어 텍스트 인식을 위한 Tesseract API 래핑

#include "tesseract_wrapper.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace toriyomi {
namespace ocr {

// Pimpl 구현 클래스
class TesseractWrapper::Impl {
public:
    tesseract::TessBaseAPI api;  // Tesseract API 인스턴스
    bool initialized = false;    // 초기화 상태
    float confidenceThreshold = 50.0f;  // 최소 신뢰도 임계값

    /**
     * @brief OpenCV Mat을 Leptonica Pix로 변환
     * 
     * @param image OpenCV 이미지 (BGR 형식)
     * @return Leptonica Pix 포인터
     */
    Pix* MatToPix(const cv::Mat& image) {
        // BGR을 RGB로 변환 (Tesseract는 RGB 요구)
        cv::Mat rgb;
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);

        // Pix 생성 (24-bit RGB)
        Pix* pix = pixCreate(rgb.cols, rgb.rows, 24);
        if (!pix) {
            return nullptr;
        }

        // 픽셀 데이터 복사
        for (int y = 0; y < rgb.rows; ++y) {
            for (int x = 0; x < rgb.cols; ++x) {
                const cv::Vec3b& pixel = rgb.at<cv::Vec3b>(y, x);
                pixSetRGBPixel(pix, x, y, pixel[0], pixel[1], pixel[2]);
            }
        }

        return pix;
    }
};

TesseractWrapper::TesseractWrapper()
    : pImpl_(std::make_unique<Impl>()) {
}

TesseractWrapper::~TesseractWrapper() {
    Shutdown();
}

bool TesseractWrapper::Initialize(const std::string& tessdataPath, const std::string& language) {
    if (pImpl_->initialized) {
        Shutdown();
    }

    // Tesseract 초기화 (OEM_LSTM_ONLY: 최신 LSTM 신경망 모드)
    if (pImpl_->api.Init(tessdataPath.c_str(), language.c_str(), tesseract::OEM_LSTM_ONLY) != 0) {
        return false;
    }

    // 페이지 세그먼테이션 모드 설정
    // PSM_AUTO: 자동으로 최적의 세그먼테이션 선택
    pImpl_->api.SetPageSegMode(tesseract::PSM_AUTO);

    pImpl_->initialized = true;
    return true;
}

std::vector<TextSegment> TesseractWrapper::RecognizeText(const cv::Mat& image) {
    std::vector<TextSegment> results;

    if (!pImpl_->initialized) {
        return results;
    }

    if (image.empty() || image.type() != CV_8UC3) {
        return results;
    }

    // OpenCV Mat을 Leptonica Pix로 변환
    Pix* pix = pImpl_->MatToPix(image);
    if (!pix) {
        return results;
    }

    // Tesseract에 이미지 설정
    pImpl_->api.SetImage(pix);

    // OCR 수행
    pImpl_->api.Recognize(nullptr);

    // 결과 추출 (단어 단위)
    tesseract::ResultIterator* iter = pImpl_->api.GetIterator();
    tesseract::PageIteratorLevel level = tesseract::RIL_WORD;

    if (iter != nullptr) {
        do {
            // 텍스트 추출
            const char* word = iter->GetUTF8Text(level);
            if (!word) {
                continue;
            }

            // 신뢰도 추출
            float confidence = iter->Confidence(level);

            // 신뢰도가 임계값 이상인 경우만 포함
            if (confidence >= pImpl_->confidenceThreshold) {
                // 바운딩 박스 추출
                int x1, y1, x2, y2;
                iter->BoundingBox(level, &x1, &y1, &x2, &y2);

                TextSegment segment;
                segment.text = word;
                segment.boundingBox = cv::Rect(x1, y1, x2 - x1, y2 - y1);
                segment.confidence = confidence;

                results.push_back(segment);
            }

            delete[] word;

        } while (iter->Next(level));

        delete iter;
    }

    // Pix 메모리 해제
    pixDestroy(&pix);

    return results;
}

void TesseractWrapper::Shutdown() {
    if (pImpl_->initialized) {
        pImpl_->api.End();
        pImpl_->initialized = false;
    }
}

bool TesseractWrapper::IsInitialized() const {
    return pImpl_->initialized;
}

}  // namespace ocr
}  // namespace toriyomi
