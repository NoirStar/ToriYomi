

/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import ToriYomiApp

Rectangle {
    id: screen01Form
    width: Constants.width
    height: Constants.height
    visible: true

    color: Constants.backgroundColor
    clip: false
    
    // 우측 상단 디버그 버튼
    Button {
        id: debugButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 10
        anchors.rightMargin: 40
        anchors.bottomMargin: 10
        width: 50
        height: 50
        text: "🐛"
        z: 1000
        
        onClicked: screen01Form.showDebugLog = true
        
        background: Rectangle {
            color: debugButton.pressed ? Qt.rgba(0.25, 0.24, 0.28, 0.9) : (debugButton.hovered ? Qt.rgba(0.32, 0.31, 0.35, 0.85) : Qt.rgba(0.28, 0.27, 0.31, 0.75))
            radius: 25
            border.color: Qt.rgba(1, 1, 1, 0.2)
            border.width: 1
            
            Behavior on color {
                ColorAnimation { duration: 200 }
            }
        }
        
        contentItem: Text {
            text: debugButton.text
            font.pixelSize: 20
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    
    // 영역 선택 창 표시 여부
    property bool showRegionSelector: false
    property bool showDebugLog: false

    // 일본어 텍스트 크기 조절
    property real japaneseFontSize: 24
    readonly property bool hasProcessSelection: comboBox.currentIndex >= 0

    // RegionSelector alias
    property alias regionSelector: regionSelector
    property alias debugLogWindow: debugLogWindow

    // 컨트롤 alias (Screen01.qml에서 접근)
    property alias comboBox: comboBox
    property alias startButton: startButton
    property alias processRefreshButton: processRefreshButton
    property alias captureRegionButton: captureRegionButton
    property alias captureIntervalSlider: captureIntervalSlider
    property alias sentencesListView: sentencesListView
    property alias sentenceListModel: sentenceListModel
    property alias tokenListModel: tokenListModel

    // Flow 대신 Column 사용하여 레이아웃 개선
    Column {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        // 상단: 문자 크기 슬라이더 + 제목
        Row {
            width: parent.width
            height: 40
            spacing: 20
            
            // 문자 크기 조절 (왼쪽)
            Row {
                width: parent.width * 0.25
                height: parent.height
                spacing: 10
                
                Text {
                    width: 100
                    height: parent.height
                    text: qsTr("문자 크기 조절")
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: "Maplestory OTF"
                    color: "#ffffff"
                }
                
                Slider {
                    id: fontSizeSlider
                    width: parent.width - 110
                    height: parent.height
                    from: 16
                    to: 48
                    value: screen01Form.japaneseFontSize
                    stepSize: 2
                    
                    onValueChanged: {
                        screen01Form.japaneseFontSize = value
                    }
                }
            }
            
            // 제목 (왼쪽 정렬)
            Text {
                id: text2
                width: parent.width * 0.75 - 20
                height: parent.height
                text: qsTr("찾은 일본어 문장들")
                font.pixelSize: 22
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                font.family: "Maplestory OTF"
                font.weight: Font.DemiBold
                color: "#ffffff"
                leftPadding: 20
            }
        }

        // 상단 리스트뷰 (문장 목록)
        Rectangle {
            id: listViewBox
            width: parent.width
            height: parent.height * 0.55
            visible: true
            color: Qt.rgba(0.15, 0.14, 0.18, 0.6)
            radius: 16
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1

            ListView {
                id: sentencesListView
                anchors.fill: parent
                anchors.margins: 15
                clip: true
                spacing: 5
                model: sentenceListModel
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    width: sentencesListView.width
                    height: screen01Form.japaneseFontSize + 12
                    property bool selected: ListView.isCurrentItem

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: selected ? Qt.rgba(0.4, 0.35, 0.45, 0.25) : Qt.rgba(1, 1, 1, 0.0)
                        border.color: selected ? "#c77da8" : "transparent"
                        border.width: selected ? 2 : 0
                        
                        Behavior on color {
                            ColorAnimation { duration: 200 }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 10

                        Rectangle {
                            width: 4
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 12
                            color: selected ? "#fa9393" : "#5a9fd4"
                            radius: 2
                        }

                        Text {
                            text: model.text
                            color: "#ffffff"
                            font.pixelSize: screen01Form.japaneseFontSize
                            font.family: "IPAexGothic"
                            verticalAlignment: Text.AlignVCenter
                            width: sentencesListView.width - 40
                            wrapMode: Text.Wrap
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: sentencesListView.currentIndex = index
                    }
                }
            }
        }

        // 하단 영역 (왼쪽: 사전/로그, 오른쪽: 컨트롤)
        Row {
            id: bottomRow
            width: parent.width
            height: parent.height - listViewBox.height - text2.height - 20 // 남은 공간 모두 사용
            spacing: 10

            // 왼쪽: ScrollView (사전/로그)
            Rectangle {
                id: scrollViewBox
                width: parent.width * 0.6 // 60% 너비
                height: parent.height
                opacity: 1
                color: Qt.rgba(0.15, 0.14, 0.18, 0.6)
                radius: 16
                border.color: Qt.rgba(1, 1, 1, 0.12)
                border.width: 1

                ScrollView {
                    id: scrollView
                    anchors.fill: parent
                    anchors.margins: 10

                    Column {
                        id: tokenColumn
                        width: scrollView.width - 20
                        spacing: 6

                        Repeater {
                            id: tokenRepeater
                            model: tokenListModel

                                delegate: Rectangle {
                                width: tokenColumn.width
                                height: 48
                                radius: 10
                                color: Qt.rgba(0.2, 0.19, 0.23, 0.4)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1

                                Row {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12

                                    Column {
                                        width: parent.width * 0.4
                                        spacing: 2

                                        Text {
                                            text: surface
                                            color: "#ffffff"
                                            font.pixelSize: 16
                                            font.family: "IPAexGothic"
                                        }

                                        Text {
                                            text: reading
                                            color: "#bbbbbb"
                                            font.pixelSize: 14
                                            font.family: "IPAexGothic"
                                        }
                                    }

                                    Column {
                                        width: parent.width * 0.6 - 12
                                        spacing: 2

                                        Text {
                                            text: qsTr("기본형: %1").arg(baseForm)
                                            color: "#ffffff"
                                            font.pixelSize: 14
                                            font.family: "Maplestory OTF"
                                        }

                                        Text {
                                            text: qsTr("품사: %1").arg(partOfSpeech)
                                            color: "#bbbbbb"
                                            font.pixelSize: 12
                                            font.family: "Maplestory OTF"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Button {
                    id: button1
                    text: qsTr("안키에 넣기")
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 20
                    anchors.bottomMargin: 20
                    font.pointSize: 12
                    font.family: "Maplestory OTF"
                    font.bold: true
                    
                    background: Rectangle {
                        color: button1.pressed ? Qt.rgba(0.28, 0.35, 0.45, 0.9) : (button1.hovered ? Qt.rgba(0.32, 0.40, 0.52, 0.85) : Qt.rgba(0.25, 0.32, 0.42, 0.75))
                        radius: 12
                        border.color: Qt.rgba(1, 1, 1, 0.12)
                        border.width: 1
                        
                        Behavior on color {
                            ColorAnimation { duration: 200 }
                        }
                    }
                    
                    contentItem: Text {
                        text: button1.text
                        font: button1.font
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 오른쪽: GridView (컨트롤 패널)
            Rectangle {
                id: gridBox
                width: parent.width * 0.4 - 10 // 40% 너비 (spacing 고려)
                height: parent.height
                color: Qt.rgba(0.15, 0.14, 0.18, 0.6)
                radius: 16
                border.color: Qt.rgba(1, 1, 1, 0.12)
                border.width: 1

                // GridView 내부를 Column으로 정리
                Column {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 15 // spacing 늘림 (10 → 15)

                    Text {
                        id: text3
                        width: parent.width
                        height: 30
                        text: qsTr("캡쳐할 프로그램")
                        font.pixelSize: 20
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: "Maplestory OTF"
                        color: "#ffb3d9"
                    }

                    RowLayout {
                        width: parent.width
                        height: 50
                        spacing: 12

                        ComboBox {
                            id: comboBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: parent.height
                            font.family: "Maplestory OTF"
                            enabled: !appBackend.isCapturing
                            opacity: enabled ? 1 : 0.95
                            Component.onCompleted: comboBox.currentIndex = -1

                            background: Rectangle {
                                radius: 12
                                color: comboBox.enabled ? "#2a2835" : "#1f1c27"
                                border.color: comboBox.activeFocus ? "#876bb8" : Qt.rgba(1, 1, 1, 0.08)
                                border.width: comboBox.activeFocus ? 2 : 1
                            }

                            contentItem: Text {
                                text: comboBox.currentIndex >= 0 && comboBox.displayText.length > 0
                                      ? comboBox.displayText
                                      : qsTr("프로세스를 선택하세요")
                                font: comboBox.font
                                color: comboBox.currentIndex >= 0 ? "#ffffff" : "#dcd4ff"
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                                elide: Text.ElideRight
                                leftPadding: 16
                                rightPadding: 16
                            }
                        }

                        Button {
                            id: processRefreshButton
                            Layout.preferredWidth: 55
                            Layout.preferredHeight: 50
                            text: "⟳"
                            font.bold: false
                            font.family: "Segoe UI Symbol"
                            font.pixelSize: 24
                            focusPolicy: Qt.NoFocus
                            enabled: !appBackend.isCapturing
                            opacity: enabled ? 1 : 0.4
                            hoverEnabled: true

                            background: Rectangle {
                                color: processRefreshButton.pressed ? Qt.rgba(0.25, 0.24, 0.28, 0.9) : (processRefreshButton.hovered ? Qt.rgba(0.28, 0.27, 0.31, 0.8) : Qt.rgba(0.22, 0.21, 0.25, 0.7))
                                radius: 12
                                border.color: Qt.rgba(1, 1, 1, 0.1)
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation { duration: 200 }
                                }
                            }

                            contentItem: Item {
                                anchors.fill: parent

                                Text {
                                    id: refreshIcon
                                    anchors.centerIn: parent
                                    text: processRefreshButton.text
                                    font.pixelSize: processRefreshButton.font.pixelSize
                                    font.family: processRefreshButton.font.family
                                    color: processRefreshButton.hovered ? "#ffffff" : "#d0d0d0"
                                    transformOrigin: Item.Center
                                    scale: processRefreshButton.pressed ? 0.9 : 1.0

                                    Behavior on scale {
                                        NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
                                    }
                                    
                                    Behavior on color {
                                        ColorAnimation { duration: 200 }
                                    }
                                }

                                PropertyAnimation {
                                    id: refreshSpin
                                    target: refreshIcon
                                    property: "rotation"
                                    from: 0
                                    to: 360
                                    duration: 600
                                    loops: 1
                                    running: processRefreshButton.pressed
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }

                    Item {
                        width: parent.width
                        height: 5
                    }

                    // 가로 배치: 왼쪽(슬라이더) + 오른쪽(버튼 2개)
                    Row {
                        width: parent.width
                        spacing: 10

                        Column {
                            width: parent.width / 2 - 5
                            spacing: 5

                            Text {
                                width: parent.width
                                height: 30
                                text: qsTr("화면 캡쳐 간격: %1초").arg(captureIntervalSlider.value.toFixed(1))
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.family: "Maplestory OTF"
                                color: "#ffffff"
                            }

                            Slider {
                                id: captureIntervalSlider
                                width: parent.width
                                height: 40
                                from: 0.1
                                to: 5.0
                                value: 1.0
                                stepSize: 0.1
                            }
                        }

                        Column {
                            width: parent.width / 2 - 5
                            spacing: 10

                            Button {
                                id: captureRegionButton
                                width: parent.width
                                height: 40
                                text: qsTr("캡쳐 영역 선택")
                                font.pointSize: 12
                                font.family: "Maplestory OTF"
                                font.bold: true
                                enabled: screen01Form.hasProcessSelection && !appBackend.isCapturing
                                opacity: enabled ? 1 : 0.4

                                onClicked: screen01Form.showRegionSelector = true

                                background: Rectangle {
                                    color: captureRegionButton.pressed ? Qt.rgba(0.35, 0.30, 0.42, 0.9) : (captureRegionButton.hovered ? Qt.rgba(0.42, 0.37, 0.50, 0.85) : Qt.rgba(0.38, 0.33, 0.46, 0.75))
                                    radius: 12
                                    border.color: Qt.rgba(1, 1, 1, 0.15)
                                    border.width: 1

                                    Behavior on color {
                                        ColorAnimation { duration: 200 }
                                    }
                                }

                                contentItem: Text {
                                    text: captureRegionButton.text
                                    font: captureRegionButton.font
                                    color: "#ffffff"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Button {
                                id: startButton
                                width: parent.width
                                height: 40
                                text: startButton.checked ? qsTr("캡쳐 중지") : qsTr("캡쳐 시작")
                                font.pointSize: 12
                                font.family: "Maplestory OTF"
                                font.bold: true
                                checkable: true
                                checked: false
                                enabled: screen01Form.hasProcessSelection || appBackend.isCapturing
                                opacity: enabled ? 1 : 0.4

                                background: Rectangle {
                                    color: startButton.checked ? Qt.rgba(0.82, 0.28, 0.22, 0.9) : (startButton.pressed ? Qt.rgba(0.15, 0.58, 0.28, 0.95) : (startButton.hovered ? Qt.rgba(0.18, 0.68, 0.35, 0.9) : Qt.rgba(0.15, 0.62, 0.32, 0.85)))
                                    radius: 12
                                    border.color: Qt.rgba(1, 1, 1, 0.15)
                                    border.width: 1

                                    Behavior on color {
                                        ColorAnimation { duration: 200 }
                                    }
                                }

                                contentItem: Text {
                                    text: startButton.text
                                    font: startButton.font
                                    color: "#ffffff"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 영역 선택 별도 창
    RegionSelector {
        id: regionSelector
        parentController: screen01Form
    }
    
    // 디버그 로그 창
    DebugLogWindow {
        id: debugLogWindow
        parentController: screen01Form
    }

    ListModel {
        id: sentenceListModel
    }

    ListModel {
        id: tokenListModel
    }
}
