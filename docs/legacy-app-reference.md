# ToriYomi Qt Widgets App - 기능 레퍼런스 (QML 전환용)

> **작성일**: 2025-11-12  
> **목적**: 기존 Qt Widgets 기반 UI를 QML로 전환 시 참고용 문서

---

## 📁 파일 구조

```
src/ui/app/
├── main.cpp                        # 진입점 (QApplication)
├── app.ui                          # Qt Designer UI 파일
├── main_app.h/cpp                  # 메인 애플리케이션 클래스
├── interactive_sentence_widget.h/cpp  # 문장 리스트 위젯
├── roi_selector_dialog.h/cpp      # ROI 선택 다이얼로그
├── roi_overlay_widget.h/cpp       # ROI 오버레이 표시
└── draggable_image_label.h/cpp    # 드래그 가능한 이미지 레이블
```

---

## 🎯 MainApp 클래스 (main_app.h/cpp)

### **핵심 기능**

#### 1. **프로세스/윈도우 선택**
```cpp
void LoadProcessList()              // 실행 중인 윈도우 목록 로드
void OnProcessSelected(int index)   // 프로세스 선택 핸들러
```
- Windows API `EnumWindows()`로 보이는 윈도우 열거
- 각 윈도우의 제목 + 프로세스 이름 표시
- `processWindows_` (std::vector<HWND>)에 저장
- 콤보박스에 "Title (ProcessName.exe)" 형식으로 표시

#### 2. **ROI (Region of Interest) 선택**
```cpp
void OnSelectRoiClicked()           // ROI 선택 버튼 핸들러
cv::Mat CaptureSelectedProcess()    // 선택된 프로세스 화면 캡처
```
- 선택된 윈도우를 `PrintWindow()` API로 캡처
- `RoiSelectorDialog` 다이얼로그 열어서 사용자가 영역 드래그
- 선택 완료 시 `selectedRoi_` (cv::Rect)에 저장
- `hasRoiSelection_` 플래그로 선택 여부 관리

#### 3. **캡처 시작/정지**
```cpp
void OnStartCaptureClicked()        // 캡처 시작
void StartThreads()                 // 비동기 스레드 초기화
void OnStopCaptureClicked()         // 캡처 정지
void StopThreads()                  // 스레드 정리
```

**시작 프로세스**:
1. 프로세스 + ROI 선택 확인
2. UI 버튼 비활성화 (초기화 중 표시)
3. `QTimer::singleShot(0)`로 비동기 스레드 시작
4. `TesseractWrapper` OCR 엔진 초기화 ("jpn" 언어)
5. `CaptureThread` 시작 (선택된 윈도우 캡처)
6. `OcrThread` 시작 (프레임 큐에서 OCR 처리)
7. 폴링 타이머 시작 (100ms 간격)

**정지 프로세스**:
- 폴링 타이머 정지
- CaptureThread, OcrThread 순서대로 Stop() 호출
- 리소스 정리 (unique_ptr reset)

#### 4. **OCR 결과 폴링**
```cpp
void OnPollOcrResults()             // 100ms마다 호출
```
- `ocrThread_->GetLatestResults()` 호출
- 여러 텍스트 세그먼트를 공백으로 결합
- `JapaneseTokenizer`로 형태소 분석
- `AddSentence()` 호출하여 UI에 표시

#### 5. **문장 관리**
```cpp
void AddSentence(tokens, originalText)  // 문장 추가
void ClearSentences()                   // 문장 목록 초기화
```
- `sentences_` (std::vector<std::string>)에 원본 텍스트 저장
- `InteractiveSentenceWidget`에 토큰 정보와 함께 추가
- 문장 번호 자동 증가

#### 6. **단어 클릭 핸들링**
```cpp
void OnWordClicked(surface, reading, baseForm)
void ShowDictionaryEntry(word, reading, meaning)
```
- `InteractiveSentenceWidget`에서 발생한 WordClicked 시그널 수신
- HTML 형식으로 사전 패널에 표시
- 색상: 제목 `#14a085`, 읽기 `#0d7377`

#### 7. **Anki 연동 (TODO)**
```cpp
void OnAnkiButtonClicked()
```
- 현재는 메시지박스로 마지막 문장만 표시
- AnkiConnect API 통합 예정

#### 8. **상태 관리**
```cpp
void UpdateStatus(message)          // 상태바 메시지
void UpdateFps(fps)                 // FPS 표시
void LogDebug(message)              // 디버그 로그 (타임스탬프 자동)
```

### **주요 멤버 변수**

