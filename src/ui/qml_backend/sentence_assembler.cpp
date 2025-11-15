#include "ui/qml_backend/sentence_assembler.h"

#include <QDateTime>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QStringList>
#include <utility>

namespace toriyomi {
namespace ui {

namespace {
constexpr float kMinConfidence = 60.0f;
constexpr int kMinArea = 400;
constexpr int kLineGapTolerance = 24;

bool IsKana(QChar ch) {
    const ushort code = ch.unicode();
    if (code >= 0x3040 && code <= 0x309F) {
        return true;
    }
    if (code >= 0x30A0 && code <= 0x30FF) {
        return true;
    }
    if (code >= 0xFF66 && code <= 0xFF9D) {
        return true;
    }
    return false;
}

bool ContainsCjk(const QString& text) {
    for (const QChar& ch : text) {
        if (ch.unicode() >= 0x2E80) {
            return true;
        }
    }
    return false;
}

bool NeedsSpace(const QString& left, const QString& right) {
    auto isAscii = [](const QString& text) {
        for (const QChar& ch : text) {
            if (ch.isSpace()) {
                continue;
            }
            if (ch.unicode() < 0x2E80) {
                return true;
            }
        }
        return false;
    };

    if (left.isEmpty() || right.isEmpty()) {
        return false;
    }

    const QChar leftLast = left.back();
    const QChar rightFirst = right.front();
    if (leftLast.isSpace() || leftLast.isPunct()) {
        return false;
    }
    if (rightFirst.isSpace() || rightFirst.isPunct()) {
        return false;
    }
    return isAscii(left) && isAscii(right);
}

}  // namespace

void SentenceAssembler::SetCaptureIntervalSeconds(double seconds) {
    captureIntervalSeconds_ = std::clamp(seconds, 0.1, 5.0);
}

void SentenceAssembler::Reset() {
    pendingSentence_.clear();
    pendingHits_ = 0;
    lastPublishedSentence_.clear();
    sentenceInFlight_.clear();
}

std::optional<QString> SentenceAssembler::TryAssemble(const SegmentList& segments,
                                                      const std::function<void(const QString&)>& logCallback) {
    if (segments.empty()) {
        pendingSentence_.clear();
        pendingHits_ = 0;
        return std::nullopt;
    }

    auto normalized = NormalizeSegments(segments);
    if (normalized.empty()) {
        pendingSentence_.clear();
        pendingHits_ = 0;
        return std::nullopt;
    }

    auto assembled = BuildLines(normalized, logCallback);
    if (!assembled || assembled->isEmpty()) {
        pendingSentence_.clear();
        pendingHits_ = 0;
        return std::nullopt;
    }

    const QString& combined = *assembled;
    if (combined == lastPublishedSentence_ || combined == sentenceInFlight_) {
        pendingSentence_.clear();
        pendingHits_ = 0;
        return std::nullopt;
    }

    if (combined == pendingSentence_) {
        pendingHits_++;
    } else {
        pendingSentence_ = combined;
        pendingHits_ = 1;
    }

    if (pendingHits_ < RequiredStableFrames()) {
        return std::nullopt;
    }

    pendingSentence_.clear();
    pendingHits_ = 0;
    return combined;
}

void SentenceAssembler::MarkSentenceInFlight(const QString& text) {
    sentenceInFlight_ = text;
}

void SentenceAssembler::ClearSentenceInFlight(const QString& text) {
    if (sentenceInFlight_ == text) {
        sentenceInFlight_.clear();
    }
}

void SentenceAssembler::MarkSentencePublished(const QString& text) {
    lastPublishedSentence_ = text;
}

int SentenceAssembler::RequiredStableFrames() const {
    return (captureIntervalSeconds_ <= 0.5) ? 1 : 2;
}

std::vector<SentenceAssembler::NormalizedSegment> SentenceAssembler::NormalizeSegments(const SegmentList& segments) const {
    std::vector<NormalizedSegment> result;
    result.reserve(segments.size());

    int widthSum = 0;
    int heightSum = 0;

    for (const auto& segment : segments) {
        if (segment.confidence < kMinConfidence) {
            continue;
        }

        const int area = segment.boundingBox.width * segment.boundingBox.height;
        if (area < kMinArea) {
            continue;
        }

        QString text = QString::fromStdString(segment.text).trimmed();
        if (text.isEmpty()) {
            continue;
        }

        NormalizedSegment normalized;
        normalized.text = text;
        normalized.x = segment.boundingBox.x;
        normalized.y = segment.boundingBox.y;
        normalized.width = segment.boundingBox.width;
        normalized.height = segment.boundingBox.height;
        normalized.confidence = segment.confidence;

        widthSum += normalized.width;
        heightSum += normalized.height;
        result.push_back(std::move(normalized));
    }

    if (result.empty()) {
        return result;
    }

    const int avgHeight = std::max(1, heightSum / static_cast<int>(result.size()));
    std::vector<NormalizedSegment> filtered;
    filtered.reserve(result.size());
    for (const auto& seg : result) {
        if (LooksLikeRuby(seg, result, avgHeight)) {
            continue;
        }
        filtered.push_back(seg);
    }

    return filtered;
}

bool SentenceAssembler::LooksLikeRuby(const NormalizedSegment& candidate,
                                      const std::vector<NormalizedSegment>& references,
                                      int avgHeight) const {
    if (candidate.text.isEmpty() || candidate.text.size() > 4) {
        return false;
    }

    if (candidate.height >= std::max(8, static_cast<int>(avgHeight * 0.6))) {
        return false;
    }

    for (const QChar& ch : candidate.text) {
        if (!IsKana(ch)) {
            return false;
        }
    }

    const int candidateBaseline = candidate.y + candidate.height;
    const int candidateCenterX = candidate.x + candidate.width / 2;

    auto overlapsHorizontally = [](const NormalizedSegment& a, const NormalizedSegment& b) {
        const int ax2 = a.x + a.width;
        const int bx2 = b.x + b.width;
        const int overlap = std::min(ax2, bx2) - std::max(a.x, b.x);
        return overlap > 0 && overlap >= std::min(a.width, b.width) * 0.4;
    };

    for (const auto& base : references) {
        if (&base == &candidate) {
            continue;
        }

        if (!overlapsHorizontally(candidate, base)) {
            continue;
        }

        const int baseBaseline = base.y + base.height;
        const bool sitsAboveBase = candidateBaseline <= baseBaseline - std::max(4, base.height / 3);
        const bool nearCenter = candidateCenterX >= base.x - base.width * 0.25 &&
                                candidateCenterX <= base.x + base.width * 1.25;
        if (sitsAboveBase && nearCenter) {
            return true;
        }
    }

    return false;
}

std::optional<QString> SentenceAssembler::BuildLines(std::vector<NormalizedSegment>& segments,
                                                     const std::function<void(const QString&)>& logCallback) {
    if (segments.empty()) {
        return std::nullopt;
    }

    int totalHeights = 0;
    for (const auto& seg : segments) {
        totalHeights += seg.height;
    }

    const int avgHeight = std::max(1, totalHeights / static_cast<int>(segments.size()));
    const int lineTolerance = std::max(kLineGapTolerance, static_cast<int>(avgHeight * 0.6));

    std::sort(segments.begin(), segments.end(), [](const NormalizedSegment& lhs, const NormalizedSegment& rhs) {
        const int lhsCenterY = lhs.y + lhs.height / 2;
        const int rhsCenterY = rhs.y + rhs.height / 2;
        if (lhsCenterY == rhsCenterY) {
            return lhs.x < rhs.x;
        }
        return lhsCenterY < rhsCenterY;
    });

    QStringList groupedLines;
    QString currentLine;
    int currentLineCenterY = std::numeric_limits<int>::min();

    auto appendSegment = [&](const NormalizedSegment& normalized) {
        if (currentLine.isEmpty()) {
            currentLine = normalized.text;
            currentLineCenterY = normalized.y + normalized.height / 2;
            return;
        }

        const int segmentCenterY = normalized.y + normalized.height / 2;
        if (std::abs(segmentCenterY - currentLineCenterY) > lineTolerance) {
            groupedLines.append(currentLine.trimmed());
            currentLine = normalized.text;
            currentLineCenterY = segmentCenterY;
            return;
        }

        if (NeedsSpace(currentLine, normalized.text)) {
            currentLine.append(' ');
        }
        currentLine.append(normalized.text);
        currentLineCenterY = (currentLineCenterY + segmentCenterY) / 2;
    };

    for (const auto& normalized : segments) {
        appendSegment(normalized);
    }

    if (!currentLine.isEmpty()) {
        groupedLines.append(currentLine.trimmed());
    }

    groupedLines.removeAll(QString());

    QStringList filtered;
    for (const auto& line : groupedLines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.size() <= 2 && !ContainsCjk(trimmed)) {
            continue;
        }
        filtered.append(trimmed);
    }

    if (filtered.isEmpty()) {
        return std::nullopt;
    }

    const QString combined = filtered.join('\n').trimmed();
    if (combined.size() <= 1 && segments.size() > 1 && logCallback) {
        QStringList debugEntries;
        debugEntries.reserve(static_cast<int>(segments.size()));
        for (const auto& seg : segments) {
            debugEntries.append(QStringLiteral("%1 (%2,%3 %4x%5 conf=%6)")
                                    .arg(seg.text)
                                    .arg(seg.x)
                                    .arg(seg.y)
                                    .arg(seg.width)
                                    .arg(seg.height)
                                    .arg(QString::number(seg.confidence, 'f', 1)));
        }

        logCallback(QStringLiteral("[%1] OCR 디버그 - %2개 세그먼트: %3")
                        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                        .arg(segments.size())
                        .arg(debugEntries.join("; ")));
    }

    return combined;
}

}  // namespace ui
}  // namespace toriyomi
