// ToriYomi - Qt 메인 애플리케이션 구현
#include "main_app.h"
#include "ui_app.h"  // AUTOUIC가 생성하는 헤더
#include "roi_selector_dialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QTime>
#include <dwmapi.h>
#include <psapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "psapi.lib")

namespace toriyomi {
namespace ui {

// 윈도우 열거 콜백
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* windows = reinterpret_cast<std::vector<HWND>*>(lParam);
    
    // 보이는 윈도우만
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    // 최소화되지 않은 윈도우
    if (IsIconic(hwnd)) {
        return TRUE;
    }

    // 제목이 있는 윈도우
    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));
    if (wcslen(title) == 0) {
        return TRUE;
    }

    windows->push_back(hwnd);
    return TRUE;
}

MainApp::MainApp(QWidget* parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow())
    , sentenceWidget_(nullptr)
    , isCapturing_(false)
    , selectedWindow_(nullptr)
    , hasRoiSelection_(false) {
    
    ui_->setupUi(this);  // AUTOUIC가 생성한 UI 설정
    
    // 공유 큐 생성
    frameQueue_ = std::make_shared<FrameQueue>(30);
    
    // OCR 엔진 미리 초기화 (블로킹 방지)
    ocrEngine_ = std::make_unique<ocr::TesseractWrapper>();
    qDebug() << "Initializing OCR engine...";
    if (!ocrEngine_->Initialize("", "jpn")) {
        qDebug() << "OCR engine initialization failed!";
        QMessageBox::warning(this, "초기화 실패", "OCR 엔진을 초기화할 수 없습니다.");
    } else {
        qDebug() << "OCR engine initialized successfully";
    }
    
    // 토크나이저 초기화
    tokenizer_ = std::make_unique<tokenizer::JapaneseTokenizer>();
    
    // 폴링 타이머 생성
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(100);  // 100ms마다 폴링
    
    SetupUi();
    ConnectSignals();
    LoadProcessList();
    
    UpdateStatus("Ready");
}

MainApp::~MainApp() {
    StopThreads();
    delete ui_;
}

void MainApp::SetupUi() {
    // InteractiveSentenceWidget로 sentenceListWidget 교체
    QWidget* sentenceListPlaceholder = ui_->sentenceListWidget;
    if (sentenceListPlaceholder) {
        sentenceWidget_ = new InteractiveSentenceWidget(this);
        
        // 기존 위젯의 위치와 크기 가져오기
        QRect geometry = sentenceListPlaceholder->geometry();
        
        // 부모 위젯 가져오기
        QWidget* parent = sentenceListPlaceholder->parentWidget();
        
        // 기존 위젯 삭제
        sentenceListPlaceholder->deleteLater();
        ui_->sentenceListWidget = nullptr;
        
        // 새 위젯 설정
        sentenceWidget_->setParent(parent);
        sentenceWidget_->setGeometry(geometry);
        sentenceWidget_->show();
    }

    // 사전 패널 읽기 전용
    if (ui_->dictionaryTextEdit) {
        ui_->dictionaryTextEdit->setReadOnly(true);
    }

    // 윈도우 제목 설정
    setWindowTitle("ToriYomi - Japanese Learning Assistant");
}

void MainApp::ConnectSignals() {
    // 단어 클릭 시그널
    if (sentenceWidget_) {
        connect(sentenceWidget_, &InteractiveSentenceWidget::WordClicked,
                this, &MainApp::OnWordClicked);
    }

    // Anki 버튼 클릭
    if (ui_->ankiButton) {
        connect(ui_->ankiButton, &QPushButton::clicked,
                this, &MainApp::OnAnkiButtonClicked);
    }

    // 프로세스 선택
    if (ui_->processComboBox) {
        connect(ui_->processComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainApp::OnProcessSelected);
    }

    // ROI 선택 버튼
    if (ui_->selectRoiButton) {
        connect(ui_->selectRoiButton, &QPushButton::clicked,
                this, &MainApp::OnSelectRoiClicked);
    }

    // 캡처 시작 버튼
    if (ui_->startCaptureButton) {
        connect(ui_->startCaptureButton, &QPushButton::clicked,
                this, &MainApp::OnStartCaptureClicked);
    }

    // 캡처 정지 버튼
    if (ui_->stopCaptureButton) {
        connect(ui_->stopCaptureButton, &QPushButton::clicked,
                this, &MainApp::OnStopCaptureClicked);
    }

    // 폴링 타이머
    connect(pollTimer_, &QTimer::timeout,
            this, &MainApp::OnPollOcrResults);
}

