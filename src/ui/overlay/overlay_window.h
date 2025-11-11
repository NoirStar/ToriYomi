// ToriYomi - 투명 오버레이 윈도우 (게임 화면 위 후리가나 표시)
#pragma once

#include "core/tokenizer/furigana_mapper.h"
#include <Windows.h>
#include <vector>
#include <mutex>
#include <memory>

namespace toriyomi {
namespace ui {

/**
 * @brief 게임 화면 위에 투명한 오버레이 윈도우를 생성하고 후리가나를 렌더링
 * 
 * Win32 레이어드 윈도우를 사용하여 투명 배경에 후리가나만 표시합니다.
 * 클릭 이벤트는 하위 윈도우(게임)로 통과됩니다.
 */
class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    // 복사/이동 금지 (윈도우 핸들 관리)
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    /**
     * @brief 오버레이 윈도우 생성 및 표시
     * @param x 윈도우 X 좌표
     * @param y 윈도우 Y 좌표
     * @param width 윈도우 너비
     * @param height 윈도우 높이
     * @return 성공 시 true
     */
    bool Create(int x, int y, int width, int height);

    /**
     * @brief 윈도우 파괴
     */
    void Destroy();

    /**
     * @brief 윈도우가 생성되었는지 확인
     */
    bool IsCreated() const;

    /**
     * @brief 후리가나 데이터 업데이트 (스레드 안전)
     * @param furiganaList 렌더링할 후리가나 목록
     */
    void UpdateFurigana(const std::vector<tokenizer::FuriganaInfo>& furiganaList);

    /**
     * @brief 윈도우 다시 그리기
     */
    void Redraw();

    /**
     * @brief 윈도우 메시지 처리
     * @return 계속 실행 시 true, 종료 시 false
     */
    bool ProcessMessages();

    /**
     * @brief 윈도우 핸들 가져오기
     */
    HWND GetHandle() const { return hwnd_; }

private:
    /**
     * @brief 윈도우 프로시저 (정적 멤버)
     */
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief 인스턴스별 메시지 핸들러
     */
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief WM_PAINT 처리 - 후리가나 렌더링
     */
    void OnPaint();

    /**
     * @brief 후리가나 텍스트 렌더링 (GDI+)
     */
    void RenderFurigana(HDC hdc);

private:
    HWND hwnd_;                                      // 윈도우 핸들
    HINSTANCE hInstance_;                            // 인스턴스 핸들
    std::vector<tokenizer::FuriganaInfo> furigana_;  // 렌더링할 후리가나 데이터
    std::mutex furiganaMutex_;                       // 후리가나 데이터 동기화
    bool isCreated_;                                 // 생성 여부
};

}  // namespace ui
}  // namespace toriyomi
