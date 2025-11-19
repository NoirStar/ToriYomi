# OCR 엔진 교체 가능 설계

## 📋 개요

ToriYomi는 OCR 엔진을 쉽게 교체할 수 있도록 추상 인터페이스를 제공합니다.

## 🏗️ 아키텍처

```
IOcrEngine (추상 인터페이스)
    ├── PaddleOcrWrapper (Paddle cpp_infer, 기본값)
    └── FutureOcrWrapper (확장 슬롯)
```

## 📝 사용 방법

### 방법 1: 직접 생성
```cpp
#include "core/ocr/paddle_ocr_wrapper.h"

auto ocr = std::make_unique<PaddleOcrWrapper>();
ocr->Initialize("C:/path/to/paddle/models", "jpn");

cv::Mat frame = /* 캡처된 프레임 */;
auto results = ocr->RecognizeText(frame);

for (const auto& segment : results) {
    std::cout << "Text: " << segment.text 
              << " (confidence: " << segment.confidence << "%)" 
              << std::endl;
}

ocr->Shutdown();
```

### 방법 2: 팩토리 패턴 (권장)
```cpp
#include "core/ocr/ocr_engine.h"

// 런타임에 엔진 선택
auto ocr = OcrEngineFactory::CreateEngine(OcrEngineType::PaddleOCR);

if (ocr) {
    ocr->Initialize("C:/path/to/paddle/models", "jpn");
    
    cv::Mat frame = /* 캡처된 프레임 */;
    auto results = ocr->RecognizeText(frame);
    
    std::cout << "Using: " << ocr->GetEngineName() << std::endl;
    
    ocr->Shutdown();
}
```

### 방법 3: 다형성 활용
```cpp
#include "core/ocr/ocr_engine.h"

class OcrProcessor {
    std::unique_ptr<IOcrEngine> engine_;
    
public:
    OcrProcessor(std::unique_ptr<IOcrEngine> engine) 
        : engine_(std::move(engine)) {}
    
    void ProcessFrame(const cv::Mat& frame) {
        if (!engine_->IsInitialized()) {
            return;
        }
        
        auto results = engine_->RecognizeText(frame);
        // 결과 처리...
    }
};

// 사용
auto ocr = OcrEngineFactory::CreateEngine(OcrEngineType::PaddleOCR);
OcrProcessor processor(std::move(ocr));
```

## 🔄 새 엔진 추가 방법

### 1. 새 클래스 작성
```cpp
// src/core/ocr/paddle_ocr_wrapper.h
#pragma once
#include "ocr_engine.h"

class PaddleOcrWrapper : public IOcrEngine {
public:
    PaddleOcrWrapper();
    ~PaddleOcrWrapper();
    
    bool Initialize(const std::string& modelPath, 
                   const std::string& language) override;
    std::vector<TextSegment> RecognizeText(const cv::Mat& image) override;
    void Shutdown() override;
    bool IsInitialized() const override;
    std::string GetEngineName() const override;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
```

### 2. 팩토리에 등록
```cpp
// src/core/ocr/ocr_engine.cpp
#include "paddle_ocr_wrapper.h"

std::unique_ptr<IOcrEngine> OcrEngineFactory::CreateEngine(OcrEngineType type) {
    switch (type) {
        case OcrEngineType::PaddleOCR:
            return std::make_unique<PaddleOcrWrapper>();

        case OcrEngineType::FutureExperimental:
            return std::make_unique<FutureOcrWrapper>();

        default:
            return nullptr;
    }
}
```

### 3. CMakeLists.txt 업데이트
```cmake
add_library(toriyomi_ocr
    src/core/ocr/ocr_engine.cpp
    src/core/ocr/paddle_ocr_wrapper.cpp
    src/core/ocr/ocr_engine_bootstrapper.cpp
)

target_link_libraries(toriyomi_ocr
    ${OpenCV_LIBS}
    toriyomi_paddleocr
)
```

## 🎯 현재 상태

### ✅ 구현됨:
- `IOcrEngine` 추상 인터페이스
- `PaddleOcrWrapper` (Paddle cpp_infer 파이프라인, **유일한 엔진**)
- `OcrEngineFactory` & `OcrEngineBootstrapper` (Paddle 전용 초기화 및 오류 보고)
- 단위 테스트 (11개+)
- CMake 자동 DLL 배포 시스템
- UI 기본 설정: PaddleOCR 기본값

### 🚧 향후 계획:
1. **실제 게임 화면 테스트**
    - PaddleOCR 인식률 및 속도 측정
    - 프레임 스킵, ROI 기반 최적화

2. **PaddleOCR 최적화**
     - 모델 전처리 & 배포 자동화
     - GPU/ONNX Runtime 경로 검토

3. **전처리 파이프라인**
    - CLAHE, bilateral filter 등 선택적 필터링
    - 텍스트 영역 사전 감지 연구

## 💡 설계 장점

1. **교체 용이**: 엔진 변경 시 다른 코드 수정 불필요
2. **테스트 가능**: Mock 엔진으로 단위 테스트 용이
3. **확장 가능**: 새 엔진 추가가 간단
4. **의존성 분리**: Pimpl 패턴으로 헤더 의존성 최소화
5. **성능 비교**: 여러 엔진 동시 테스트 가능

## 🔍 다음 단계

```bash
# 1. Paddle Inference SDK / 모델 다운로드
#    - SDK: https://www.paddlepaddle.org.cn/inference/download (Windows CPU)
#    - 모델: models/paddleocr/{det,rec,ppocr_keys_v1.txt}

# 2. CMake 구성 시 필수 옵션 전달
cmake .. -DTORIYOMI_PADDLE_DIR="C:/Dev/paddle_inference" \
         -DTORIYOMI_PADDLE_RUNTIME_DIR="C:/Dev/paddle_inference/paddle/lib"

# 3. 빌드 및 테스트
cmake --build . --config Release
ctest -C Release --output-on-failure

# 4. 실제 게임 화면으로 성능 측정 및 파라미터 튜닝
```
