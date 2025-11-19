// ToriYomi - OCR 엔진 팩토리 구현
// 런타임에 다른 OCR 엔진을 생성

#include "ocr_engine.h"
#include "paddle_ocr_wrapper.h"

namespace toriyomi {
namespace ocr {

std::unique_ptr<IOcrEngine> OcrEngineFactory::CreateEngine(OcrEngineType type) {
    switch (type) {
        case OcrEngineType::PaddleOCR:
            return std::make_unique<PaddleOcrWrapper>();

        case OcrEngineType::EasyOCR:
            // TODO: EasyOCR 구현 시 추가
            return nullptr;

        default:
            return nullptr;
    }
}

}  // namespace ocr
}  // namespace toriyomi
