// ToriYomi - 인터랙티브 문장 리스트 위젯
#pragma once

#include <QTextBrowser>
#include <QWidget>
#include <string>
#include <vector>

namespace toriyomi {
namespace tokenizer {
    struct Token;  // Forward declaration
}

namespace ui {

/**
 * @brief 호버 가능한 일본어 문장 리스트 위젯
 * 
 * 각 단어에 마우스를 올리면 밑줄이 표시되고,
 * 클릭하면 사전 패널에 단어 정보를 표시합니다.
 */
class InteractiveSentenceWidget : public QTextBrowser {
    Q_OBJECT

public:
    explicit InteractiveSentenceWidget(QWidget* parent = nullptr);
    ~InteractiveSentenceWidget() override = default;

    /**
     * @brief 문장 추가 (토큰 정보 포함)
     * @param tokens 형태소 분석된 토큰 목록
     * @param originalText 원본 문장
     */
    void AddSentence(const std::vector<tokenizer::Token>& tokens, 
                     const std::string& originalText);

    /**
     * @brief 문장 리스트 초기화
     */
    void Clear();

signals:
    /**
     * @brief 단어 클릭 시그널
     * @param surface 단어 표면형
     * @param reading 읽기
     * @param baseForm 기본형
     */
    void WordClicked(const QString& surface, 
                     const QString& reading,
                     const QString& baseForm);

private slots:
    /**
     * @brief 링크 클릭 핸들러
     * @param url 클릭된 링크 (word://surface/reading/baseForm)
     */
    void OnLinkClicked(const QUrl& url);

private:
    /**
     * @brief 토큰을 HTML로 변환
     * @param tokens 토큰 목록
     * @return HTML 문자열
     */
    QString TokensToHtml(const std::vector<tokenizer::Token>& tokens) const;

private:
    int sentenceCount_;  // 문장 카운트 (번호 표시용)
};

}  // namespace ui
}  // namespace toriyomi
