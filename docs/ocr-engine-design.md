# OCR 엔진 교체 가능 설계

## 📋 개요

ToriYomi는 OCR 엔진을 쉽게 교체할 수 있도록 추상 인터페이스를 제공합니다.

## 🏗️ 아키텍처

```
IOcrEngine (추상 인터페이스)
    ├── TesseractWrapper (현재 구현)
    ├── PaddleOcrWrapper (미래 구현)
    └── EasyOcrWrapper (미래 구현)
```

## 📝 사용 방법

### 방법 1: 직접 생성
```cpp
#include "core/ocr/tesseract_wrapper.h"

auto ocr = std::make_unique<TesseractWrapper>();
ocr->Initialize("C:/path/to/tessdata", "jpn");

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
auto ocr = OcrEngineFactory::CreateEngine(OcrEngineType::Tesseract);

if (ocr) {
    ocr->Initialize("C:/path/to/tessdata", "jpn");
    
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
auto ocr = OcrEngineFactory::CreateEngine(OcrEngineType::Tesseract);
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
        case OcrEngineType::Tesseract:
            return std::make_unique<TesseractWrapper>();
        
        case OcrEngineType::PaddleOCR:
            return std::make_unique<PaddleOcrWrapper>();  // 추가!
        
        // ...
    }
}
```

### 3. CMakeLists.txt 업데이트
```cmake
add_library(toriyomi_ocr
    src/core/ocr/ocr_engine.cpp
    src/core/ocr/tesseract_wrapper.cpp
    src/core/ocr/paddle_ocr_wrapper.cpp  # 추가!
)

target_link_libraries(toriyomi_ocr
    ${OpenCV_LIBS}
    ${Tesseract_LIBRARIES}
    ${PaddleOCR_LIBRARIES}  # 추가!
)
```

## 🎯 현재 상태

### ✅ 구현됨:
- `IOcrEngine` 추상 인터페이스
- `TesseractWrapper` 구현
- `OcrEngineFactory` 팩토리 패턴
- 단위 테스트 (10개)

### 🚧 향후 계획:
1. **실제 게임 화면 테스트**
   - Tesseract 성능 측정
   - 인식률, 속도 평가

2. **필요시 PaddleOCR 추가**
   - C++ API 또는 Python 바인딩
   - 성능 비교 테스트

3. **전처리 파이프라인**
   - 이진화, 노이즈 제거
   - 텍스트 영역 사전 감지

## 💡 설계 장점

1. **교체 용이**: 엔진 변경 시 다른 코드 수정 불필요
2. **테스트 가능**: Mock 엔진으로 단위 테스트 용이
3. **확장 가능**: 새 엔진 추가가 간단
4. **의존성 분리**: Pimpl 패턴으로 헤더 의존성 최소화
5. **성능 비교**: 여러 엔진 동시 테스트 가능

## 🔍 다음 단계

```bash
# 1. jpn.traineddata 다운로드
# https://github.com/tesseract-ocr/tessdata

# 2. 빌드
cd build
cmake ..
cmake --build . --config Release

# 3. 테스트 실행
./bin/tests/Release/test_tesseract_wrapper.exe

# 4. 실제 게임 화면으로 성능 측정
# 필요시 PaddleOCR로 전환 결정
```