```cpp
// UI
Ui::MainWindow* ui_;                        // AUTOUIC 생성 UI
InteractiveSentenceWidget* sentenceWidget_;  // 문장 위젯

// 스레드
std::shared_ptr<FrameQueue> frameQueue_;               // 공유 프레임 큐
std::unique_ptr<CaptureThread> captureThread_;         // 캡처 스레드
std::unique_ptr<OcrThread> ocrThread_;                 // OCR 스레드
std::unique_ptr<TesseractWrapper> ocrEngine_;          // OCR 엔진
std::unique_ptr<JapaneseTokenizer> tokenizer_;         // 형태소 분석기
QTimer* pollTimer_;                                    // 폴링 타이머

// 상태
bool isCapturing_;                          // 캡처 중 여부

// 데이터
std::vector<std::string> sentences_;        // 문장 목록
std::vector<HWND> processWindows_;          // 프로세스 윈도우 목록
HWND selectedWindow_;                       // 선택된 윈도우
cv::Rect selectedRoi_;                      // 선택된 ROI
bool hasRoiSelection_;                      // ROI 선택 여부
```

---

## 📝 InteractiveSentenceWidget (interactive_sentence_widget.h/cpp)

### **기능**
- 형태소 분석된 일본어 문장을 **인터랙티브 HTML**로 표시
- 각 단어가 클릭 가능한 링크 (한자 포함 단어만)
- 마우스 호버 시 밑줄 표시

### **주요 메서드**

```cpp
void AddSentence(tokens, originalText)   // 문장 추가
void Clear()                             // 초기화
QString TokensToHtml(tokens)             // 토큰 → HTML 변환
void OnLinkClicked(url)                  // 링크 클릭 핸들러
```

### **구현 세부사항**

#### **HTML 생성**
```cpp
// 문장 컨테이너
<div style='margin: 10px 0; padding: 8px; background-color: #2b2b2b; border-radius: 5px;'>
    <span style='color: #0d7377; font-weight: bold;'>[번호] </span>
    // 토큰들...
</div>
```

#### **토큰별 링크**
```cpp
// 한자 포함 단어: 클릭 가능
<a href='word://surface(base64)/reading(base64)/baseForm(base64)'>表面形</a>

// 히라가나/가타카나: 일반 텍스트
<span>ひらがな</span>
```

#### **한자 판별**
```cpp
// Unicode 범위: 0x4E00 ~ 0x9FFF (CJK Unified Ideographs)
bool hasKanji = (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF);
```

#### **URL 스키마**
```
word://<surface_base64>/<reading_base64>/<baseForm_base64>
```
- Base64 인코딩으로 슬래시(/) 문제 해결
- 클릭 시 디코딩하여 `WordClicked` 시그널 발생

### **시그널**
```cpp
signals:
    void WordClicked(QString surface, QString reading, QString baseForm);
```

---

## 🖼️ RoiSelectorDialog (roi_selector_dialog.h/cpp)

### **기능**
- 게임 화면 스크린샷에서 OCR 영역 선택
- 마우스 드래그로 사각형 영역 지정

### **사용 흐름**
1. `CaptureSelectedProcess()`로 cv::Mat 이미지 전달
2. OpenCV Mat → QPixmap 변환 (`MatToPixmap()`)
3. 이미지가 800x550보다 크면 비율 유지하며 축소
4. `DraggableImageLabel`에 이미지 표시
5. 사용자가 드래그하여 영역 선택
6. "확인" 클릭 시 화면 좌표 → 원본 이미지 좌표 변환
7. 최소 크기 검증 (10x10 이상)
8. `selectedRoi_` (cv::Rect)에 저장

### **좌표 변환**
```cpp
QPoint ScreenToImage(QPoint screenPos)
```
- 축소된 이미지에서의 좌표 → 원본 이미지 좌표 변환
- 스케일 비율 계산 필요

### **반환값**
```cpp
cv::Rect GetSelectedRoi()           // 선택된 영역 (이미지 좌표)
bool HasSelection()                 // 선택 완료 여부
```

---

## 🎨 DraggableImageLabel (draggable_image_label.h/cpp)

### **기능**
- QLabel 기반 드래그 가능한 이미지 위젯
- 마우스로 사각형 영역 선택

### **마우스 이벤트**
```cpp
void mousePressEvent(QMouseEvent*)      // 시작점 기록
void mouseMoveEvent(QMouseEvent*)       // 드래그 중 사각형 업데이트
void mouseReleaseEvent(QMouseEvent*)    // 종료점 기록
void paintEvent(QPaintEvent*)           // 빨간 반투명 사각형 그리기
```