void MainApp::LoadProcessList() {
    if (!ui_->processComboBox) {
        return;
    }

    ui_->processComboBox->clear();
    processWindows_.clear();

    // 모든 윈도우 열거
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&processWindows_));

    // 콤보박스에 추가
    for (HWND hwnd : processWindows_) {
        wchar_t title[256];
        GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));
        
        // 프로세스 이름도 가져오기
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            wchar_t processName[MAX_PATH];
            if (GetModuleBaseNameW(hProcess, nullptr, processName, MAX_PATH)) {
                QString itemText = QString("%1 (%2)")
                    .arg(QString::fromWCharArray(title))
                    .arg(QString::fromWCharArray(processName));
                ui_->processComboBox->addItem(itemText);
            }
            CloseHandle(hProcess);
        } else {
            ui_->processComboBox->addItem(QString::fromWCharArray(title));
        }
    }

    if (ui_->processComboBox->count() > 0) {
        ui_->processComboBox->setCurrentIndex(0);
    }
}

void MainApp::OnProcessSelected(int index) {
    if (index >= 0 && index < static_cast<int>(processWindows_.size())) {
        selectedWindow_ = processWindows_[index];
        hasRoiSelection_ = false;  // 프로세스 변경 시 ROI 초기화
        UpdateStatus("프로세스 선택됨");
    }
}

void MainApp::OnSelectRoiClicked() {
    if (!selectedWindow_) {
        QMessageBox::warning(this, "경고", "먼저 프로세스를 선택해주세요.");
        return;
    }

    // 선택된 윈도우 캡처
    cv::Mat screenshot = CaptureSelectedProcess();
    
    if (screenshot.empty()) {
        QMessageBox::critical(this, "Error", "화면 캡처 실패");
        return;
    }

    // ROI 선택 다이얼로그 표시
    RoiSelectorDialog dialog(screenshot, this);
    
    if (dialog.exec() == QDialog::Accepted && dialog.HasSelection()) {
        selectedRoi_ = dialog.GetSelectedRoi();
        hasRoiSelection_ = true;
        
        QString msg = QString("ROI 선택: %1x%2 at (%3, %4)")
            .arg(selectedRoi_.width)
            .arg(selectedRoi_.height)
            .arg(selectedRoi_.x)
            .arg(selectedRoi_.y);
        
        UpdateStatus(msg);
        
        QMessageBox::information(this, "성공", "캡처 영역이 선택되었습니다.");
    }
}

cv::Mat MainApp::CaptureSelectedProcess() {
    if (!selectedWindow_) {
        return cv::Mat();
    }

    // 윈도우 크기 가져오기
    RECT rect;
    if (!GetWindowRect(selectedWindow_, &rect)) {
        return cv::Mat();
    }

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0) {
        return cv::Mat();
    }

    // DC 생성
    HDC hdcWindow = GetDC(selectedWindow_);
    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // 윈도우 캡처
    PrintWindow(selectedWindow_, hdcMem, PW_RENDERFULLCONTENT);

    // HBITMAP → cv::Mat 변환
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;  // Top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    cv::Mat mat(height, width, CV_8UC4);
    GetDIBits(hdcMem, hBitmap, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // BGR 변환
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);

    // 정리
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(selectedWindow_, hdcWindow);

    return bgr;
}

void MainApp::AddSentence(const std::vector<tokenizer::Token>& tokens,
                          const std::string& originalText) {
    if (!sentenceWidget_) {
        return;
    }

    sentences_.push_back(originalText);
    sentenceWidget_->AddSentence(tokens, originalText);
    
    UpdateStatus(QString("문장 추가: %1개").arg(sentences_.size()));
}

void MainApp::ClearSentences() {
    if (sentenceWidget_) {
        sentenceWidget_->clear();
    }
    sentences_.clear();
    UpdateStatus("문장 목록 초기화");
}

void MainApp::OnWordClicked(const QString& surface, const QString& reading,
                            const QString& baseForm) {
    // 현재 선택된 단어를 사전에서 검색
    std::string surfaceStr = surface.toUtf8().constData();
    std::string readingStr = reading.toUtf8().constData();
    
    // TODO: 실제 사전 API 연동
    // 임시로 받은 정보만 표시
    std::string meaning = "Example meaning / 예시 의미";
    ShowDictionaryEntry(surfaceStr, readingStr, meaning);
}

void MainApp::ShowDictionaryEntry(const std::string& word, 
                                  const std::string& reading,
                                  const std::string& meaning) {
    if (!ui_->dictionaryTextEdit) {
        return;
    }

    QString html = QString(
        "<div style='font-family: Yu Gothic, Meiryo; color: #e0e0e0;'>"
        "<h2 style='color: #14a085;'>%1</h2>"
        "<p style='font-size: 16px; color: #0d7377;'>【%2】</p>"
        "<p style='font-size: 14px;'>%3</p>"
        "</div>"
    ).arg(QString::fromUtf8(word.c_str()))
     .arg(QString::fromUtf8(reading.c_str()))
     .arg(QString::fromUtf8(meaning.c_str()));

    ui_->dictionaryTextEdit->setHtml(html);
}

