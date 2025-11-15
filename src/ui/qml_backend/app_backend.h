#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <QSize>
#include <QPixmap>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

#include "core/capture/frame_queue.h"
#include "core/capture/capture_thread.h"
#include "core/ocr/ocr_engine_bootstrapper.h"
#include "core/ocr/ocr_thread.h"
#include "core/tokenizer/japanese_tokenizer.h"
#include "ui/qml_backend/process_enumerator.h"
#include "ui/qml_backend/sentence_assembler.h"
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
    Q_PROPERTY(QString previewImageData READ GetPreviewImageData NOTIFY previewImageDataChanged)
    Q_PROPERTY(QSize previewImageSize READ GetPreviewImageSize NOTIFY previewImageDataChanged)
    Q_PROPERTY(double captureIntervalSeconds READ GetCaptureIntervalSeconds WRITE setCaptureIntervalSeconds NOTIFY captureIntervalSecondsChanged)
    Q_PROPERTY(int ocrEngineType READ GetOcrEngineType WRITE setOcrEngineType NOTIFY ocrEngineTypeChanged)

public:
    explicit AppBackend(QObject* parent = nullptr);
    ~AppBackend();

    // Q_PROPERTY getters
    QStringList GetProcessList() const { return processList_; }
    bool GetIsCapturing() const { return isCapturing_; }
    QString GetStatusMessage() const { return statusMessage_; }
    QString GetPreviewImageData() const { return previewImageData_; }
    QSize GetPreviewImageSize() const { return previewImageSize_; }
    double GetCaptureIntervalSeconds() const { return captureIntervalSeconds_; }
    int GetOcrEngineType() const { return static_cast<int>(selectedEngineType_); }

public slots:
    // UI에서 호출하는 메서드들 (Qt slots는 camelCase 관례 따름)
    void refreshProcessList();
    void selectProcess(int index);
    void selectRoi(int x, int y, int width, int height);
    void startCapture();
    void stopCapture();
    void requestShutdown();
    void clearSentences();
    Q_INVOKABLE void refreshPreviewImage();
    Q_INVOKABLE void setCaptureIntervalSeconds(double seconds);
    Q_INVOKABLE QString saveCurrentRoiSnapshot();
    Q_INVOKABLE void runSampleOcr(const QString& imagePath);
    Q_INVOKABLE void setOcrEngineType(int engineType);

signals:
    // QML로 보내는 시그널들
    void processListChanged();
    void isCapturingChanged();
    void statusMessageChanged();
    void logMessage(const QString& message);
    void sentenceDetected(const QString& originalText, const QVariantList& tokens);
    void previewImageDataChanged();
    void captureIntervalSecondsChanged();
    void ocrEngineTypeChanged();

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
    QPixmap CaptureWindowPreview() const;
    void ApplyRoiToOcrThread();
    void DispatchSentenceForTokenization(const QString& text);
    void HandleTokensReady(const QString& text, std::vector<tokenizer::Token>&& tokens);
    QVariantList ConvertTokensToVariant(const std::vector<tokenizer::Token>& tokens) const;

    // UI 상태
    QStringList processList_;
    std::vector<HWND> processWindows_;
    bool isCapturing_ = false;
    QString statusMessage_;
    QString previewImageData_;
    QSize previewImageSize_;
    double captureIntervalSeconds_ = 1.0;
    
    // 선택된 윈도우 및 ROI
    HWND selectedWindow_ = nullptr;
    cv::Rect selectedRoi_;
    bool hasRoiSelection_ = false;

    // 파이프라인 컴포넌트
    std::shared_ptr<toriyomi::FrameQueue> frameQueue_;
    std::unique_ptr<capture::CaptureThread> captureThread_;
    std::unique_ptr<ocr::OcrThread> ocrThread_;
    std::shared_ptr<ocr::IOcrEngine> ocrEngine_;  // shared: OcrThread와 생명주기 공유
    std::unique_ptr<tokenizer::JapaneseTokenizer> tokenizer_;
    std::unique_ptr<OverlayThread> overlayThread_;

    ocr::OcrEngineBootstrapper ocrBootstrapper_;
    ocr::OcrEngineType selectedEngineType_ = ocr::OcrEngineType::PaddleOCR;

    // OCR 결과 폴링 타이머
    QTimer* pollTimer_ = nullptr;

    // 문장 리스트
    std::vector<std::string> sentences_;
    std::mutex sentencesMutex_;
    SentenceAssembler sentenceAssembler_;

    std::mutex tokenizerMutex_;
    std::vector<std::shared_ptr<std::future<void>>> tokenizationFutures_;
    std::mutex tokenizationFuturesMutex_;

    std::vector<std::shared_ptr<std::future<void>>> cleanupFutures_;
    std::mutex cleanupFuturesMutex_;
    std::atomic_int cleanupTasksInFlight_{0};
    std::atomic_bool shutdownRequested_{false};
};

} // namespace ui
} // namespace toriyomi
