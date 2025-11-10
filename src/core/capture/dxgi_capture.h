// ToriYomi - DXGI 화면 캡처
// DirectX 11 Desktop Duplication API 래퍼로 효율적인 화면 캡처 제공

#pragma once

#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <memory>

namespace toriyomi::capture {

/**
 * @brief DirectX 11 Desktop Duplication API 기반 DXGI 화면 캡처
 * 
 * 대상 윈도우에 대한 고성능 화면 캡처를 제공합니다.
 * 최소 CPU 오버헤드로 30+ FPS를 달성합니다.
 * 
 * @note Windows 8+ 및 DirectX 11 지원 GPU 필요
 */
class DxgiCapture {
public:
    DxgiCapture();
    ~DxgiCapture();

    DxgiCapture(const DxgiCapture&) = delete;
    DxgiCapture& operator=(const DxgiCapture&) = delete;

    /**
     * @brief 대상 윈도우에 대한 DXGI 캡처 초기화
     * 
     * @param targetWindow 캡처할 윈도우 핸들
     * @return 초기화 성공 시 true, 실패 시 false
     */
    bool Initialize(HWND targetWindow);

    /**
     * @brief 대상으로부터 단일 프레임 캡처
     * 
     * 새 프레임이 사용 가능하거나 타임아웃(100ms)될 때까지 블로킹됩니다.
     * 
     * @return BGR 프레임이 담긴 cv::Mat, 실패/타임아웃 시 빈 Mat
     */
    cv::Mat CaptureFrame();

    /**
     * @brief 모든 DXGI 리소스 종료 및 해제
     */
    void Shutdown();

    /**
     * @brief DXGI 캡처가 현재 초기화되었는지 확인
     * 
     * @return 초기화되어 캡처 준비가 완료된 경우 true
     */
    bool IsInitialized() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace toriyomi::capture