### **드래그 로직**
```cpp
// 시작
dragStartPos_ = event->pos();
isDragging_ = true;

// 업데이트
dragCurrentPos_ = event->pos();
update();  // paintEvent() 호출

// 완료
dragEndPos_ = event->pos();
isDragging_ = false;
hasSelection_ = true;
```

### **사각형 그리기** (paintEvent)
```cpp
QPainter painter(this);
painter.setPen(QPen(Qt::red, 2));
painter.setBrush(QBrush(QColor(255, 0, 0, 50)));  // 반투명 빨강
painter.drawRect(selectionRect);
```

---

## 🎨 app.ui 레이아웃 구조

### **위젯 배치** (absolute positioning)

```
┌─────────────────────────────────────────────────┐
│  sentenceListWidget (40,10, 861x291)           │  ← InteractiveSentenceWidget으로 교체
│  [실제론 숨김, 동적 교체됨]                      │
├─────────────────────────────────────────────────┤
│  dictionaryTextEdit (40,310, 521x221)          │  processComboBox (590,320, 311x21)
│  [사전 검색 결과]                                │  selectRoiButton (660,370, 181x51)
│                                                  │  startCaptureButton (660,430, 181x51)
│  debugLogTextEdit (40,540, 521x141)            │  stopCaptureButton (660,490, 181x51)
│  [디버그 로그]                                   │  
│                                                  │
│  [ankiButton] (450,470, 81x41)                  │
└─────────────────────────────────────────────────┘
```

### **스타일 (다크 테마)**

```css
QMainWindow {
    background-color: #2b2b2b;
}

QListWidget {
    background-color: #1e1e1e;
    color: #ffffff;
    border: 1px solid #3c3c3c;
    border-radius: 5px;
}

QListWidget::item:selected {
    background-color: #0d7377;  /* 청록색 */
}

QPushButton {
    background-color: #0d7377;
    color: white;
    border-radius: 5px;
    padding: 10px 20px;
}

QPushButton:hover {
    background-color: #14a085;
}

/* 캡처 시작 버튼 */
startCaptureButton {
    background-color: #14a085;
}

/* 캡처 정지 버튼 */
stopCaptureButton {
    background-color: #c0392b;  /* 빨강 */
}
stopCaptureButton:hover {
    background-color: #e74c3c;
}
```

---

## 🔄 UI 상태 관리

### **버튼 활성화 상태**

| 상태 | startCapture | stopCapture | selectRoi | processCombo |
|------|--------------|-------------|-----------|--------------|
| **초기** | ✅ | ❌ | ✅ | ✅ |
| **초기화 중** | ❌ | ❌ | ❌ | ❌ |
| **캡처 중** | ❌ | ✅ | ❌ | ❌ |
| **정지 후** | ✅ | ❌ | ✅ | ✅ |

### **필수 조건**
- **ROI 선택 전**: 프로세스 선택 필수
- **캡처 시작 전**: 프로세스 + ROI 선택 필수
- **ROI 재선택**: 캡처 정지 필요

---

## 🧵 스레드 아키텍처

```
MainApp (UI Thread)
    │
    ├── pollTimer (100ms) ──► OnPollOcrResults()
    │                              │
    │                              ▼
    ├─────► CaptureThread ──► FrameQueue ──► OcrThread
    │           │                                 │
    │           │                                 ▼
    │           └─ selectedWindow_         TesseractWrapper
    │           └─ selectedRoi_                   │
    │                                             ▼
    │                                      OCR Results
    │                                             │
    └──────────────────────────────────────────◄──┘
                                            │
                                            ▼
                                    JapaneseTokenizer
                                            │
                                            ▼
                              InteractiveSentenceWidget
```

---

## 📊 데이터 흐름

```
1. 프로세스 선택
   └─► processWindows_[index] → selectedWindow_

2. ROI 선택
   └─► RoiSelectorDialog → selectedRoi_

3. 캡처 시작
   └─► CaptureThread.Start(selectedWindow_)
       └─► 프레임 → FrameQueue

4. OCR 처리
   └─► OcrThread.Start()
       └─► FrameQueue → TesseractWrapper → OCR 결과

5. 폴링
   └─► OcrThread.GetLatestResults()
       └─► 텍스트 세그먼트 결합
           └─► JapaneseTokenizer.Tokenize()
               └─► AddSentence(tokens, originalText)

6. 문장 표시
   └─► InteractiveSentenceWidget.AddSentence()
       └─► HTML 생성 (토큰별 링크)

7. 단어 클릭
   └─► WordClicked 시그널
       └─► ShowDictionaryEntry(surface, reading, baseForm)
```

---

## 🔧 QML 전환 시 고려사항

