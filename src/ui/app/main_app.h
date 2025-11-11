// ToriYomi - Qt 메인 애플리케이션
#pragma once

#include "interactive_sentence_widget.h"
#include <QMainWindow>
#include <QLayout>
#include <Windows.h>
#include <memory>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

// Forward declaration for AUTOUIC generated class
namespace Ui {
    class MainWindow;
}

namespace toriyomi {

namespace tokenizer {
    struct Token;
}

namespace ui {

/**
 * @brief ToriYomi 메인 애플리케이션 윈도우
 * 
 * app.ui 파일을 빌드타임에 컴파일하여 UI를 구성하고,
 * OCR 결과를 표시하며 사전 검색과 Anki 연동을 제공합니다.
 */
class MainApp : public QMainWindow {
    Q_OBJECT

public:
    explicit MainApp(QWidget* parent = nullptr);
    ~MainApp() override;

    /**
     * @brief 문장 추가 (OCR 결과 + 토큰)
     * @param tokens 형태소 분석된 토큰
     * @param originalText 원본 문장
     */
    void AddSentence(const std::vector<tokenizer::Token>& tokens,
                     const std::string& originalText);

    /**
     * @brief 문장 리스트 초기화
     */
    void ClearSentences();

    /**
     * @brief 상태바 메시지 업데이트
     * @param message 표시할 메시지
     */
    void UpdateStatus(const QString& message);

    /**
     * @brief FPS 표시 업데이트
     * @param fps 초당 프레임 수
     */
    void UpdateFps(double fps);

    /**
     * @brief 선택된 ROI 영역 가져오기
     */
    cv::Rect GetSelectedRoi() const { return selectedRoi_; }

    /**
     * @brief ROI가 선택되었는지 확인
     */
    bool HasRoiSelection() const { return hasRoiSelection_; }

private slots:
    /**
     * @brief 단어 클릭 핸들러 (인터랙티브 위젯에서)
     */
    void OnWordClicked(const QString& surface, const QString& reading,
                      const QString& baseForm);

    /**
     * @brief Anki 버튼 클릭 핸들러
     */
    void OnAnkiButtonClicked();

    /**
     * @brief 프로세스 선택 변경 핸들러
     * @param index 선택된 인덱스
     */
    void OnProcessSelected(int index);

    /**
     * @brief ROI 선택 버튼 클릭 핸들러
     */
    void OnSelectRoiClicked();

private:
    /**
     * @brief UI 초기화
     */
    void SetupUi();

    /**
     * @brief 시그널 연결
     */
    void ConnectSignals();

    /**
     * @brief 실행 중인 프로세스 목록 로드
     */
    void LoadProcessList();

    /**
     * @brief 선택된 프로세스 캡처
     * @return 캡처된 이미지
     */
    cv::Mat CaptureSelectedProcess();

    /**
     * @brief 사전 패널에 단어 정보 표시
     * @param word 단어
     * @param reading 읽기
     * @param baseForm 기본형
     */
    void ShowDictionaryEntry(const std::string& word, 
                            const std::string& reading,
                            const std::string& baseForm);

private:
    Ui::MainWindow* ui_;  // AUTOUIC가 생성한 UI 클래스
    
    InteractiveSentenceWidget* sentenceWidget_;  // 인터랙티브 문장 위젯

    // 데이터
    std::string currentWord_;                    // 현재 선택된 단어
    std::string currentReading_;                 // 현재 단어 읽기
    std::vector<std::string> sentences_;         // 문장 목록
    
    std::vector<HWND> processWindows_;           // 프로세스 윈도우 핸들 목록
    HWND selectedWindow_;                        // 선택된 윈도우
    
    cv::Rect selectedRoi_;                       // 선택된 ROI 영역
    bool hasRoiSelection_;                       // ROI 선택 여부
};

}  // namespace ui
}  // namespace toriyomi
