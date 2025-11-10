// ToriYomi - GDI 화면 캡처
// GDI BitBlt를 사용한 화면 캡처 (DXGI 폴백용)

#pragma once

#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <memory>

namespace toriyomi::capture {

/**
 * @brief GDI BitBlt 기반 화면 캡처 (DXGI 폴백용)
 * 
 * DXGI가 지원되지 않는 환경에서 사용되는 폴백 캡처 방식입니다.
 * BitBlt를 사용하여 화면을 메모리 DC에 복사한 후 OpenCV Mat으로 변환합니다.
 * 
 * DXGI보다 느리지만 (약 10-30 FPS), 모든 Windows 환경에서 작동합니다.
 * 
 * @note Windows GDI API 사용 (Windows 7 이상)
 */
class GdiCapture {
public:
    GdiCapture();
    ~GdiCapture();

    // 복사 불가
    GdiCapture(const GdiCapture&) = delete;
    GdiCapture& operator=(const GdiCapture&) = delete;

    /**
     * @brief 대상 윈도우에 대한 GDI 캡처 초기화
     * 
     * Device Context (DC)와 호환 비트맵을 생성합니다.
     * 
     * @param targetWindow 캡처할 윈도우 핸들
     * @return 초기화 성공 시 true, 실패 시 false
     */
    bool Initialize(HWND targetWindow);

    /**
     * @brief 대상으로부터 단일 프레임 캡처
     * 
     * BitBlt를 사용하여 화면을 메모리 DC에 복사하고
     * OpenCV Mat으로 변환합니다.
     * 
     * DXGI와 달리 항상 즉시 반환됩니다 (타임아웃 없음).
     * 
     * @return BGR 프레임이 담긴 cv::Mat, 실패 시 빈 Mat
     */
    cv::Mat CaptureFrame();

    /**
     * @brief 모든 GDI 리소스 해제
     * 
     * Device Context, 비트맵 등을 정리합니다.
     */
    void Shutdown();

    /**
     * @brief GDI 캡처가 현재 초기화되었는지 확인
     * 
     * @return 초기화되어 캡처 준비가 완료된 경우 true
     */
    bool IsInitialized() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace toriyomi::capture
