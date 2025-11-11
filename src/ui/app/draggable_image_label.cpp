// ToriYomi - 드래그 가능한 이미지 레이블 구현
#include "draggable_image_label.h"
#include <QPainter>
#include <QPen>
#include <QMessageBox>
#include <algorithm>

namespace toriyomi {
namespace ui {

DraggableImageLabel::DraggableImageLabel(QWidget* parent)
    : QLabel(parent)
    , isDragging_(false)
    , hasSelection_(false) {
    
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

QRect DraggableImageLabel::GetSelection() const {
    if (!hasSelection_) {
        return QRect();
    }
    
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

void DraggableImageLabel::ClearSelection() {
    hasSelection_ = false;
    isDragging_ = false;
    update();
}

void DraggableImageLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        startPoint_ = event->pos();
        endPoint_ = startPoint_;
        isDragging_ = true;
        hasSelection_ = false;
        update();
    }
    QLabel::mousePressEvent(event);
}

void DraggableImageLabel::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging_) {
        endPoint_ = event->pos();
        
        // 위젯 범위 내로 제한
        endPoint_.setX(std::max(0, std::min(endPoint_.x(), width() - 1)));
        endPoint_.setY(std::max(0, std::min(endPoint_.y(), height() - 1)));
        
        update();
    }
    QLabel::mouseMoveEvent(event);
}

void DraggableImageLabel::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && isDragging_) {
        endPoint_ = event->pos();
        
        // 위젯 범위 내로 제한
        endPoint_.setX(std::max(0, std::min(endPoint_.x(), width() - 1)));
        endPoint_.setY(std::max(0, std::min(endPoint_.y(), height() - 1)));
        
        isDragging_ = false;
        
        // 최소 크기 체크
        int w = std::abs(endPoint_.x() - startPoint_.x());
        int h = std::abs(endPoint_.y() - startPoint_.y());
        
        if (w > 10 && h > 10) {
            hasSelection_ = true;
        }
        
        update();
    }
    QLabel::mouseReleaseEvent(event);
}

void DraggableImageLabel::paintEvent(QPaintEvent* event) {
    // 먼저 이미지 그리기
    QLabel::paintEvent(event);
    
    // 선택 영역이 있으면 위에 그리기
    if (isDragging_ || hasSelection_) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QPoint topLeft(
            std::min(startPoint_.x(), endPoint_.x()),
            std::min(startPoint_.y(), endPoint_.y())
        );
        QPoint bottomRight(
            std::max(startPoint_.x(), endPoint_.x()),
            std::max(startPoint_.y(), endPoint_.y())
        );
        
        QRect selectionRect(topLeft, bottomRight);
        
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
}

}  // namespace ui
}  // namespace toriyomi
