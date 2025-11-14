// ToriYomi - 투명 오버레이 윈도우 구현
#include "overlay_window.h"
#include <windowsx.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace toriyomi {
namespace ui {

// GDI+ 초기화용 전역 토큰
static ULONG_PTR gdiplusToken = 0;

// 윈도우 클래스 이름
static const wchar_t* WINDOW_CLASS_NAME = L"ToriYomiOverlay";

OverlayWindow::OverlayWindow()
    : hwnd_(nullptr)
    , hInstance_(GetModuleHandle(nullptr))
    , isCreated_(false) {
    // GDI+ 초기화
    if (gdiplusToken == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    }
}

OverlayWindow::~OverlayWindow() {
    Destroy();
    
    // GDI+ 종료 (마지막 인스턴스)
    if (gdiplusToken != 0) {
        GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
}

bool OverlayWindow::Create(int x, int y, int width, int height) {
    if (isCreated_) {
        return true;  // 이미 생성됨
    }

    // 윈도우 클래스 등록
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // 윈도우 생성 (레이어드 + 최상위 + 클릭 투과)
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,    // 확장 스타일
        WINDOW_CLASS_NAME,                                                       // 클래스 이름
        L"ToriYomi Overlay",                                                     // 윈도우 제목
        WS_POPUP,                                                                // 윈도우 스타일 (테두리 없음)
        x, y, width, height,                                                     // 위치와 크기
        nullptr,                                                                 // 부모 윈도우 없음
        nullptr,                                                                 // 메뉴 없음
        hInstance_,
        this                                                                     // 생성 파라미터 (this 포인터 전달)
    );

    if (!hwnd_) {
        return false;
    }

    // 완전 투명 배경 설정 (알파 0)
    SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);

    // 윈도우 표시
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    isCreated_ = true;
    return true;
}

void OverlayWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    isCreated_ = false;
}

bool OverlayWindow::IsCreated() const {
    return isCreated_ && hwnd_ != nullptr;
}

void OverlayWindow::UpdateFurigana(const std::vector<tokenizer::FuriganaInfo>& furiganaList) {
    std::lock_guard<std::mutex> lock(furiganaMutex_);
    furigana_ = furiganaList;
}

void OverlayWindow::Redraw() {
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

bool OverlayWindow::ProcessMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* window = nullptr;

    if (msg == WM_CREATE) {
        // 생성 시 this 포인터 저장
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        // 저장된 this 포인터 가져오기
        window = reinterpret_cast<OverlayWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_DESTROY:
            return 0;

        default:
            return DefWindowProc(hwnd_, msg, wParam, lParam);
    }
}

void OverlayWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);

    // 메모리 DC 생성 (더블 버퍼링)
    RECT rect;
    GetClientRect(hwnd_, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // 투명 배경으로 초기화
    HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memDC, &rect, clearBrush);
    DeleteObject(clearBrush);

    // 후리가나 렌더링
    RenderFurigana(memDC);

    // 레이어드 윈도우 업데이트 (알파 블렌딩)
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    
    UpdateLayeredWindow(hwnd_, hdc, nullptr, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    // 정리
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hwnd_, &ps);
}

void OverlayWindow::RenderFurigana(HDC hdc) {
    std::lock_guard<std::mutex> lock(furiganaMutex_);

    if (furigana_.empty()) {
        return;
    }

    // GDI+ Graphics 객체 생성
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // 폰트 설정 (Yu Gothic, 10pt)
    FontFamily fontFamily(L"Yu Gothic");
    Font font(&fontFamily, 10, FontStyleRegular, UnitPoint);

    // 텍스트 브러시 (검은색)
    SolidBrush textBrush(Color(255, 0, 0, 0));

    // 외곽선 펜 (흰색, 두께 1.5)
    Pen outlinePen(Color(255, 255, 255, 255), 1.5f);
    outlinePen.SetLineJoin(LineJoinRound);

    // 문자열 포맷 (중앙 정렬)
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);

    // 각 후리가나 렌더링 (한자만)
    for (const auto& info : furigana_) {
        if (!info.needsRuby) {
            continue;  // 한자 없으면 스킵
        }

        // UTF-8 → UTF-16 변환
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, info.reading.c_str(), -1, nullptr, 0);
        if (wideLen <= 0) continue;

        std::wstring wideReading(wideLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, info.reading.c_str(), -1, &wideReading[0], wideLen);

        // 렌더링 위치 (루비 위치)
        RectF textRect(
            static_cast<REAL>(info.rubyPosition.x),
            static_cast<REAL>(info.rubyPosition.y),
            static_cast<REAL>(info.position.width),
            static_cast<REAL>(info.position.height)
        );

        // 외곽선 렌더링 (Path 사용)
        GraphicsPath path;
        path.AddString(
            wideReading.c_str(),
            -1,
            &fontFamily,
            FontStyleRegular,
            graphics.GetDpiY() * 10 / 72.0f,  // pt → px 변환
            textRect,
            &format
        );

        graphics.DrawPath(&outlinePen, &path);   // 흰색 외곽선
        graphics.FillPath(&textBrush, &path);    // 검은색 채우기
    }
}

}  // namespace ui
}  // namespace toriyomi
