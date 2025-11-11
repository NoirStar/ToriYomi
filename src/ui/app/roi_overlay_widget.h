// ToriYomi - ROI 선택 오버레이 위젯
#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QMouseEvent>
#include <QPaintEvent>

namespace toriyomi {
namespace ui {

/**
 * @brief ROI 선택을 위한 투명 오버레이 위젯
 * 
 * QLabel 위에 겹쳐서 배치되어 마우스 드래그로 영역 선택
 */
class RoiOverlayWidget : public QWidget {
    Q_OBJECT

public:
    explicit RoiOverlayWidget(QWidget* parent = nullptr);
    ~RoiOverlayWidget() override = default;

    /**
     * @brief 선택 영역 가져오기
     */
    QRect GetSelection() const;

    /**
     * @brief 선택 여부 확인
     */
    bool HasSelection() const { return hasSelection_; }

    /**
     * @brief 선택 초기화
     */
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
