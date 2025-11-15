#pragma once

#include "core/ocr/ocr_engine.h"
#include <QString>
#include <functional>
#include <optional>
#include <vector>

namespace toriyomi {
namespace ui {

class SentenceAssembler {
public:
    void SetCaptureIntervalSeconds(double seconds);
    void Reset();

    std::optional<QString> TryAssemble(const std::vector<ocr::TextSegment>& segments,
                                       const std::function<void(const QString&)>& logCallback);

    void MarkSentenceInFlight(const QString& text);
    void ClearSentenceInFlight(const QString& text);
    void MarkSentencePublished(const QString& text);

private:
    using SegmentList = std::vector<ocr::TextSegment>;

    struct NormalizedSegment {
        QString text;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        float confidence = 0.0f;
    };

    std::vector<NormalizedSegment> NormalizeSegments(const SegmentList& segments) const;
    bool LooksLikeRuby(const NormalizedSegment& candidate,
                       const std::vector<NormalizedSegment>& references,
                       int avgHeight) const;

    std::optional<QString> BuildLines(std::vector<NormalizedSegment>& segments,
                                      const std::function<void(const QString&)>& logCallback);

    int RequiredStableFrames() const;

    double captureIntervalSeconds_ = 1.0;
    QString pendingSentence_;
    int pendingHits_ = 0;
    QString lastPublishedSentence_;
    QString sentenceInFlight_;
};

}  // namespace ui
}  // namespace toriyomi
