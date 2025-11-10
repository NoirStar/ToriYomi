// ToriYomi - DXGI 화면 캡처 구현
// DirectX 11 Desktop Duplication API를 사용한 고성능 화면 캡처

#include "core/capture/dxgi_capture.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace toriyomi::capture {

// Pimpl 패턴으로 DXGI 세부 구현 숨김
struct DxgiCapture::Impl {
    HWND targetWindow{nullptr};
    bool initialized{false};

    // DirectX/DXGI 인터페이스
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    ComPtr<IDXGIOutputDuplication> deskDupl;
    ComPtr<ID3D11Texture2D> stagingTexture;

    // 화면 정보
    DXGI_OUTDUPL_DESC outputDuplDesc{};
    D3D11_TEXTURE2D_DESC textureDesc{};

    bool InitializeD3D();
    bool InitializeDuplication();
    bool CreateStagingTexture();
    cv::Mat ConvertTextureToMat(ID3D11Texture2D* texture);
};

DxgiCapture::DxgiCapture() : pImpl_(std::make_unique<Impl>()) {
}

DxgiCapture::~DxgiCapture() {
    Shutdown();
}

bool DxgiCapture::Initialize(HWND targetWindow) {
    if (!targetWindow || targetWindow == INVALID_HANDLE_VALUE) {
        return false;
    }

    // 윈도우 유효성 검사
    if (!IsWindow(targetWindow)) {
        return false;
    }

    // 이미 초기화된 경우 먼저 정리
    if (pImpl_->initialized) {
        Shutdown();
    }

    pImpl_->targetWindow = targetWindow;

    // D3D11 디바이스 초기화
    if (!pImpl_->InitializeD3D()) {
        return false;
    }

    // Desktop Duplication 초기화
    if (!pImpl_->InitializeDuplication()) {
        Shutdown();
        return false;
    }

    // 스테이징 텍스처 생성 (CPU 접근용)
    if (!pImpl_->CreateStagingTexture()) {
        Shutdown();
        return false;
    }

    pImpl_->initialized = true;
    return true;
}

cv::Mat DxgiCapture::CaptureFrame() {
    if (!pImpl_->initialized) {
        return cv::Mat();
    }

    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo{};

    // 새 프레임 획득 (100ms 타임아웃)
    HRESULT hr = pImpl_->deskDupl->AcquireNextFrame(
        100, // 타임아웃 (ms)
        &frameInfo,
        &desktopResource
    );

    if (FAILED(hr)) {
        // 타임아웃이나 에러 - 빈 Mat 반환
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // 타임아웃은 정상 (화면 변경 없음)
            return cv::Mat();
        }
        
        // 접근 손실 등의 에러 - 재초기화 필요
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            pImpl_->deskDupl.Reset();
            pImpl_->initialized = false;
        }
        return cv::Mat();
    }

    // 프레임을 텍스처로 변환
    ComPtr<ID3D11Texture2D> acquiredTexture;
    hr = desktopResource.As(&acquiredTexture);
    
    if (FAILED(hr)) {
        pImpl_->deskDupl->ReleaseFrame();
        return cv::Mat();
    }

    // 스테이징 텍스처로 복사
    pImpl_->d3dContext->CopyResource(pImpl_->stagingTexture.Get(), acquiredTexture.Get());

    // OpenCV Mat으로 변환
    cv::Mat frame = pImpl_->ConvertTextureToMat(pImpl_->stagingTexture.Get());

    // 프레임 해제
    pImpl_->deskDupl->ReleaseFrame();

    return frame;
}

void DxgiCapture::Shutdown() {
    if (pImpl_) {
        pImpl_->stagingTexture.Reset();
        pImpl_->deskDupl.Reset();
        pImpl_->d3dContext.Reset();
        pImpl_->d3dDevice.Reset();
        pImpl_->initialized = false;
        pImpl_->targetWindow = nullptr;
    }
}

bool DxgiCapture::IsInitialized() const {
    return pImpl_ && pImpl_->initialized;
}

// === Impl 메서드 구현 ===

bool DxgiCapture::Impl::InitializeD3D() {
    // D3D11 디바이스 생성
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // 기본 어댑터
        D3D_DRIVER_TYPE_HARDWARE,   // 하드웨어 가속
        nullptr,                    // 소프트웨어 래스터라이저 없음
        0,                          // 플래그
        nullptr,                    // 기능 레벨 배열
        0,                          // 기능 레벨 수
        D3D11_SDK_VERSION,
        &d3dDevice,
        &featureLevel,
        &d3dContext
    );

    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool DxgiCapture::Impl::InitializeDuplication() {
    // DXGI 디바이스 가져오기
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        return false;
    }

    // DXGI 어댑터 가져오기
    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) {
        return false;
    }

    // 첫 번째 출력 가져오기 (주 모니터)
    ComPtr<IDXGIOutput> dxgiOutput;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        return false;
    }

    // IDXGIOutput1로 캐스팅
    ComPtr<IDXGIOutput1> dxgiOutput1;
    hr = dxgiOutput.As(&dxgiOutput1);
    if (FAILED(hr)) {
        return false;
    }

    // Desktop Duplication 생성
    hr = dxgiOutput1->DuplicateOutput(d3dDevice.Get(), &deskDupl);
    if (FAILED(hr)) {
        return false;
    }

    // 출력 설명 가져오기
    deskDupl->GetDesc(&outputDuplDesc);

    return true;
}

bool DxgiCapture::Impl::CreateStagingTexture() {
    // 스테이징 텍스처 설명 설정
    textureDesc.Width = outputDuplDesc.ModeDesc.Width;
    textureDesc.Height = outputDuplDesc.ModeDesc.Height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = outputDuplDesc.ModeDesc.Format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.BindFlags = 0;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    textureDesc.MiscFlags = 0;

    // 스테이징 텍스처 생성
    HRESULT hr = d3dDevice->CreateTexture2D(&textureDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

cv::Mat DxgiCapture::Impl::ConvertTextureToMat(ID3D11Texture2D* texture) {
    // 텍스처 매핑
    D3D11_MAPPED_SUBRESOURCE mappedResource{};
    HRESULT hr = d3dContext->Map(
        texture,
        0,
        D3D11_MAP_READ,
        0,
        &mappedResource
    );

    if (FAILED(hr)) {
        return cv::Mat();
    }

    // OpenCV Mat 생성 (BGRA -> BGR 변환)
    int width = static_cast<int>(textureDesc.Width);
    int height = static_cast<int>(textureDesc.Height);
    
    // DXGI_FORMAT_B8G8R8A8_UNORM 가정
    cv::Mat bgraFrame(height, width, CV_8UC4, mappedResource.pData, mappedResource.RowPitch);
    cv::Mat bgrFrame;
    cv::cvtColor(bgraFrame, bgrFrame, cv::COLOR_BGRA2BGR);

    // 깊은 복사 (언매핑 전에)
    cv::Mat result = bgrFrame.clone();

    // 텍스처 언매핑
    d3dContext->Unmap(texture, 0);

    return result;
}

} // namespace toriyomi::capture
