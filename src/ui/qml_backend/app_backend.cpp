#include "ui/qml_backend/app_backend.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <windows.h>
#include <psapi.h>

namespace {

QString CurrentTimestamp() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

}

namespace toriyomi {
namespace ui {

AppBackend::AppBackend(QObject* parent)
    : QObject(parent)
{
    fprintf(stderr, "[AppBackend] 생성자 시작\n");
    
    try {
        pollTimer_ = new QTimer(this);
        fprintf(stderr, "[AppBackend] QTimer 생성 완료\n");
        
        connect(pollTimer_, &QTimer::timeout, this, &AppBackend::OnPollOcrResults);
        fprintf(stderr, "[AppBackend] QTimer 연결 완료\n");
        
        SetStatusMessage("준비됨");
        fprintf(stderr, "[AppBackend] 상태 메시지 설정 완료\n");
        
        qDebug() << "[AppBackend] 초기화 완료";
    } catch (const std::exception& e) {
        fprintf(stderr, "[AppBackend] 생성자 예외: %s\n", e.what());
        throw;
    } catch (...) {
        fprintf(stderr, "[AppBackend] 생성자 알 수 없는 예외\n");
        throw;
    }
}

AppBackend::~AppBackend() {
    shutdownRequested_.store(true);
    stopCapture();

    {
        std::lock_guard<std::mutex> lock(cleanupFuturesMutex_);
        for (auto& futurePtr : cleanupFutures_) {
            if (futurePtr && futurePtr->valid()) {
                futurePtr->wait();
            }
        }
        cleanupFutures_.clear();
    }

    ocrEngine_.reset();
    tokenizer_.reset();
}

void AppBackend::refreshProcessList() {
    qDebug() << "[AppBackend] 프로세스 목록 새로고침";
    
    processList_.clear();
    processWindows_.clear();

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* backend = reinterpret_cast<AppBackend*>(lParam);
        
        if (!IsWindowVisible(hwnd)) {
            return TRUE;
        }

        wchar_t title[256] = {0};
        GetWindowTextW(hwnd, title, 256);
        if (wcslen(title) == 0) {
            return TRUE;
        }

        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            wchar_t processName[MAX_PATH] = {0};
            HMODULE hMod;
            DWORD cbNeeded;
            
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                GetModuleBaseNameW(hProcess, hMod, processName, MAX_PATH);
            }
            CloseHandle(hProcess);

            QString displayText = QString::fromWCharArray(title);
            if (wcslen(processName) > 0) {
                displayText += QString(" (%1)").arg(QString::fromWCharArray(processName));
            }

            backend->processList_.append(displayText);
            backend->processWindows_.push_back(hwnd);
        }

        return TRUE;
    }, reinterpret_cast<LPARAM>(this));

    qDebug() << "[AppBackend] 프로세스" << processList_.size() << "개 발견";
    emit processListChanged();
    emit logMessage(QString("[%1] 프로세스 %2개 발견")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(processList_.size()));
}

