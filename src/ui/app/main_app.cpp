// ToriYomi - Qt 메인 애플리케이션 구현
#include "main_app.h"
#include "ui_app.h"  // AUTOUIC가 생성하는 헤더
#include "roi_selector_dialog.h"
#include <QMessageBox>
#include <QDebug>
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
    , selectedWindow_(nullptr)
    , hasRoiSelection_(false) {
    
    ui_->setupUi(this);  // AUTOUIC가 생성한 UI 설정
    SetupUi();
    ConnectSignals();
    LoadProcessList();
    
    UpdateStatus("Ready");
}

MainApp::~MainApp() {
    delete ui_;
}

void MainApp::SetupUi() {
    // AUTOUIC가 이미 UI를 설정했으므로, 추가 설정만 수행
    
    // InteractiveSentenceWidget로 sentenceListWidget 교체
    QWidget* sentenceListPlaceholder = ui_->sentenceListWidget;
    if (sentenceListPlaceholder) {
        sentenceWidget_ = new InteractiveSentenceWidget(this);
        
        // 기존 위젯의 레이아웃에서 제거하고 새 위젯 추가
        QLayout* layout = sentenceListPlaceholder->parentWidget()->layout();
        if (layout) {
            int index = layout->indexOf(sentenceListPlaceholder);
            if (index >= 0) {
                layout->removeWidget(sentenceListPlaceholder);
                layout->addWidget(sentenceWidget_);
                sentenceListPlaceholder->hide();
            }
        }
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

}  // namespace ui
}  // namespace toriyomi