void MainApp::OnAnkiButtonClicked() {
    if (sentences_.empty()) {
        QMessageBox::warning(this, tr("경고"), tr("먼저 문장을 선택해주세요."));
        return;
    }

    // TODO: AnkiConnect 통합
    // 임시: 마지막 문장 정보 표시
    QString lastSentence = QString::fromUtf8(sentences_.back().c_str());
    QMessageBox::information(this, tr("Anki"), 
        tr("Anki에 추가 예정:\n%1").arg(lastSentence));
}

void MainApp::UpdateStatus(const QString& message) {
    statusBar()->showMessage(message);
}

void MainApp::UpdateFps(double fps) {
    QString msg = statusBar()->currentMessage();
    msg += QString(" | FPS: %1").arg(fps, 0, 'f', 1);
    statusBar()->showMessage(msg);
}

void MainApp::LogDebug(const QString& message) {
    if (ui_->debugLogTextEdit) {
        QString timestamp = QTime::currentTime().toString("hh:mm:ss.zzz");
        ui_->debugLogTextEdit->append(QString("[%1] %2").arg(timestamp, message));
        
        // 자동 스크롤
        QTextCursor cursor = ui_->debugLogTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui_->debugLogTextEdit->setTextCursor(cursor);
    }
}

void MainApp::OnStartCaptureClicked() {
    if (!selectedWindow_) {
        QMessageBox::warning(this, "경고", "먼저 프로세스를 선택해주세요.");
        return;
    }

    if (!hasRoiSelection_) {
        QMessageBox::warning(this, "경고", "먼저 ROI 영역을 선택해주세요.");
        return;
    }

    LogDebug("=== 캡처 시작 버튼 클릭 ===");
    
    // UI 상태 변경 (즉시)
    ui_->startCaptureButton->setEnabled(false);
    ui_->stopCaptureButton->setEnabled(false);  // 초기화 중에는 둘 다 비활성화
    ui_->selectRoiButton->setEnabled(false);
    ui_->processComboBox->setEnabled(false);
    
    UpdateStatus("OCR 엔진 초기화 중...");
    LogDebug("OCR 엔진 초기화 시작...");
    
    // 비동기로 스레드 초기화 (QTimer 사용하여 다음 이벤트 루프에서 실행)
    QTimer::singleShot(0, this, &MainApp::StartThreads);
}

void MainApp::OnStopCaptureClicked() {
    StopThreads();
    
    ui_->startCaptureButton->setEnabled(true);
    ui_->stopCaptureButton->setEnabled(false);
    ui_->selectRoiButton->setEnabled(true);
    ui_->processComboBox->setEnabled(true);
    
    UpdateStatus("캡처 정지");
}

void MainApp::StartThreads() {
    if (!selectedWindow_ || !hasRoiSelection_) {
        UpdateStatus("프로세스와 ROI를 선택하세요");
        LogDebug("ERROR: 프로세스 또는 ROI 미선택");
        OnThreadsInitialized();  // 실패 시에도 UI 복구
        return;
    }

    LogDebug(QString("선택된 윈도우: %1").arg(reinterpret_cast<quintptr>(selectedWindow_)));
    LogDebug(QString("ROI: %1x%2 at (%3,%4)")
        .arg(selectedRoi_.width).arg(selectedRoi_.height)
        .arg(selectedRoi_.x).arg(selectedRoi_.y));

    // OCR 엔진 생성 (Tesseract)
    LogDebug("OCR 엔진 생성 중...");
    auto ocrEngine = std::make_unique<ocr::TesseractWrapper>();
    
    LogDebug("OCR 엔진 초기화 중 (jpn)...");
    if (!ocrEngine->Initialize("", "jpn")) {
        UpdateStatus("OCR 엔진 초기화 실패");
        LogDebug("ERROR: OCR 엔진 초기화 실패");
        QMessageBox::critical(this, "오류", "OCR 엔진을 초기화할 수 없습니다.\nTesseract와 jpn.traineddata가 설치되어 있는지 확인하세요.");
        OnThreadsInitialized();  // 실패 시에도 UI 복구
        return;
    }
    LogDebug("OCR 엔진 초기화 완료");

    // CaptureThread 생성 및 시작
    LogDebug("캡처 스레드 생성 중...");
    captureThread_ = std::make_unique<capture::CaptureThread>(frameQueue_);
    
    LogDebug("캡처 스레드 시작 중...");
    if (!captureThread_->Start(selectedWindow_)) {
        UpdateStatus("캡처 스레드 시작 실패");
        LogDebug("ERROR: 캡처 스레드 시작 실패");
        captureThread_.reset();
        OnThreadsInitialized();  // 실패 시에도 UI 복구
        return;
    }
    LogDebug("캡처 스레드 시작 완료");
    
    // OcrThread 생성 및 시작
    LogDebug("OCR 스레드 생성 중...");
    ocrThread_ = std::make_unique<ocr::OcrThread>(frameQueue_, std::move(ocrEngine));
    
    LogDebug("OCR 스레드 시작 중...");
    if (!ocrThread_->Start()) {
        UpdateStatus("OCR 스레드 시작 실패");
        LogDebug("ERROR: OCR 스레드 시작 실패");
        captureThread_->Stop();
        captureThread_.reset();
        ocrThread_.reset();
        OnThreadsInitialized();  // 실패 시에도 UI 복구
        return;
    }
    LogDebug("OCR 스레드 시작 완료");
    
    // 폴링 타이머 시작
    pollTimer_->start();
    LogDebug("폴링 타이머 시작 (100ms 간격)");
    
    isCapturing_ = true;
    UpdateStatus("캡처 중...");
    LogDebug("=== 모든 스레드 시작 완료 ===");
    
    // UI 상태 변경
    ui_->stopCaptureButton->setEnabled(true);
}

