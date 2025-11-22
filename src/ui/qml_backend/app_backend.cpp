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
#include <exception>
#include <chrono>
#include <thread>
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
#include <opencv2/imgproc.hpp>

#include "core/capture/dxgi_capture.h"
#include "core/capture/gdi_capture.h"

namespace {

bool IsFrameNearlyBlack(const cv::Mat& frame) {
    if (frame.empty()) {
        return true;
    }

    cv::Scalar meanScalar;
    cv::Scalar stddevScalar;
    cv::meanStdDev(frame, meanScalar, stddevScalar);
    const double maxMean = std::max({meanScalar[0], meanScalar[1], meanScalar[2]});
    const double maxStdDev = std::max({stddevScalar[0], stddevScalar[1], stddevScalar[2]});
    return maxMean < 2.5 && maxStdDev < 1.5;
}

bool IsPixmapNearlyBlack(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        return true;
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return true;
    }

    const int width = image.width();
    const int height = image.height();
    if (width == 0 || height == 0) {
        return true;
    }

    const int sampleStepX = std::max(1, width / 64);
    const int sampleStepY = std::max(1, height / 64);
    double accum = 0.0;
    double accumStd = 0.0;
    int samples = 0;

    for (int y = 0; y < height; y += sampleStepY) {
        const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < width; x += sampleStepX) {
            const QRgb pixel = row[x];
            const double intensity = (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3.0;
            accum += intensity;
            accumStd += intensity * intensity;
            ++samples;
        }
    }

    if (samples == 0) {
        return true;
    }

    const double mean = accum / samples;
    const double variance = std::max(0.0, (accumStd / samples) - (mean * mean));
    const double stddev = std::sqrt(variance);
    return mean < 2.5 && stddev < 1.5;
}

QString CurrentTimestamp() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

toriyomi::ocr::OcrBootstrapConfig BuildDefaultOcrConfig() {
    toriyomi::ocr::OcrBootstrapConfig config;
    const QDir baseDir(QCoreApplication::applicationDirPath());
    const QString paddleModels = QDir::cleanPath(baseDir.filePath("models/paddleocr"));
    config.paddleModelDirectory = QDir::toNativeSeparators(paddleModels).toStdString();
    config.paddleLanguage = "jpn";
    return config;
}

QString OcrEngineNameForDisplay(toriyomi::ocr::OcrEngineType type) {
    switch (type) {
        case toriyomi::ocr::OcrEngineType::PaddleOCR:
            return QStringLiteral("PaddleOCR");
        case toriyomi::ocr::OcrEngineType::EasyOCR:
            return QStringLiteral("EasyOCR");
        default:
            return QStringLiteral("Unknown");
    }
}

constexpr int kPreferredWindowMinWidth = 320;
constexpr int kPreferredWindowMinHeight = 200;
constexpr int kPreferredWindowMinArea = kPreferredWindowMinWidth * kPreferredWindowMinHeight;

bool QueryWindowArea(HWND hwnd, int* width, int* height) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }

    const LONG w = rect.right - rect.left;
    const LONG h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (width) {
        *width = static_cast<int>(w);
    }
    if (height) {
        *height = static_cast<int>(h);
    }
    return true;
}

cv::Mat CropFrameToClientArea(HWND target, const cv::Mat& frame) {
    if (!target || !IsWindow(target) || frame.empty()) {
        return cv::Mat();
    }

    RECT clientRect{};
    if (!GetClientRect(target, &clientRect)) {
        return cv::Mat();
    }

    POINT clientTopLeft{0, 0};
    if (!ClientToScreen(target, &clientTopLeft)) {
        return cv::Mat();
    }

    LONG monitorLeft = 0;
    LONG monitorTop = 0;
    HMONITOR monitor = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
    if (monitor) {
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfo(monitor, &monitorInfo)) {
            monitorLeft = monitorInfo.rcMonitor.left;
            monitorTop = monitorInfo.rcMonitor.top;
        }
    }

    const int width = std::max(1L, clientRect.right - clientRect.left);
    const int height = std::max(1L, clientRect.bottom - clientRect.top);
    const int relativeX = clientTopLeft.x - static_cast<int>(monitorLeft);
    const int relativeY = clientTopLeft.y - static_cast<int>(monitorTop);

    cv::Rect desired(relativeX, relativeY, width, height);
    cv::Rect frameRect(0, 0, frame.cols, frame.rows);
    cv::Rect safe = desired & frameRect;

    if (safe.width <= 0 || safe.height <= 0) {
        return cv::Mat();
    }

    return frame(safe).clone();
}