void AppBackend::selectProcess(int index) {
    if (index < 0 || index >= static_cast<int>(processWindows_.size())) {
        qWarning() << "[AppBackend] 잘못된 프로세스 인덱스:" << index;
        return;
    }

    selectedWindow_ = processWindows_[index];
    hasRoiSelection_ = false;
    
    qDebug() << "[AppBackend] 프로세스 선택:" << processList_[index];
    emit logMessage(QString("[%1] 프로세스 선택: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(processList_[index]));
}

void AppBackend::selectRoi(int x, int y, int width, int height) {
    selectedRoi_ = cv::Rect(x, y, width, height);
    hasRoiSelection_ = true;
    
    qDebug() << "[AppBackend] ROI 선택:" << x << y << width << height;
    emit logMessage(QString("[%1] ROI 선택: (%2, %3, %4x%5)")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(x).arg(y).arg(width).arg(height));
}

void AppBackend::startCapture() {
    if (isCapturing_) {
        qWarning() << "[AppBackend] 이미 캡처 중";
        return;
    }

    if (!selectedWindow_) {
        SetStatusMessage("프로세스를 먼저 선택하세요");
        emit logMessage(QString("[%1] 오류: 프로세스 미선택")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return;
    }

    // ROI 미선택 시 전체 윈도우 크기로 설정
    if (!hasRoiSelection_) {
        RECT rect;
        if (GetWindowRect(selectedWindow_, &rect)) {
            selectedRoi_ = cv::Rect(0, 0, rect.right - rect.left, rect.bottom - rect.top);
            hasRoiSelection_ = true;
            
            qDebug() << "[AppBackend] ROI 미선택 - 전체 윈도우 캡처:" 
                     << selectedRoi_.width << "x" << selectedRoi_.height;
            emit logMessage(QString("[%1] ROI 미선택 - 전체 윈도우 캡처 (%2x%3)")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                .arg(selectedRoi_.width)
                .arg(selectedRoi_.height));
        } else {
            SetStatusMessage("윈도우 크기를 가져올 수 없습니다");
            emit logMessage(QString("[%1] 오류: 윈도우 크기 조회 실패")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            return;
        }
    }

    qDebug() << "[AppBackend] 캡처 시작...";
    emit logMessage(QString("[%1] 캡처 시작 중...")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

    QTimer::singleShot(0, this, [this]() {
        auto cleanupNow = [this]() {
            auto resources = std::make_shared<CleanupResources>();
            resources->overlay = std::move(overlayThread_);
            resources->ocr = std::move(ocrThread_);
            resources->capture = std::move(captureThread_);
            resources->frameQueue = std::move(frameQueue_);
            CleanupThreads(resources);
        };

        InitializeEngines();
        
        if (!ocrEngine_ || !tokenizer_) {
            SetStatusMessage("엔진 초기화 실패");
            emit logMessage(QString("[%1] 오류: 엔진 초기화 실패")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            return;
        }

        frameQueue_ = std::make_shared<toriyomi::FrameQueue>(8);
        captureThread_ = std::make_unique<capture::CaptureThread>(frameQueue_);
        
        if (!captureThread_->Start(selectedWindow_)) {
            SetStatusMessage("캡처 스레드 시작 실패");
            emit logMessage(QString("[%1] 오류: 캡처 스레드 시작 실패")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            cleanupNow();
            return;
        }

        // ocrEngine_을 shared_ptr로 OcrThread에 전달 (생명주기 공유)
        ocrThread_ = std::make_unique<ocr::OcrThread>(frameQueue_, ocrEngine_);
        
        if (!ocrThread_->Start()) {
            SetStatusMessage("OCR 스레드 시작 실패");
            emit logMessage(QString("[%1] 오류: OCR 스레드 시작 실패")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            cleanupNow();
            return;
        }

        overlayThread_ = std::make_unique<OverlayThread>();
        
        if (!overlayThread_->Start(selectedRoi_.x, selectedRoi_.y, 
                                    selectedRoi_.width, selectedRoi_.height)) {
            SetStatusMessage("오버레이 스레드 시작 실패");
            emit logMessage(QString("[%1] 경고: 오버레이 시작 실패")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        }

        pollTimer_->start(100);

        isCapturing_ = true;
        emit isCapturingChanged();
        SetStatusMessage("캡처 중...");
        emit logMessage(QString("[%1] 캡처 시작 완료")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

        qDebug() << "[AppBackend] 캡처 시작 완료";
    });
}

void AppBackend::stopCapture() {
    const bool hasWorkers = HasActiveWorkers();

    if (!isCapturing_ && !hasWorkers) {
        if (HasCleanupInFlight()) {
            SetStatusMessage("Stopping...");
        }
        return;
    }

    if (pollTimer_) {
        pollTimer_->stop();
    }

    if (isCapturing_) {
        isCapturing_ = false;
        emit isCapturingChanged();
    }

    emit logMessage(QString("[%1] 캡처 중지 중...").arg(CurrentTimestamp()));
    SetStatusMessage("Stopping...");

    auto resources = std::make_shared<CleanupResources>();
    resources->overlay = std::move(overlayThread_);
    resources->ocr = std::move(ocrThread_);
    resources->capture = std::move(captureThread_);
    resources->frameQueue = std::move(frameQueue_);

    cleanupTasksInFlight_.fetch_add(1);
    auto futurePtr = std::make_shared<std::future<void>>();
    *futurePtr = std::async(std::launch::async, [this, resources, futurePtr]() {
        const auto summary = CleanupThreads(resources);
        QMetaObject::invokeMethod(this, [this, summary, futurePtr]() {
            HandleCleanupFinished(summary, futurePtr);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lock(cleanupFuturesMutex_);
        cleanupFutures_.push_back(futurePtr);
    }
}

void AppBackend::requestShutdown() {
    const bool alreadyRequested = shutdownRequested_.exchange(true);
    if (!alreadyRequested) {
        emit logMessage(QString("[%1] Shutdown requested (window close)")
            .arg(CurrentTimestamp()));
    } else {
        emit logMessage(QString("[%1] Shutdown already in progress")
            .arg(CurrentTimestamp()));
    }
    stopCapture();

    if (!HasCleanupInFlight() && !HasActiveWorkers()) {
        emit logMessage(QString("[%1] Exiting application (no active work)")
            .arg(CurrentTimestamp()));
        QCoreApplication::quit();
    }
}

void AppBackend::clearSentences() {
    std::lock_guard<std::mutex> lock(sentencesMutex_);
    sentences_.clear();
    
    qDebug() << "[AppBackend] 문장 목록 초기화";
    emit logMessage(QString("[%1] 문장 목록 초기화")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void AppBackend::OnPollOcrResults() {
    if (!ocrThread_) {
        return;
    }

    auto results = ocrThread_->GetLatestResults();
    if (results.empty()) {
        return;
    }

    for (const auto& segment : results) {
        if (segment.text.empty()) {
            continue;
        }

        if (!tokenizer_) {
            continue;
        }

        auto tokens = tokenizer_->Tokenize(segment.text);
        
        if (tokens.empty()) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(sentencesMutex_);
            sentences_.push_back(segment.text);
        }

        QVariantList qmlTokens;
        for (const auto& token : tokens) {
            QVariantMap tokenMap;
            tokenMap["surface"] = QString::fromStdString(token.surface);
            tokenMap["reading"] = QString::fromStdString(token.reading);
            tokenMap["baseForm"] = QString::fromStdString(token.baseForm);
            tokenMap["partOfSpeech"] = QString::fromStdString(token.partOfSpeech);
            qmlTokens.append(tokenMap);
        }

        QString originalText = QString::fromStdString(segment.text);
        
        emit sentenceDetected(originalText, qmlTokens);
        emit logMessage(QString("[%1] 문장 감지: %2")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(originalText));
    }
}

void AppBackend::InitializeEngines() {
    qDebug() << "[AppBackend] 엔진 초기화 시작...";
    emit logMessage(QString("[%1] 엔진 초기화 중...")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

    // shared_ptr로 생성 (OcrThread와 생명주기 공유)
    ocrEngine_ = std::make_shared<ocr::TesseractWrapper>();
    
    std::vector<std::string> tessdataPaths = {
        "C:/vcpkg/installed/x64-windows/share/tessdata",
        "C:/Program Files/Tesseract-OCR/tessdata",
        "./tessdata",
        "../tessdata"
    };

    bool tesseractInitialized = false;
    for (const auto& path : tessdataPaths) {
        qDebug() << "[AppBackend] Tesseract 초기화 시도:" << QString::fromStdString(path);
        
        if (ocrEngine_->Initialize(path, "jpn")) {
            tesseractInitialized = true;
            qDebug() << "[AppBackend] Tesseract 초기화 성공:" << QString::fromStdString(path);
            emit logMessage(QString("[%1] Tesseract 초기화 성공: %2")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                .arg(QString::fromStdString(path)));
            break;
        }
    }

    if (!tesseractInitialized) {
        qCritical() << "[AppBackend] Tesseract 초기화 실패!";
        emit logMessage(QString("[%1] 오류: Tesseract 초기화 실패 (jpn.traineddata 파일 확인 필요)")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        ocrEngine_.reset();
        return;
    }

    tokenizer_ = std::make_unique<tokenizer::JapaneseTokenizer>();
    
    if (!tokenizer_->Initialize()) {
        qCritical() << "[AppBackend] MeCab 초기화 실패!";
        emit logMessage(QString("[%1] 오류: MeCab 초기화 실패")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        tokenizer_.reset();
        return;
    }

    qDebug() << "[AppBackend] 엔진 초기화 완료";
    emit logMessage(QString("[%1] 엔진 초기화 완료")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

AppBackend::CleanupSummary AppBackend::CleanupThreads(const std::shared_ptr<CleanupResources>& resources) {
    CleanupSummary summary;

    if (resources && resources->overlay) {
        resources->overlay->Stop();
        summary.overlayStopped = true;
    }

    if (resources && resources->ocr) {
        resources->ocr->Stop();
        summary.ocrStopped = true;
    }

    if (resources && resources->capture) {
        resources->capture->Stop();
        summary.captureStopped = true;
    }

    if (resources && resources->frameQueue) {
        resources->frameQueue->Clear();
    }

    return summary;
}

bool AppBackend::HasActiveWorkers() const {
    return overlayThread_ || ocrThread_ || captureThread_;
}

bool AppBackend::HasCleanupInFlight() const {
    return cleanupTasksInFlight_.load() > 0;
}

void AppBackend::HandleCleanupFinished(const CleanupSummary& summary,
                                       const std::shared_ptr<std::future<void>>& futureRef) {
    if (futureRef && futureRef->valid()) {
        futureRef->wait();
    }

    {
        std::lock_guard<std::mutex> lock(cleanupFuturesMutex_);
        auto it = std::find(cleanupFutures_.begin(), cleanupFutures_.end(), futureRef);
        if (it != cleanupFutures_.end()) {
            cleanupFutures_.erase(it);
        }
    }

    const int remaining = cleanupTasksInFlight_.fetch_sub(1) - 1;

    if (summary.overlayStopped) {
        emit logMessage(QString("[%1] 오버레이 스레드 정리 완료").arg(CurrentTimestamp()));
    }

    if (summary.ocrStopped) {
        emit logMessage(QString("[%1] OCR 스레드 정리 완료").arg(CurrentTimestamp()));
    }

    if (summary.captureStopped) {
        emit logMessage(QString("[%1] 캡처 스레드 정리 완료").arg(CurrentTimestamp()));
    }

    if (!isCapturing_ && remaining == 0) {
        SetStatusMessage("Stopped");
        emit logMessage(QString("[%1] 캡처 중지 완료").arg(CurrentTimestamp()));
    }

    if (shutdownRequested_.load() && remaining == 0 && !HasActiveWorkers()) {
        emit logMessage(QString("[%1] All cleanup finished, quitting application")
            .arg(CurrentTimestamp()));
        QCoreApplication::quit();
    }
}

void AppBackend::SetStatusMessage(const QString& message) {
    if (statusMessage_ != message) {
        statusMessage_ = message;
        emit statusMessageChanged();
    }
}

} // namespace ui
} // namespace toriyomi
