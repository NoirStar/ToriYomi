// ToriYomi - ROI 선택 오버레이 위젯 구현
#include "roi_overlay_widget.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <algorithm>

namespace toriyomi {
namespace ui {

RoiOverlayWidget::RoiOverlayWidget(QWidget* parent)
    : QWidget(parent)
    , isDragging_(false)
    , hasSelection_(false) {
    
    // 투명 배경 설정
    setAttribute(Qt::WA_TranslucentBackground);
    
    // 마우스 이벤트 명시적으로 활성화
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // 위젯이 입력을 받도록 설정
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

QRect RoiOverlayWidget::GetSelection() const {
    QPoint topLeft(
        std::min(startPoint_.x(), endPoint_.x()),
        std::min(startPoint_.y(), endPoint_.y())
    );
    QPoint bottomRight(
        std::max(startPoint_.x(), endPoint_.x()),
        std::max(startPoint_.y(), endPoint_.y())
    );
    return QRect(topLeft, bottomRight);
}

void RoiOverlayWidget::ClearSelection() {
    hasSelection_ = false;
    isDragging_ = false;
    update();
}

void RoiOverlayWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        startPoint_ = event->pos();
        endPoint_ = startPoint_;
        isDragging_ = true;
        hasSelection_ = false;
        qDebug() << "Mouse Press:" << startPoint_;
        update();
        event->accept();
    }
}

void RoiOverlayWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging_) {
        endPoint_ = event->pos();
        
        // 위젯 범위 내로 제한
        endPoint_.setX(std::max(0, std::min(endPoint_.x(), width() - 1)));
        endPoint_.setY(std::max(0, std::min(endPoint_.y(), height() - 1)));
        
        qDebug() << "Mouse Move:" << endPoint_;
        update();
        event->accept();
    }
}

void RoiOverlayWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && isDragging_) {
        endPoint_ = event->pos();
        
        // 위젯 범위 내로 제한
        endPoint_.setX(std::max(0, std::min(endPoint_.x(), width() - 1)));
        endPoint_.setY(std::max(0, std::min(endPoint_.y(), height() - 1)));
        
        isDragging_ = false;
        
        // 최소 크기 체크 (10x10 픽셀)
        QRect selection = GetSelection();
        if (selection.width() > 10 && selection.height() > 10) {
            hasSelection_ = true;
        }
        
        update();
    }
}

void RoiOverlayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    
    if (!isDragging_ && !hasSelection_) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect selectionRect = GetSelection();
    
    // 반투명 배경
    painter.setBrush(QColor(13, 115, 119, 80));
    painter.setPen(QPen(QColor(20, 160, 133), 2, Qt::SolidLine));
    painter.drawRect(selectionRect);
    
    // 선택 완료 시 테두리 굵게
    if (hasSelection_ && !isDragging_) {
        painter.setPen(QPen(QColor(20, 160, 133), 3, Qt::SolidLine));
        painter.drawRect(selectionRect);
    }
}

}  // namespace ui
}  // namespace toriyomi