void MainApp::StopThreads() {
    if (!isCapturing_) {
        return;
    }
    
    LogDebug("=== 캡처 정지 시작 ===");
    
    // 폴링 타이머 정지
    pollTimer_->stop();
    LogDebug("폴링 타이머 정지");
    
    if (captureThread_) {
        LogDebug("캡처 스레드 정지 중...");
        captureThread_->Stop();
        captureThread_.reset();
        LogDebug("캡처 스레드 정지 완료");
    }
    
    if (ocrThread_) {
        LogDebug("OCR 스레드 정지 중...");
        ocrThread_->Stop();
        ocrThread_.reset();
        LogDebug("OCR 스레드 정지 완료");
    }
    
    isCapturing_ = false;
    UpdateStatus("캡처 정지");
    LogDebug("=== 캡처 정지 완료 ===");
}

void MainApp::OnThreadsInitialized() {
    // 초기화 완료 또는 실패 후 UI 상태 복구
    if (isCapturing_) {
        // 성공적으로 시작됨
        ui_->stopCaptureButton->setEnabled(true);
    } else {
        // 실패 - UI를 원래 상태로 복구
        ui_->startCaptureButton->setEnabled(true);
        ui_->stopCaptureButton->setEnabled(false);
        ui_->selectRoiButton->setEnabled(true);
        ui_->processComboBox->setEnabled(true);
    }
}

void MainApp::OnPollOcrResults() {
    if (!ocrThread_) {
        return;
    }

    // OCR 결과 가져오기
    auto results = ocrThread_->GetLatestResults();
    if (results.empty()) {
        return;
    }

    LogDebug(QString("OCR 결과 수신: %1개 세그먼트").arg(results.size()));

    // 모든 텍스트 세그먼트 결합
    std::string fullText;
    for (const auto& segment : results) {
        fullText += segment.text + " ";
        LogDebug(QString("  세그먼트: '%1' (신뢰도: %2)")
            .arg(QString::fromStdString(segment.text))
            .arg(segment.confidence));
    }

    if (fullText.empty()) {
        LogDebug("WARNING: 결합된 텍스트가 비어있음");
        return;
    }

    LogDebug(QString("결합된 텍스트: '%1'").arg(QString::fromStdString(fullText)));

    // 형태소 분석
    LogDebug("형태소 분석 시작...");
    std::vector<tokenizer::Token> tokens = tokenizer_->Tokenize(fullText);
    if (tokens.empty()) {
        LogDebug("WARNING: 토큰화 실패 (결과 없음)");
        return;
    }

    LogDebug(QString("토큰 개수: %1개").arg(tokens.size()));
    size_t maxTokensToShow = (tokens.size() < 5) ? tokens.size() : 5;
    for (size_t i = 0; i < maxTokensToShow; ++i) {
        LogDebug(QString("  토큰[%1]: '%2' (품사: %3)")
            .arg(i)
            .arg(QString::fromStdString(tokens[i].surface))
            .arg(QString::fromStdString(tokens[i].partOfSpeech)));
    }

    // UI에 표시
    LogDebug("문장 추가 중...");
    AddSentence(tokens, fullText);
    LogDebug("문장 추가 완료");
}

}  // namespace ui
}  // namespace toriyomi