### **유지할 로직**
- ✅ 프로세스 열거 (EnumWindows)
- ✅ 화면 캡처 (PrintWindow)
- ✅ 스레드 관리 (CaptureThread, OcrThread)
- ✅ OCR 엔진 (TesseractWrapper)
- ✅ 형태소 분석 (JapaneseTokenizer)
- ✅ 폴링 메커니즘 (QTimer 100ms)

### **QML로 재구현 필요**
- ❌ app.ui (QML ApplicationWindow)
- ❌ InteractiveSentenceWidget (QML ListView + Repeater)
- ❌ RoiSelectorDialog (QML Window + MouseArea)
- ❌ DraggableImageLabel (QML Rectangle + MouseArea)

### **C++ Backend 구조**
```cpp
class AppBackend : public QObject {
    Q_OBJECT
    
    // Properties (QML 바인딩)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY capturingChanged)
    Q_PROPERTY(QString statusText ...)
    Q_PROPERTY(QStringList processList ...)
    Q_PROPERTY(bool hasRoiSelection ...)
    
    // Invokable (QML에서 호출)
    Q_INVOKABLE void selectProcess(int index);
    Q_INVOKABLE void openRoiSelector();
    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void onWordClicked(QString surface, QString reading);
    
    // Signals (QML로 알림)
signals:
    void sentenceDetected(QString sentence, QVariantList tokens);
    void dictionaryResultReady(QString html);
    void debugLog(QString message);
    
private:
    // 기존 MainApp 로직 재사용
    std::unique_ptr<CaptureThread> captureThread_;
    std::unique_ptr<OcrThread> ocrThread_;
    // ...
};
```

### **QAbstractListModel 필요**
- **SentenceModel**: 문장 목록 (ListView 데이터 소스)
- **TokenModel**: 각 문장의 토큰 (Repeater 데이터 소스)
- **DebugLogModel**: 디버그 로그

---

## 🎯 QML 컴포넌트 매핑

| Qt Widgets | QML 컴포넌트 |
|------------|--------------|
| `InteractiveSentenceWidget` | `ListView` + 토큰별 `Repeater` |
| `RoiSelectorDialog` | `Window` + `Image` + `MouseArea` |
| `DraggableImageLabel` | `Rectangle` + `MouseArea` (드래그) |
| `QComboBox` (프로세스) | `ComboBox` (model: processList) |
| `QTextEdit` (사전) | `TextArea` (html 지원) |
| `QTextEdit` (로그) | `ListView` (로그 모델) |
| `QPushButton` | `Button` (custom style) |

---

## 📌 중요 상수/설정

```cpp
// 폴링 간격
pollTimer_->setInterval(100);  // 100ms

// 프레임 큐 크기
FrameQueue(30)  // 최대 30 프레임

// OCR 언어
ocrEngine->Initialize("", "jpn")  // 일본어

// ROI 최소 크기
width > 10 && height > 10

// 이미지 축소 최대 크기
maxWidth = 800;
maxHeight = 550;

// 한자 Unicode 범위
0x4E00 ~ 0x9FFF

// 색상 스키마
primary: #0d7377      // 청록색
hover: #14a085        // 밝은 청록색
pressed: #0a5a5d      // 어두운 청록색
error: #c0392b        // 빨강
background: #2b2b2b   // 다크 회색
panel: #1e1e1e        // 더 어두운 패널
border: #3c3c3c       // 테두리
text: #ffffff         // 흰색
```

---

## 🚀 다음 단계

1. **AppBackend 클래스 생성**
   - MainApp의 비즈니스 로직 이식
   - Q_PROPERTY, Q_INVOKABLE 메서드 정의
   - signals/slots 연결

2. **QML UI 구조 생성**
   - Main.qml (ApplicationWindow)
   - components/ (SentenceList, DictionaryView, ControlPanel 등)

3. **QAbstractListModel 구현**
   - SentenceModel (문장 + 토큰 목록)
   - DebugLogModel (타임스탬프 + 메시지)

4. **CMakeLists.txt 수정**
   - Qt6::Quick, Qt6::Qml 모듈 추가
   - qt_add_qml_module() 설정
   - QML 리소스 파일 등록

5. **main.cpp QML 엔진으로 전환**
   - QGuiApplication 사용
   - QQmlApplicationEngine 초기화
   - AppBackend 등록

---

**참고**: 이 문서는 기존 Qt Widgets 기반 코드를 QML로 전환하기 위한 레퍼런스입니다.  
모든 비즈니스 로직은 C++ Backend에 유지하고, UI만 QML로 재작성하는 방식을 권장합니다.
