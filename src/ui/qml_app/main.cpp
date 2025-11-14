#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QDebug>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "ui/qml_backend/app_backend.h"

// Windows 콘솔 출력을 위한 헬퍼 함수
static void EnableConsoleOutput() {
#ifdef _WIN32
    // 이미 콘솔이 있는지 확인
    if (GetConsoleWindow() == nullptr) {
        // 새 콘솔 생성
        if (!AllocConsole()) {
            return;
        }
    }
    
    // UTF-8 코드 페이지 설정
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // stdout을 콘솔로 리다이렉트
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    
    // 버퍼링 비활성화
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
}

int main(int argc, char *argv[])
{
    // 콘솔 출력 활성화
    EnableConsoleOutput();
    
    printf("[INIT] ToriYomi 시작...\n");

    // Qt 메시지 핸들러를 먼저 설정 (QGuiApplication 생성 전)
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString &msg){
        const char* prefix = "";
        switch (type) {
            case QtDebugMsg:    prefix = "[DEBUG]"; break;
            case QtInfoMsg:     prefix = "[INFO]"; break;
            case QtWarningMsg:  prefix = "[WARN]"; break;
            case QtCriticalMsg: prefix = "[CRIT]"; break;
            case QtFatalMsg:    prefix = "[FATAL]"; break;
        }
        fprintf(stderr, "%s %s\n", prefix, msg.toUtf8().constData());
    });

    printf("[INIT] Qt 메시지 핸들러 설정 완료\n");

    // DPI 설정
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    printf("[INIT] QGuiApplication 생성 중...\n");
    QGuiApplication app(argc, argv);
    printf("[INIT] QGuiApplication 생성 완료\n");

    // *** 중요: 마지막 Window가 닫혀도 앱이 자동 종료되지 않도록 설정 ***
    // RegionSelector나 DebugLogWindow 같은 서브 윈도우가 닫힐 때 메인 앱이 종료되는 것을 방지
    app.setQuitOnLastWindowClosed(false);
    printf("[INIT] setQuitOnLastWindowClosed(false) 설정 완료\n");

    // 애플리케이션 메타데이터
    app.setOrganizationName("ToriYomi");
    app.setOrganizationDomain("toriyomi.app");
    app.setApplicationName("ToriYomi");
    app.setApplicationVersion("0.1.0");
    
    qDebug() << "애플리케이션 시작 - Qt" << qVersion();

    printf("[INIT] QML 엔진 생성 중...\n");
    QQmlApplicationEngine engine;
    printf("[INIT] QML 엔진 생성 완료\n");

    // QML import path 설정
    engine.addImportPath("qrc:/");
    engine.addImportPath("qrc:/ToriYomiApp");
    qDebug() << "QML import paths:" << engine.importPathList();

    printf("[INIT] AppBackend 생성 중...\n");
    
    // AppBackend 생성 - 여기서 크래시가 발생할 가능성이 높음
    toriyomi::ui::AppBackend* backend = nullptr;
    try {
        backend = new toriyomi::ui::AppBackend(&app);
        printf("[INIT] AppBackend 생성 완료\n");
    } catch (const std::exception& e) {
        fprintf(stderr, "[ERROR] AppBackend 생성 실패: %s\n", e.what());
        return -1;
    } catch (...) {
        fprintf(stderr, "[ERROR] AppBackend 생성 실패: 알 수 없는 예외\n");
        return -1;
    }
    
    if (!backend) {
        fprintf(stderr, "[ERROR] AppBackend가 nullptr입니다\n");
        return -1;
    }
    
    qDebug() << "백엔드 생성 완료";

    // QML에 백엔드 노출
    engine.rootContext()->setContextProperty("appBackend", backend);
    qDebug() << "QML 컨텍스트 설정 완료";

    // QML 로드
    const QUrl url(u"qrc:/ToriYomiAppContent/App.qml"_qs);
    qDebug() << "QML URL:" << url;
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { 
            qCritical() << "QML 로딩 실패!";
            QCoreApplication::exit(-1); 
        },
        Qt::QueuedConnection
    );

    printf("[INIT] QML 로딩 중...\n");
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML root 객체가 없습니다!";
        return -1;
    }

    qDebug() << "QML 로딩 완료, 이벤트 루프 시작";
    printf("[INIT] 초기화 완료, 애플리케이션 실행\n");

    return app.exec();
}
