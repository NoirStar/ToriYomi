// ToriYomi - ROI (Region of Interest) 선택 다이얼로그
#pragma once

#include "draggable_image_label.h"
#include <QDialog>
#include <QPixmap>
#include <opencv2/opencv.hpp>

namespace toriyomi {
namespace ui {

/**
 * @brief 캡처 영역 선택 다이얼로그
 * 
 * 게임 화면의 스크린샷을 보여주고,
 * 마우스 드래그로 OCR할 영역을 선택합니다.
 */
class RoiSelectorDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 생성자
     * @param screenshot 선택할 스크린샷 이미지
     * @param parent 부모 위젯
     */
    explicit RoiSelectorDialog(const cv::Mat& screenshot, QWidget* parent = nullptr);
    ~RoiSelectorDialog() override = default;

    /**
     * @brief 선택된 ROI 영역 가져오기
     * @return 선택된 영역 (cv::Rect)
     */
    cv::Rect GetSelectedRoi() const { return selectedRoi_; }

    /**
     * @brief ROI가 선택되었는지 확인
     */
    bool HasSelection() const { return hasSelection_; }

private:
    /**
     * @brief OpenCV Mat를 QPixmap으로 변환
     */
    QPixmap MatToPixmap(const cv::Mat& mat);

    /**
     * @brief 화면 좌표를 이미지 좌표로 변환
     */
    QPoint ScreenToImage(const QPoint& screenPos) const;

private:
    cv::Mat screenshot_;            // 원본 스크린샷
    QPixmap pixmap_;                // Qt용 픽스맵
    
    bool hasSelection_;             // 선택 완료 여부
    
    cv::Rect selectedRoi_;          // 선택된 영역 (이미지 좌표)
    DraggableImageLabel* imageLabel_;  // 드래그 가능한 이미지 레이블
};

}  // namespace ui
}  // namespace toriyomi
