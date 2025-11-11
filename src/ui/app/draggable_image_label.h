// ToriYomi - 드래그 가능한 이미지 레이블
#pragma once

#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPoint>
#include <QRect>

namespace toriyomi {
namespace ui {

/**
 * @brief 마우스 드래그로 영역 선택이 가능한 QLabel
 */
class DraggableImageLabel : public QLabel {
    Q_OBJECT

public:
    explicit DraggableImageLabel(QWidget* parent = nullptr);
    ~DraggableImageLabel() override = default;

    QRect GetSelection() const;
    bool HasSelection() const { return hasSelection_; }
    void ClearSelection();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QPoint startPoint_;
    QPoint endPoint_;
    bool isDragging_;
    bool hasSelection_;
};

}  // namespace ui
}  // namespace toriyomi
