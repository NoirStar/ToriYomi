// ToriYomi - Tesseract OCR 래퍼 구현
// 일본어 텍스트 인식을 위한 Tesseract API 래핑

#include "tesseract_wrapper.h"
#include <tesseract/baseapi.h>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace toriyomi {
namespace ocr {

// Pimpl 구현 클래스
class TesseractWrapper::Impl {
public:
    tesseract::TessBaseAPI api;  // Tesseract API 인스턴스
    bool initialized = false;    // 초기화 상태
    float confidenceThreshold = 50.0f;  // 최소 신뢰도 임계값
    std::mutex apiMutex;  // Tesseract API 호출 동기화 (thread-safety 보장)
};

TesseractWrapper::TesseractWrapper()
    : pImpl_(std::make_unique<Impl>()) {
}

TesseractWrapper::~TesseractWrapper() {
    Shutdown();
}

bool TesseractWrapper::Initialize(const std::string& tessdataPath, const std::string& language) {
    std::lock_guard<std::mutex> lock(pImpl_->apiMutex);
    
    if (pImpl_->initialized) {
        pImpl_->api.End();
        pImpl_->initialized = false;
    }

    // Tesseract 초기화
    if (pImpl_->api.Init(tessdataPath.c_str(), language.c_str(), tesseract::OEM_LSTM_ONLY) != 0) {
        return false;
    }

    // 페이지 세그먼테이션 모드 설정
    pImpl_->api.SetPageSegMode(tesseract::PSM_AUTO);
    
    // DPI 명시 지정 (내부 Leptonica 호출 회피)
    pImpl_->api.SetVariable("user_defined_dpi", "300");
    pImpl_->api.SetVariable("classify_bln_numeric_mode", "1");

    pImpl_->initialized = true;
    return true;
}

std::vector<TextSegment> TesseractWrapper::RecognizeText(const cv::Mat& image) {
    std::vector<TextSegment> results;

    if (!pImpl_->initialized) {
        fprintf(stderr, "[TessWrap] RecognizeText aborted: engine is not initialized.\n");
        fflush(stderr);
        return results;
    }

    // 이미지 유효성 검사
    if (image.empty() || image.type() != CV_8UC3) {
        return results;
    }

    // 크기 제한 검사
    if (image.cols < 10 || image.rows < 10 || image.cols > 4096 || image.rows > 4096) {
        return results;
    }

    const double targetPixels = 800.0 * 600.0;
    const double pixelCount = static_cast<double>(image.cols) * image.rows;
    double scale = 1.0;
    if (pixelCount > targetPixels) {
        scale = std::sqrt(targetPixels / pixelCount);
    }

    cv::Mat scaled;
    if (scale < 1.0) {
        cv::resize(image, scaled, cv::Size(), scale, scale, cv::INTER_AREA);
    }

    const cv::Mat& working = (scale < 1.0) ? scaled : image;
    
    // Tesseract API는 thread-safe하지 않으므로 mutex로 보호
    std::lock_guard<std::mutex> lock(pImpl_->apiMutex);

    // OpenCV 스레드 충돌 방지
    cv::setNumThreads(1);

    try {
        // BGR을 RGB로 변환 (Tesseract는 RGB를 기대)
        cv::Mat rgb;
        cv::cvtColor(working, rgb, cv::COLOR_BGR2RGB);
        
        // 연속 메모리 보장
        if (!rgb.isContinuous()) {
            rgb = rgb.clone();
        }

        // Tesseract에 Mat 버퍼 직접 전달 (Leptonica Pix 경유 X)
        pImpl_->api.SetImage(
            rgb.data,                              // 이미지 데이터
            rgb.cols,                              // 너비
            rgb.rows,                              // 높이
            3,                                     // bytes_per_pixel (RGB)
            static_cast<int>(rgb.step[0])         // bytes_per_line
        );

        // OCR 수행
        int recognizeResult = pImpl_->api.Recognize(nullptr);
        if (recognizeResult != 0) {
            return results;
        }

        // 결과 추출 (RIL_WORD 레벨)
        std::unique_ptr<tesseract::ResultIterator> iter(pImpl_->api.GetIterator());
        if (!iter) {
            return results;
        }

        tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
        int wordCount = 0;
        const int MAX_ITERATIONS = 10000;
        
        do {
            // Shutdown 호출 시 조기 종료
            if (!pImpl_->initialized) {
                break;
            }
            
            wordCount++;
            if (wordCount > MAX_ITERATIONS) {
                break;
            }
            
            // GetUTF8Text 사용 후 즉시 std::string으로 복사
            std::unique_ptr<char[], std::default_delete<char[]>> word(iter->GetUTF8Text(level));
            if (!word) {
                continue;
            }
            
            std::string text(word.get());  // 즉시 복사
            
            // 신뢰도 체크
            float confidence = iter->Confidence(level);
            if (confidence < pImpl_->confidenceThreshold) {
                continue;
            }
            
            // 바운딩 박스 추출
            int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            if (iter->BoundingBox(level, &x1, &y1, &x2, &y2) && x2 > x1 && y2 > y1) {
                TextSegment segment;
                segment.text = text;

                const double invScale = (scale < 1.0 && scale > 0.0) ? (1.0 / scale) : 1.0;
                const int width = static_cast<int>((x2 - x1) * invScale);
                const int height = static_cast<int>((y2 - y1) * invScale);
                const int originX = static_cast<int>(x1 * invScale);
                const int originY = static_cast<int>(y1 * invScale);
                segment.boundingBox = cv::Rect(originX, originY, width, height);
                segment.confidence = confidence;
                results.push_back(segment);
            }
            
        } while (iter->Next(level));
        
    } catch (const std::exception& e) {
        fprintf(stderr, "[TessWrap] Exception: %s\n", e.what());
        fflush(stderr);
        results.clear();
    } catch (...) {
        fprintf(stderr, "[TessWrap] Unknown exception\n");
        fflush(stderr);
        results.clear();
    }
    
    return results;
}
// 여기서 lock_guard 소멸 → mutex 해제

void TesseractWrapper::Shutdown() {
    // 먼저 플래그 설정 (RecognizeText에서 조기 종료하도록)
    pImpl_->initialized = false;

    fprintf(stderr, "[TessWrap] Shutdown requested, waiting for active OCR calls...\n");
    fflush(stderr);

    std::lock_guard<std::mutex> lock(pImpl_->apiMutex);
    
    try {
        pImpl_->api.ClearAdaptiveClassifier();
    } catch (...) {
        // ignore
    }
    
    try {
        pImpl_->api.End();
    } catch (...) {
        // ignore - 이미 종료된 경우 예외 무시
    }
    
    fprintf(stderr, "[TessWrap] Shutdown complete\n");
    fflush(stderr);
}

bool TesseractWrapper::IsInitialized() const {
    return pImpl_->initialized;
}

std::string TesseractWrapper::GetEngineName() const {
    return "Tesseract";
}

}  // namespace ocr
}  // namespace toriyomi
