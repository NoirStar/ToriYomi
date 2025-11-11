// ToriYomi - 인터랙티브 문장 리스트 위젯 구현
#include "interactive_sentence_widget.h"
#include "core/tokenizer/japanese_tokenizer.h"
#include <QUrl>
#include <QString>
#include <QTextCursor>
#include <QScrollBar>

namespace toriyomi {
namespace ui {

InteractiveSentenceWidget::InteractiveSentenceWidget(QWidget* parent)
    : QTextBrowser(parent)
    , sentenceCount_(0) {
    
    // 링크 클릭 활성화
    setOpenLinks(false);  // 커스텀 핸들러 사용
    connect(this, &QTextBrowser::anchorClicked, this, &InteractiveSentenceWidget::OnLinkClicked);
    
    // 스타일 설정
    setStyleSheet(R"(
        QTextBrowser {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #3c3c3c;
            border-radius: 5px;
            padding: 10px;
            font-size: 16px;
            font-family: "Yu Gothic", "Meiryo", "MS Gothic";
        }
        
        QTextBrowser a {
            color: #ffffff;
            text-decoration: none;
        }
        
        QTextBrowser a:hover {
            color: #14a085;
            text-decoration: underline;
        }
    )");
}

void InteractiveSentenceWidget::AddSentence(
    const std::vector<tokenizer::Token>& tokens,
    const std::string& originalText) {
    
    sentenceCount_++;
    
    // HTML 생성
    QString html = QString("<div style='margin: 10px 0; padding: 8px; "
                          "background-color: #2b2b2b; border-radius: 5px;'>");
    
    // 문장 번호
    html += QString("<span style='color: #0d7377; font-weight: bold; font-size: 14px;'>"
                   "[%1] </span>").arg(sentenceCount_);
    
    // 토큰별 링크 생성
    html += TokensToHtml(tokens);
    
    html += "</div>";
    
    // 기존 내용에 추가
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(html);
    
    // 스크롤을 맨 아래로
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void InteractiveSentenceWidget::Clear() {
    clear();
    sentenceCount_ = 0;
}

QString InteractiveSentenceWidget::TokensToHtml(
    const std::vector<tokenizer::Token>& tokens) const {
    
    QString html;
    
    for (const auto& token : tokens) {
        // URL 인코딩 (word://surface/reading/baseForm)
        QString surface = QString::fromUtf8(token.surface.c_str());
        QString reading = QString::fromUtf8(token.reading.c_str());
        QString baseForm = QString::fromUtf8(token.baseForm.c_str());
        
        // URL 생성 (슬래시를 %2F로 인코딩)
        QString url = QString("word://%1/%2/%3")
            .arg(QString(surface.toUtf8().toBase64()))
            .arg(QString(reading.toUtf8().toBase64()))
            .arg(QString(baseForm.toUtf8().toBase64()));
        
        // 한자가 포함된 단어만 링크로
        bool hasKanji = false;
        for (QChar c : surface) {
            if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF) {
                hasKanji = true;
                break;
            }
        }
        
        if (hasKanji) {
            // 클릭 가능한 링크
            html += QString("<a href='%1' style='font-size: 18px;'>%2</a>")
                .arg(url)
                .arg(surface);
        } else {
            // 일반 텍스트
            html += QString("<span style='font-size: 18px;'>%1</span>")
                .arg(surface);
        }
    }
    
    return html;
}

void InteractiveSentenceWidget::OnLinkClicked(const QUrl& url) {
    if (url.scheme() != "word") {
        return;
    }
    
    // URL 파싱: word://surface/reading/baseForm
    QString path = url.path();
    QStringList parts = path.split('/');
    
    if (parts.size() >= 3) {
        // Base64 디코딩
        QString surface = QString::fromUtf8(QByteArray::fromBase64(parts[0].toUtf8()));
        QString reading = QString::fromUtf8(QByteArray::fromBase64(parts[1].toUtf8()));
        QString baseForm = QString::fromUtf8(QByteArray::fromBase64(parts[2].toUtf8()));
        
        // 시그널 발생 (QString 직접 전달)
        emit WordClicked(surface, reading, baseForm);
    }
}

}  // namespace ui
}  // namespace toriyomi
