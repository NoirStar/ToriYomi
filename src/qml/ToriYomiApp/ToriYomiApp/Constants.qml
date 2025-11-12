pragma Singleton
import QtQuick
import QtQuick.Studio.Application

QtObject {
    readonly property int width: 1280
    readonly property int height: 760

    property string relativeFontDirectory: "fonts"

    /* Edit this comment to add your custom font */
    readonly property font font: Qt.font({
                                             family: Qt.application.font.family,
                                             pixelSize: Qt.application.font.pixelSize
                                         })
    readonly property font largeFont: Qt.font({
                                                  family: Qt.application.font.family,
                                                  pixelSize: Qt.application.font.pixelSize * 1.6
                                              })

    readonly property color backgroundColor: "#2b2b2b"  // 다크 테마


    property StudioApplication application: StudioApplication {
        fontPath: Qt.resolvedUrl("../ToriYomiAppContent/" + relativeFontDirectory)
    }
}