void ForceWindowRefresh(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    const UINT redrawFlags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME;
    RedrawWindow(hwnd, nullptr, nullptr, redrawFlags);
    SendMessageTimeout(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 50, nullptr);
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

    // 기본 전체 화면 캡처 옵션 추가
    processList_.append(QStringLiteral("전체 화면 캡처 (Desktop)"));
    processWindows_.push_back(GetDesktopWindow());

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

    HWND candidate = processWindows_[index];
    if (!candidate || !IsWindow(candidate)) {
        qWarning() << "[AppBackend] 유효하지 않은 윈도우 핸들, 인덱스:" << index;
        emit logMessage(QString("[%1] 경고: 선택한 윈도우 핸들을 사용할 수 없습니다")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return;
    }

    HWND resolvedWindow = ResolvePreferredWindow(candidate);
    if (resolvedWindow != candidate) {
        processWindows_[index] = resolvedWindow;
        candidate = resolvedWindow;
        emit logMessage(QString("[%1] 참고: 더 큰 윈도우를 자동으로 선택했습니다 (원래 선택은 보조 창으로 추정)")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }

    selectedWindow_ = candidate;
    hasRoiSelection_ = false;

    if (!previewImageData_.isEmpty() || previewImageSize_.isValid()) {
        previewImageData_.clear();
        previewImageSize_ = QSize();
        emit previewImageDataChanged();
    }
    refreshPreviewImage();
    
    wchar_t title[256] = {0};
    GetWindowTextW(selectedWindow_, title, 256);
    DWORD pid = 0;
    GetWindowThreadProcessId(selectedWindow_, &pid);

    const QString windowLabel = QString::fromWCharArray(title);
    const QString hwndHex = QString::number(reinterpret_cast<qulonglong>(selectedWindow_), 16).toUpper();

    int windowWidth = 0;
    int windowHeight = 0;
    QueryWindowArea(selectedWindow_, &windowWidth, &windowHeight);

    qDebug() << "[AppBackend] 프로세스 선택:" << processList_[index]
             << "PID=" << pid << "HWND=0x" << hwndHex
             << "크기=" << windowWidth << "x" << windowHeight;
    emit logMessage(QString("[%1] 프로세스 선택: %2 (PID=%3, HWND=0x%4, %5x%6)")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(!windowLabel.isEmpty() ? windowLabel : processList_[index])
        .arg(pid)
        .arg(hwndHex)
        .arg(windowWidth)
        .arg(windowHeight));
}

HWND AppBackend::ResolvePreferredWindow(HWND candidate) const {
    if (!candidate || !IsWindow(candidate)) {
        return candidate;
    }

    int width = 0;
    int height = 0;
    if (!QueryWindowArea(candidate, &width, &height)) {
        return candidate;
    }

    const int area = width * height;
    if (area >= kPreferredWindowMinArea) {
        return candidate;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(candidate, &pid);
    if (!pid) {
        return candidate;
    }

    struct SearchContext {
        DWORD pid;
        HWND best;
        int bestArea;
        int minArea;
    } context{pid, candidate, area, kPreferredWindowMinArea};

    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto* ctx = reinterpret_cast<SearchContext*>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != ctx->pid || hwnd == ctx->best) {
            return TRUE;
        }

        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
            return TRUE;
        }

        if (GetWindow(hwnd, GW_OWNER) != nullptr) {
            return TRUE;
        }

        RECT rect{};
        if (!GetWindowRect(hwnd, &rect)) {
            return TRUE;
        }

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) {
            return TRUE;
        }

        const int area = width * height;
        if (area >= ctx->minArea && area > ctx->bestArea) {
            ctx->best = hwnd;
            ctx->bestArea = area;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    return context.best;
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
        selectedWindow_ = GetDesktopWindow();
        hasRoiSelection_ = false;
        emit logMessage(QString("[%1] 기본 전체 화면 캡처 모드 활성화")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }

    // ROI 미선택 시 전체 윈도우 크기로 설정
    if (!hasRoiSelection_) {
        RECT rect;
        bool roiResolved = false;
        if (GetClientRect(selectedWindow_, &rect)) {
            selectedRoi_ = cv::Rect(0, 0, rect.right - rect.left, rect.bottom - rect.top);
            roiResolved = true;
        } else if (selectedWindow_ == GetDesktopWindow()) {
            selectedRoi_ = cv::Rect(0, 0,
                                    GetSystemMetrics(SM_CXSCREEN),
                                    GetSystemMetrics(SM_CYSCREEN));
            roiResolved = true;
        }

        if (!roiResolved) {
            SetStatusMessage("윈도우 크기를 가져올 수 없습니다");
            emit logMessage(QString("[%1] 오류: 윈도우 크기 조회 실패")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            return;
        }

        hasRoiSelection_ = true;
        ApplyRoiToOcrThread();
        
        qDebug() << "[AppBackend] ROI 미선택 - 전체 윈도우 캡처:" 
                 << selectedRoi_.width << "x" << selectedRoi_.height;
        emit logMessage(QString("[%1] ROI 미선택 - 전체 윈도우 캡처 (%2x%3)")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(selectedRoi_.width)
            .arg(selectedRoi_.height));
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

    if (captureThread_) {
        const auto stats = captureThread_->GetStatistics();
        if (stats.windowOccluded != lastCaptureOccluded_) {
            lastCaptureOccluded_ = stats.windowOccluded;
            if (stats.windowOccluded) {
                emit logMessage(QString("[%1] 경고: 선택한 창이 다른 창에 가려져 정확한 화면을 캡처할 수 없습니다. 창을 화면 맨 앞으로 이동하거나 오버레이를 최소화해주세요.")
                                 .arg(CurrentTimestamp()));
                SetStatusMessage("창이 가려져 있습니다");
            } else {
                emit logMessage(QString("[%1] 안내: 선택한 창이 다시 보이는 상태입니다.")
                                 .arg(CurrentTimestamp()));
                SetStatusMessage("캡처 중...");
            }
        }
    } else {
        lastCaptureOccluded_ = false;
    }

    const auto results = ocrThread_->GetLatestResults();
    auto logHook = [this](const QString& message) {
        emit logMessage(message);
    };

    auto assembled = sentenceAssembler_.TryAssemble(results, logHook);
    if (!assembled.has_value()) {
        return;
    }

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

    QPointer<AppBackend> self(this);
    auto futurePtr = std::make_shared<std::future<void>>();

    sentenceAssembler_.MarkSentenceInFlight(text);

    auto task = [this, self, text, futurePtr]() mutable {
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
    };

    try {
        *futurePtr = std::async(std::launch::async, std::move(task));
    } catch (const std::exception& ex) {
        sentenceAssembler_.ClearSentenceInFlight(text);
        emit logMessage(QString("[%1] 토큰화 작업 시작 실패: %2")
            .arg(CurrentTimestamp())
            .arg(QString::fromLocal8Bit(ex.what())));
        return;
    } catch (...) {
        sentenceAssembler_.ClearSentenceInFlight(text);
        emit logMessage(QString("[%1] 토큰화 작업 시작 실패: 알 수 없는 오류")
            .arg(CurrentTimestamp()));
        return;
    }

    {
        std::lock_guard<std::mutex> guard(tokenizationFuturesMutex_);
        tokenizationFutures_.push_back(futurePtr);
    }
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
    auto convertToPixmap = [](const cv::Mat& frame) -> QPixmap {
        if (frame.empty()) {
            return QPixmap();
        }
        cv::Mat rgb;
        if (frame.channels() == 3) {
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        } else if (frame.channels() == 4) {
            cv::cvtColor(frame, rgb, cv::COLOR_BGRA2RGB);
        } else {
            cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
        }
        QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        return QPixmap::fromImage(image.copy());
    };

    HWND target = selectedWindow_ ? selectedWindow_ : GetDesktopWindow();
    if (!target || !IsWindow(target)) {
        return QPixmap();
    }

    auto buildPixmapFromFrame = [&](cv::Mat frame, const char* tag) -> QPixmap {
        if (frame.empty()) {
            return QPixmap();
        }
        cv::Mat cropped = CropFrameToClientArea(target, frame);
        if (!cropped.empty()) {
            frame = std::move(cropped);
        }
        if (IsFrameNearlyBlack(frame)) {
            qWarning() << "[AppBackend] 프리뷰 프레임이 거의 검정이라" << tag << "경로를 폐기합니다";
            return QPixmap();
        }
        QPixmap pixmap = convertToPixmap(frame);
        if (IsPixmapNearlyBlack(pixmap)) {
            qWarning() << "[AppBackend] 프리뷰 픽스맵이 거의 검정이라" << tag << "경로를 폐기합니다";
            return QPixmap();
        }
        return pixmap;
    };

    // DXGI 기반 프리뷰 우선 시도 (GPU 가속 창 대응)
    capture::DxgiCapture dxgiPreview;
    if (dxgiPreview.Initialize(target)) {
        const int kMaxDxgiAttempts = 3;
        for (int attempt = 0; attempt < kMaxDxgiAttempts; ++attempt) {
            ForceWindowRefresh(target);
            cv::Mat frame = dxgiPreview.CaptureFrame();
            if (!frame.empty()) {
                QPixmap pix = buildPixmapFromFrame(std::move(frame), "DXGI");
                if (!pix.isNull()) {
                    dxgiPreview.Shutdown();
                    return pix;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(35));
        }
        dxgiPreview.Shutdown();
    }

    // DXGI 실패 시 GDI 폴백
    capture::GdiCapture previewCapture;
    previewCapture.SetPreferPrintWindow(target != GetDesktopWindow());
    if (previewCapture.Initialize(target)) {
        ForceWindowRefresh(target);
        cv::Mat frame = previewCapture.CaptureFrame();
        previewCapture.Shutdown();
        QPixmap pix = buildPixmapFromFrame(std::move(frame), "GDI");
        if (!pix.isNull()) {
            return pix;
        }
    }

    // 모든 캡처 실패 시 QScreen 경로로 최종 폴백
    RECT rect;
    if (!GetWindowRect(target, &rect)) {
        if (target == GetDesktopWindow()) {
            rect.left = 0;
            rect.top = 0;
            rect.right = GetSystemMetrics(SM_CXSCREEN);
            rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        } else {
            return QPixmap();
        }
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

    ForceWindowRefresh(target);

    QPixmap pixmap;
    if (target == GetDesktopWindow()) {
        pixmap = screen->grabWindow(0);
    } else {
        pixmap = screen->grabWindow(reinterpret_cast<WId>(target));
    }
    if (pixmap.isNull()) {
        return pixmap;
    }

    if (IsPixmapNearlyBlack(pixmap)) {
        qWarning() << "[AppBackend] QScreen 기반 캡처도 거의 검정입니다";
        return pixmap;
    }

    RECT clientRect;
    if (!GetClientRect(target, &clientRect)) {
        if (target == GetDesktopWindow()) {
            clientRect.left = 0;
            clientRect.top = 0;
            clientRect.right = GetSystemMetrics(SM_CXSCREEN);
            clientRect.bottom = GetSystemMetrics(SM_CYSCREEN);
        } else {
            return pixmap;
        }
    }

    POINT clientTopLeft{0, 0};
    if (!ClientToScreen(target, &clientTopLeft)) {
        if (target == GetDesktopWindow()) {
            clientTopLeft.x = 0;
            clientTopLeft.y = 0;
        } else {
            return pixmap;
        }
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
