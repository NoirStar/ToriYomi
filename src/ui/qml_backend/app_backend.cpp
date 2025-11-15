#include "ui/qml_backend/app_backend.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QPointer>
#include <QStandardPaths>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <psapi.h>
#include <opencv2/imgcodecs.hpp>

namespace {

QString CurrentTimestamp() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

toriyomi::ocr::OcrBootstrapConfig BuildDefaultOcrConfig() {
    toriyomi::ocr::OcrBootstrapConfig config;
    config.tessdataSearchPaths = {
        "C:/vcpkg/installed/x64-windows/share/tessdata",
        "C:/Program Files/Tesseract-OCR/tessdata",
        "./tessdata",
        "../tessdata"
    };
    const QDir baseDir(QCoreApplication::applicationDirPath());
    const QString paddleModels = QDir::cleanPath(baseDir.filePath("models/paddleocr"));
    config.paddleModelDirectory = QDir::toNativeSeparators(paddleModels).toStdString();
    config.paddleLanguage = "jpn";
    config.allowTesseractFallback = true;
    return config;
}

QString OcrEngineNameForDisplay(toriyomi::ocr::OcrEngineType type) {
    switch (type) {
        case toriyomi::ocr::OcrEngineType::Tesseract:
            return QStringLiteral("Tesseract");
        case toriyomi::ocr::OcrEngineType::PaddleOCR:
            return QStringLiteral("PaddleOCR");
        case toriyomi::ocr::OcrEngineType::EasyOCR:
            return QStringLiteral("EasyOCR");
        default:
            return QStringLiteral("Unknown");
    }
}

}

