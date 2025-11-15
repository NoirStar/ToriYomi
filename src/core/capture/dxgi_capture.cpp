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
    ComPtr<IDXGIAdapter> adapterForWindow;
    ComPtr<IDXGIOutput> outputForWindow;

    // 화면 정보
    DXGI_OUTDUPL_DESC outputDuplDesc{};
    D3D11_TEXTURE2D_DESC textureDesc{};

    bool SelectOutputForWindow();
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

    if (!pImpl_->SelectOutputForWindow()) {
        return false;
    }

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
        pImpl_->adapterForWindow.Reset();
        pImpl_->outputForWindow.Reset();
        pImpl_->initialized = false;
        pImpl_->targetWindow = nullptr;
    }
}

bool DxgiCapture::IsInitialized() const {
    return pImpl_ && pImpl_->initialized;
}

// === Impl 메서드 구현 ===

bool DxgiCapture::Impl::InitializeD3D() {
    D3D_FEATURE_LEVEL featureLevel;
    IDXGIAdapter* adapterPtr = adapterForWindow.Get();
    const D3D_DRIVER_TYPE driverType = adapterPtr ? D3D_DRIVER_TYPE_UNKNOWN
                                                  : D3D_DRIVER_TYPE_HARDWARE;

    HRESULT hr = D3D11CreateDevice(
        adapterPtr,
        driverType,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &d3dDevice,
        &featureLevel,
        &d3dContext
    );

    return SUCCEEDED(hr);
}

bool DxgiCapture::Impl::InitializeDuplication() {
    ComPtr<IDXGIOutput> dxgiOutput = outputForWindow;
    ComPtr<IDXGIAdapter> adapter = adapterForWindow;

    if (!dxgiOutput) {
        ComPtr<IDXGIDevice> dxgiDevice;
        HRESULT hr = d3dDevice.As(&dxgiDevice);
        if (FAILED(hr)) {
            return false;
        }

        hr = dxgiDevice->GetAdapter(&adapter);
        if (FAILED(hr)) {
            return false;
        }

        hr = adapter->EnumOutputs(0, &dxgiOutput);
        if (FAILED(hr)) {
            return false;
        }
    }

    ComPtr<IDXGIOutput1> dxgiOutput1;
    HRESULT hr = dxgiOutput.As(&dxgiOutput1);
    if (FAILED(hr)) {
        return false;
    }

    hr = dxgiOutput1->DuplicateOutput(d3dDevice.Get(), &deskDupl);
    if (FAILED(hr)) {
        return false;
    }

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

bool DxgiCapture::Impl::SelectOutputForWindow() {
    HMONITOR monitor = MonitorFromWindow(targetWindow, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return false;
    }

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    for (UINT adapterIndex = 0;; ++adapterIndex) {
        ComPtr<IDXGIAdapter> adapter;
        hr = factory->EnumAdapters(adapterIndex, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            return false;
        }

        for (UINT outputIndex = 0;; ++outputIndex) {
            ComPtr<IDXGIOutput> output;
            hr = adapter->EnumOutputs(outputIndex, &output);
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr)) {
                return false;
            }

            DXGI_OUTPUT_DESC desc;
            if (FAILED(output->GetDesc(&desc))) {
                continue;
            }

            if (desc.Monitor == monitor) {
                adapterForWindow = adapter;
                outputForWindow = output;
                return true;
            }
        }
    }

    return false;
}

} // namespace toriyomi::capture
