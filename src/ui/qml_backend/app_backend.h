#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/capture/frame_queue.h"
#include "core/capture/capture_thread.h"
#include "core/ocr/ocr_thread.h"
#include "core/ocr/tesseract_wrapper.h"
#include "core/tokenizer/japanese_tokenizer.h"
#include "ui/overlay/overlay_window.h"
#include "ui/overlay/overlay_thread.h"

namespace toriyomi {
namespace ui {

/**
 * @brief QML UI를 위한 C++ 백엔드 클래스
 * 
 * 캡처 → OCR → 토큰화 → 오버레이 파이프라인을 관리하고
 * QML과 시그널/슬롯으로 통신합니다.
 */
class AppBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList processList READ GetProcessList NOTIFY processListChanged)
    Q_PROPERTY(bool isCapturing READ GetIsCapturing NOTIFY isCapturingChanged)
    Q_PROPERTY(QString statusMessage READ GetStatusMessage NOTIFY statusMessageChanged)

public:
    explicit AppBackend(QObject* parent = nullptr);
    ~AppBackend();

    // Q_PROPERTY getters
    QStringList GetProcessList() const { return processList_; }
    bool GetIsCapturing() const { return isCapturing_; }
    QString GetStatusMessage() const { return statusMessage_; }

public slots:
    // UI에서 호출하는 메서드들 (Qt slots는 camelCase 관례 따름)
    void refreshProcessList();
    void selectProcess(int index);
    void selectRoi(int x, int y, int width, int height);
    void startCapture();
    void stopCapture();
    void requestShutdown();
    void clearSentences();

signals:
    // QML로 보내는 시그널들
    void processListChanged();
    void isCapturingChanged();
    void statusMessageChanged();
    void logMessage(const QString& message);
    void sentenceDetected(const QString& originalText, const QVariantList& tokens);

private slots:
    void OnPollOcrResults();

private:
    void InitializeEngines();
    struct CleanupSummary {
        bool overlayStopped = false;
        bool ocrStopped = false;
        bool captureStopped = false;
    };

    struct CleanupResources {
        std::unique_ptr<OverlayThread> overlay;
        std::unique_ptr<ocr::OcrThread> ocr;
        std::unique_ptr<capture::CaptureThread> capture;
        std::shared_ptr<toriyomi::FrameQueue> frameQueue;
    };

    CleanupSummary CleanupThreads(const std::shared_ptr<CleanupResources>& resources);
    bool HasActiveWorkers() const;
    bool HasCleanupInFlight() const;
    void HandleCleanupFinished(const CleanupSummary& summary,
                               const std::shared_ptr<std::future<void>>& futureRef);
    void SetStatusMessage(const QString& message);

    // UI 상태
    QStringList processList_;
    std::vector<HWND> processWindows_;
    bool isCapturing_ = false;
    QString statusMessage_;
    
    // 선택된 윈도우 및 ROI
    HWND selectedWindow_ = nullptr;
    cv::Rect selectedRoi_;
    bool hasRoiSelection_ = false;

    // 파이프라인 컴포넌트
    std::shared_ptr<toriyomi::FrameQueue> frameQueue_;
    std::unique_ptr<capture::CaptureThread> captureThread_;
    std::unique_ptr<ocr::OcrThread> ocrThread_;
    std::shared_ptr<ocr::TesseractWrapper> ocrEngine_;  // shared: OcrThread와 생명주기 공유
    std::unique_ptr<tokenizer::JapaneseTokenizer> tokenizer_;
    std::unique_ptr<OverlayThread> overlayThread_;

    // OCR 결과 폴링 타이머
    QTimer* pollTimer_ = nullptr;

    // 문장 리스트
    std::vector<std::string> sentences_;
    std::mutex sentencesMutex_;

    std::vector<std::shared_ptr<std::future<void>>> cleanupFutures_;
    std::mutex cleanupFuturesMutex_;
    std::atomic_int cleanupTasksInFlight_{0};
    std::atomic_bool shutdownRequested_{false};
};

} // namespace ui
} // namespace toriyomi