namespace toriyomi {
namespace ui {

AppBackend::AppBackend(QObject* parent)
    : QObject(parent),
      ocrBootstrapper_(BuildDefaultOcrConfig())
{
    fprintf(stderr, "[AppBackend] 생성자 시작\n");
    
    try {
        pollTimer_ = new QTimer(this);
        fprintf(stderr, "[AppBackend] QTimer 생성 완료\n");
        
        connect(pollTimer_, &QTimer::timeout, this, &AppBackend::OnPollOcrResults);
        fprintf(stderr, "[AppBackend] QTimer 연결 완료\n");
        
        SetStatusMessage("준비됨");
    sentenceAssembler_.SetCaptureIntervalSeconds(captureIntervalSeconds_);
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

    {
        std::lock_guard<std::mutex> lock(tokenizationFuturesMutex_);
        for (auto& futurePtr : tokenizationFutures_) {
            if (futurePtr && futurePtr->valid()) {
                futurePtr->wait();
            }
        }
        tokenizationFutures_.clear();
    }

    ocrEngine_.reset();
    tokenizer_.reset();
}

void AppBackend::refreshProcessList() {
    qDebug() << "[AppBackend] 프로세스 목록 새로고침";
    
    const auto processes = ProcessEnumerator::EnumerateVisibleWindows(GetCurrentProcessId());
    processList_.clear();
    processWindows_.clear();

    for (const auto& entry : processes) {
        processList_.append(entry.displayText);
        processWindows_.push_back(entry.windowHandle);
    }

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
    refreshPreviewImage();
    
    qDebug() << "[AppBackend] 프로세스 선택:" << processList_[index];
    emit logMessage(QString("[%1] 프로세스 선택: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(processList_[index]));
}

void AppBackend::selectRoi(int x, int y, int width, int height) {
    if (!selectedWindow_ || !IsWindow(selectedWindow_)) {
        qWarning() << "[AppBackend] ROI 선택 전 유효한 윈도우가 없습니다";
        return;
    }

    RECT clientRect;
    if (!GetClientRect(selectedWindow_, &clientRect)) {
        qWarning() << "[AppBackend] 클라이언트 영역 조회 실패";
        return;
    }

    const int clientWidth = std::max(1L, clientRect.right - clientRect.left);
    const int clientHeight = std::max(1L, clientRect.bottom - clientRect.top);

    auto clampValue = [](int value, int minValue, int maxValue) {
        if (value < minValue) {
            return minValue;
        }
        if (value > maxValue) {
            return maxValue;
        }
        return value;
    };

    const int clampedX = clampValue(x, 0, clientWidth - 1);
    const int clampedY = clampValue(y, 0, clientHeight - 1);
    const int maxWidth = clientWidth - clampedX;
    const int maxHeight = clientHeight - clampedY;
    const int clampedWidth = std::max(1, std::min(width, maxWidth));
    const int clampedHeight = std::max(1, std::min(height, maxHeight));

    selectedRoi_ = cv::Rect(clampedX, clampedY, clampedWidth, clampedHeight);
    hasRoiSelection_ = true;
    ApplyRoiToOcrThread();
    
    qDebug() << "[AppBackend] ROI 선택:" << clampedX << clampedY << clampedWidth << clampedHeight;
    emit logMessage(QString("[%1] ROI 선택: (%2, %3, %4x%5)")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(clampedX).arg(clampedY).arg(clampedWidth).arg(clampedHeight));
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
        if (GetClientRect(selectedWindow_, &rect)) {
            selectedRoi_ = cv::Rect(0, 0, rect.right - rect.left, rect.bottom - rect.top);
            hasRoiSelection_ = true;
            ApplyRoiToOcrThread();
            
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
        sentenceAssembler_.Reset();

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
        captureThread_->SetChangeDetection(false);
    const int captureIntervalMs = std::max(10, static_cast<int>(std::round(captureIntervalSeconds_ * 1000.0)));
    captureThread_->SetCaptureIntervalMilliseconds(captureIntervalMs);
        
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

        ApplyRoiToOcrThread();

    overlayThread_ = std::make_unique<OverlayThread>();

    POINT clientTopLeft{0, 0};
    const bool clientToScreenOk = ClientToScreen(selectedWindow_, &clientTopLeft);
    const int overlayX = clientToScreenOk ? clientTopLeft.x + selectedRoi_.x : selectedRoi_.x;
    const int overlayY = clientToScreenOk ? clientTopLeft.y + selectedRoi_.y : selectedRoi_.y;
        
    if (!overlayThread_->Start(overlayX, overlayY, 
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

    sentenceAssembler_.Reset();

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
    sentenceAssembler_.Reset();
    
    qDebug() << "[AppBackend] 문장 목록 초기화";
    emit logMessage(QString("[%1] 문장 목록 초기화")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void AppBackend::setCaptureIntervalSeconds(double seconds) {
    const double clamped = std::clamp(seconds, 0.1, 5.0);
    const bool changed = std::abs(captureIntervalSeconds_ - clamped) > 0.0001;
    captureIntervalSeconds_ = clamped;
    sentenceAssembler_.SetCaptureIntervalSeconds(captureIntervalSeconds_);

    if (changed) {
        emit captureIntervalSecondsChanged();
    }

    if (captureThread_) {
        const int intervalMs = std::max(10, static_cast<int>(std::round(captureIntervalSeconds_ * 1000.0)));
        captureThread_->SetCaptureIntervalMilliseconds(intervalMs);
    }
}

void AppBackend::setOcrEngineType(int engineType) {
    ocr::OcrEngineType resolved = selectedEngineType_;

    switch (engineType) {
        case static_cast<int>(ocr::OcrEngineType::Tesseract):
            resolved = ocr::OcrEngineType::Tesseract;
            break;
        case static_cast<int>(ocr::OcrEngineType::PaddleOCR):
            resolved = ocr::OcrEngineType::PaddleOCR;
            break;
        default:
            emit logMessage(QString("[%1] 지원되지 않는 OCR 타입: %2")
                                .arg(CurrentTimestamp())
                                .arg(engineType));
            return;
    }

    if (resolved == selectedEngineType_) {
        return;
    }

    selectedEngineType_ = resolved;
    ocrBootstrapper_.SetPreferredEngine(selectedEngineType_);
    emit ocrEngineTypeChanged();

    emit logMessage(QString("[%1] OCR 엔진 타입 변경: %2")
                        .arg(CurrentTimestamp())
                        .arg(OcrEngineNameForDisplay(selectedEngineType_)));

    if (isCapturing_) {
        emit logMessage(QString("[%1] 변경 사항은 다음 캡처 시작 시 적용됩니다")
                            .arg(CurrentTimestamp()));
    }
}

void AppBackend::refreshPreviewImage() {
    auto pixmap = CaptureWindowPreview();

    if (pixmap.isNull()) {
        if (!previewImageData_.isEmpty() || previewImageSize_.isValid()) {
            previewImageData_.clear();
            previewImageSize_ = QSize();
            emit previewImageDataChanged();
        }
        return;
    }

    QImage image = pixmap.toImage();
    previewImageSize_ = image.size();
    const QSize maxPreviewSize(960, 540);
    if (image.width() > maxPreviewSize.width() || image.height() > maxPreviewSize.height()) {
        image = image.scaled(maxPreviewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        qWarning() << "[AppBackend] 미리보기 이미지 PNG 저장 실패";
        return;
    }

    const QString encoded = QString::fromLatin1(buffer.data().toBase64());
    previewImageData_ = QStringLiteral("data:image/png;base64,%1").arg(encoded);
    emit previewImageDataChanged();
}

QString AppBackend::saveCurrentRoiSnapshot() {
    auto pixmap = CaptureWindowPreview();

    if (pixmap.isNull()) {
        emit logMessage(QString("[%1] ROI 스냅샷 실패: 미리보기 이미지를 가져올 수 없습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return QString();
    }

    QRect roiRect = hasRoiSelection_
        ? QRect(selectedRoi_.x, selectedRoi_.y, selectedRoi_.width, selectedRoi_.height)
        : QRect(0, 0, pixmap.width(), pixmap.height());

    QRect bounded = roiRect.intersected(QRect(0, 0, pixmap.width(), pixmap.height()));
    if (!bounded.isValid() || bounded.isEmpty()) {
        emit logMessage(QString("[%1] ROI 스냅샷 실패: ROI가 창 범위를 벗어났습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return QString();
    }

    QImage roiImage = pixmap.copy(bounded).toImage();
    if (roiImage.isNull()) {
        emit logMessage(QString("[%1] ROI 스냅샷 실패: 이미지를 복사할 수 없습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return QString();
    }

    QString picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesDir.isEmpty()) {
        picturesDir = QDir::currentPath();
    }

    QDir dir(picturesDir);
    if (!dir.exists("ToriYomiDebug")) {
        dir.mkpath("ToriYomiDebug");
    }

    if (!dir.cd("ToriYomiDebug")) {
        emit logMessage(QString("[%1] ROI 스냅샷 실패: 디렉터리를 생성할 수 없습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return QString();
    }

    const QString filePath = dir.filePath(QString("roi_debug_%1.png")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    if (!roiImage.save(filePath, "PNG")) {
        emit logMessage(QString("[%1] ROI 스냅샷 실패: 파일 저장 오류")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return QString();
    }

    emit logMessage(QString("[%1] ROI 스냅샷 저장: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(filePath));

    return filePath;
}

void AppBackend::runSampleOcr(const QString& imagePath) {
    const QString trimmed = imagePath.trimmed();
    if (trimmed.isEmpty()) {
        emit logMessage(QString("[%1] Sample OCR 실패: 경로가 비었습니다")
            .arg(CurrentTimestamp()));
        return;
    }

    QFileInfo fileInfo(trimmed);
    if (!fileInfo.isAbsolute()) {
        fileInfo.setFile(QDir::current(), trimmed);
    }

    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit logMessage(QString("[%1] Sample OCR 실패: 파일을 찾을 수 없습니다 (%2)")
            .arg(CurrentTimestamp())
            .arg(trimmed));
        return;
    }

    if (!ocrEngine_ || !ocrEngine_->IsInitialized()) {
        InitializeEngines();
        if (!ocrEngine_ || !ocrEngine_->IsInitialized()) {
            emit logMessage(QString("[%1] Sample OCR 실패: OCR 엔진을 초기화할 수 없습니다")
                .arg(CurrentTimestamp()));
            return;
        }
    }

    const std::string absolutePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath()).toStdString();
    cv::Mat sample = cv::imread(absolutePath, cv::IMREAD_COLOR);
    if (sample.empty()) {
        emit logMessage(QString("[%1] Sample OCR 실패: 이미지를 열 수 없습니다 (%2)")
            .arg(CurrentTimestamp())
            .arg(fileInfo.absoluteFilePath()));
        return;
    }

    auto results = ocrEngine_->RecognizeText(sample);
    emit logMessage(QString("[%1] Sample OCR: 세그먼트 %2개 (파일: %3)")
        .arg(CurrentTimestamp())
        .arg(results.size())
        .arg(fileInfo.fileName()));

    if (results.empty()) {
        return;
    }

    QStringList recognizedLines;
    for (const auto& segment : results) {
        const QString text = QString::fromStdString(segment.text).trimmed();
        if (text.isEmpty()) {
            continue;
        }

        recognizedLines.append(text);

        emit logMessage(QString("[%1]  → \"%2\" (conf=%3, rect=%4,%5 %6x%7)")
            .arg(CurrentTimestamp())
            .arg(text)
            .arg(QString::number(segment.confidence, 'f', 1))
            .arg(segment.boundingBox.x)
            .arg(segment.boundingBox.y)
            .arg(segment.boundingBox.width)
            .arg(segment.boundingBox.height));
    }

    if (!recognizedLines.isEmpty()) {
        const QString combined = recognizedLines.join("\n");
        emit logMessage(QString("[%1] Sample OCR 텍스트:\n%2")
            .arg(CurrentTimestamp())
            .arg(combined));
    }
}

void AppBackend::OnPollOcrResults() {
    if (!ocrThread_) {
        sentenceAssembler_.Reset();
        return;
    }

    const auto results = ocrThread_->GetLatestResults();
    auto logHook = [this](const QString& message) {
        emit logMessage(message);
    };

    auto assembled = sentenceAssembler_.TryAssemble(results, logHook);
    if (!assembled.has_value()) {
        return;
    }

    sentenceAssembler_.MarkSentenceInFlight(*assembled);
    DispatchSentenceForTokenization(*assembled);
}

void AppBackend::InitializeEngines() {
    qDebug() << "[AppBackend] 엔진 초기화 시작...";
    emit logMessage(QString("[%1] 엔진 초기화 중...")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

    ocrBootstrapper_.SetPreferredEngine(selectedEngineType_);
    ocrEngine_ = ocrBootstrapper_.CreateAndInitialize(selectedEngineType_);

    if (!ocrEngine_) {
        qCritical() << "[AppBackend] OCR 엔진 초기화 실패!";
        emit logMessage(QString("[%1] 오류: OCR 엔진 초기화 실패")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return;
    }

    emit logMessage(QString("[%1] OCR 엔진 선택: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(QString::fromStdString(ocrEngine_->GetEngineName())));

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

void AppBackend::ApplyRoiToOcrThread() {
    if (!ocrThread_) {
        return;
    }

    if (selectedRoi_.width <= 0 || selectedRoi_.height <= 0) {
        ocrThread_->ClearCropRegion();
        return;
    }

    ocrThread_->SetCropRegion(selectedRoi_);
}

void AppBackend::DispatchSentenceForTokenization(const QString& text) {
    if (!tokenizer_) {
        emit logMessage(QString("[%1] 토크나이저가 초기화되지 않았습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return;
    }

    sentenceAssembler_.MarkSentenceInFlight(text);

    QPointer<AppBackend> self(this);
    auto futurePtr = std::make_shared<std::future<void>>();

    {
        std::lock_guard<std::mutex> guard(tokenizationFuturesMutex_);
        tokenizationFutures_.push_back(futurePtr);
    }

    *futurePtr = std::async(std::launch::async, [this, self, text, futurePtr]() mutable {
        std::vector<tokenizer::Token> tokens;
        {
            std::lock_guard<std::mutex> lock(tokenizerMutex_);
            if (tokenizer_) {
                tokens = tokenizer_->Tokenize(text.toStdString());
            }
        }

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self, [self, text, tokens = std::move(tokens), futurePtr]() mutable {
            if (!self) {
                return;
            }
            self->HandleTokensReady(text, std::move(tokens));

            std::lock_guard<std::mutex> guard(self->tokenizationFuturesMutex_);
            auto it = std::find(self->tokenizationFutures_.begin(), self->tokenizationFutures_.end(), futurePtr);
            if (it != self->tokenizationFutures_.end()) {
                self->tokenizationFutures_.erase(it);
            }
        }, Qt::QueuedConnection);
    });
}

void AppBackend::HandleTokensReady(const QString& text, std::vector<tokenizer::Token>&& tokens) {
    sentenceAssembler_.ClearSentenceInFlight(text);

    if (text.isEmpty()) {
        return;
    }

    if (tokens.empty()) {
        emit logMessage(QString("[%1] 토큰화 결과가 비어 있습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return;
    }

    auto qmlTokens = ConvertTokensToVariant(tokens);

    sentenceAssembler_.MarkSentencePublished(text);

    {
        std::lock_guard<std::mutex> lock(sentencesMutex_);
        sentences_.push_back(text.toStdString());
    }

    emit sentenceDetected(text, qmlTokens);
    emit logMessage(QString("[%1] 문장 감지: %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(text));
}

QVariantList AppBackend::ConvertTokensToVariant(const std::vector<tokenizer::Token>& tokens) const {
    QVariantList qmlTokens;
    qmlTokens.reserve(tokens.size());

    for (const auto& token : tokens) {
        QVariantMap tokenMap;
        tokenMap["surface"] = QString::fromStdString(token.surface);
        tokenMap["reading"] = QString::fromStdString(token.reading);
        tokenMap["baseForm"] = QString::fromStdString(token.baseForm);
        tokenMap["partOfSpeech"] = QString::fromStdString(token.partOfSpeech);
        qmlTokens.append(tokenMap);
    }

    return qmlTokens;
}

QPixmap AppBackend::CaptureWindowPreview() const {
    if (!selectedWindow_ || !IsWindow(selectedWindow_)) {
        return QPixmap();
    }

    RECT rect;
    if (!GetWindowRect(selectedWindow_, &rect)) {
        return QPixmap();
    }

    const int centerX = static_cast<int>((rect.left + rect.right) / 2);
    const int centerY = static_cast<int>((rect.top + rect.bottom) / 2);
    QScreen* screen = QGuiApplication::screenAt(QPoint(centerX, centerY));
    if (!screen) {
        const auto screens = QGuiApplication::screens();
        if (screens.isEmpty()) {
            return QPixmap();
        }
        screen = screens.front();
    }

    QPixmap pixmap = screen->grabWindow(reinterpret_cast<WId>(selectedWindow_));
    if (pixmap.isNull()) {
        return pixmap;
    }

    RECT clientRect;
    if (!GetClientRect(selectedWindow_, &clientRect)) {
        return pixmap;
    }

    POINT clientTopLeft{0, 0};
    if (!ClientToScreen(selectedWindow_, &clientTopLeft)) {
        return pixmap;
    }

    const int clientWidth = std::max(1L, clientRect.right - clientRect.left);
    const int clientHeight = std::max(1L, clientRect.bottom - clientRect.top);
    const int offsetX = clientTopLeft.x - rect.left;
    const int offsetY = clientTopLeft.y - rect.top;

    QImage image = pixmap.toImage();
    QRect cropRect(offsetX, offsetY, clientWidth, clientHeight);
    QRect imageRect = image.rect();
    QRect safeRect = cropRect.intersected(imageRect);

    if (!safeRect.isEmpty()) {
        image = image.copy(safeRect);
    }

    return QPixmap::fromImage(image);
}

} // namespace ui
} // namespace toriyomi
