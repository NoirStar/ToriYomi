// ToriYomi - ROI 선택 다이얼로그 구현
#include "roi_selector_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <algorithm>

namespace toriyomi {
namespace ui {

RoiSelectorDialog::RoiSelectorDialog(const cv::Mat& screenshot, QWidget* parent)
    : QDialog(parent)
    , screenshot_(screenshot.clone())
    , hasSelection_(false)
    , imageLabel_(nullptr) {
    
    setWindowTitle("캡처 영역 선택 - 마우스로 드래그하세요");
    setModal(true);

    // OpenCV Mat → QPixmap 변환
    pixmap_ = MatToPixmap(screenshot_);

    // 메인 레이아웃
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    // 드래그 가능한 이미지 레이블
    imageLabel_ = new DraggableImageLabel(this);
    imageLabel_->setScaledContents(false);  // 원본 크기 유지
    imageLabel_->setStyleSheet("QLabel { border: 2px solid #0d7377; }");
    
    // 이미지 크기 조정 (최대 800x600, 비율 유지)
    int maxWidth = 800;
    int maxHeight = 550;
    
    double scale = std::min(
        static_cast<double>(maxWidth) / pixmap_.width(),
        static_cast<double>(maxHeight) / pixmap_.height()
    );
    
    // 스케일이 1보다 작을 때만 축소
    if (scale < 1.0) {
        int scaledWidth = static_cast<int>(pixmap_.width() * scale);
        int scaledHeight = static_cast<int>(pixmap_.height() * scale);
        pixmap_ = pixmap_.scaled(scaledWidth, scaledHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    imageLabel_->setPixmap(pixmap_);
    imageLabel_->setFixedSize(pixmap_.size());  // 레이블 크기 고정
    layout->addWidget(imageLabel_, 0, Qt::AlignCenter);

    // 버튼 레이아웃
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* okButton = new QPushButton("확인", this);
    QPushButton* cancelButton = new QPushButton("취소", this);
    
    connect(okButton, &QPushButton::clicked, this, [this]() {
        if (imageLabel_->HasSelection()) {
            QRect screenRect = imageLabel_->GetSelection();
            
            // 화면 좌표 → 이미지 좌표 변환
            QPoint imgTopLeft = ScreenToImage(screenRect.topLeft());
            QPoint imgBottomRight = ScreenToImage(screenRect.bottomRight());
            
            int width = imgBottomRight.x() - imgTopLeft.x();
            int height = imgBottomRight.y() - imgTopLeft.y();
            
            if (width > 10 && height > 10) {
                selectedRoi_ = cv::Rect(imgTopLeft.x(), imgTopLeft.y(), width, height);
                
                // 이미지 범위 내로 제한
                selectedRoi_.x = std::max(0, selectedRoi_.x);
                selectedRoi_.y = std::max(0, selectedRoi_.y);
                selectedRoi_.width = std::min(screenshot_.cols - selectedRoi_.x, selectedRoi_.width);
                selectedRoi_.height = std::min(screenshot_.rows - selectedRoi_.y, selectedRoi_.height);
                
                hasSelection_ = true;
                accept();
            } else {
                QMessageBox::warning(this, "경고", "영역이 너무 작습니다. (최소 10x10)");
            }
        } else {
            QMessageBox::warning(this, "경고", "영역을 선택해주세요.");
        }
    });
    
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    layout->addLayout(buttonLayout);

    // 창 크기를 내용에 맞춤
    adjustSize();
    
    // 크기 조정 불가능하게
    setFixedSize(size());
}

QPixmap RoiSelectorDialog::MatToPixmap(const cv::Mat& mat) {
    cv::Mat rgb;
    
    if (mat.channels() == 1) {
        cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
    } else if (mat.channels() == 3) {
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    } else if (mat.channels() == 4) {
        cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
    } else {
        rgb = mat;
    }

    return QPixmap::fromImage(
        QImage(rgb.data, rgb.cols, rgb.rows, 
               static_cast<int>(rgb.step), 
               rgb.channels() == 3 ? QImage::Format_RGB888 : QImage::Format_RGBA8888)
    );
}

QPoint RoiSelectorDialog::ScreenToImage(const QPoint& screenPos) const {
    if (!imageLabel_) {
        return QPoint(0, 0);
    }

    // pixmap 크기 대비 원본 이미지 크기 비율 계산
    double scaleX = static_cast<double>(screenshot_.cols) / pixmap_.width();
    double scaleY = static_cast<double>(screenshot_.rows) / pixmap_.height();

    return QPoint(
        static_cast<int>(screenPos.x() * scaleX),
        static_cast<int>(screenPos.y() * scaleY)
    );
}

}  // namespace ui
}  // namespace toriyomi
