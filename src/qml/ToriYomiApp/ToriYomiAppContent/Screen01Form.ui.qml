

/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/
import QtQuick
import QtQuick.Controls
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
            color: debugButton.pressed ? "#8a7ac7" : (debugButton.hovered ? "#a88aff" : "#9a7aff")
            radius: 20
            border.color: "#ffffff"
            border.width: debugButton.hovered ? 2 : 0
            
            Behavior on color {
                ColorAnimation { duration: 150 }
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
    
    // RegionSelector alias
    property alias regionSelector: regionSelector
    property alias debugLogWindow: debugLogWindow
    
    // 컨트롤 alias (Screen01.qml에서 접근)
    property alias comboBox: comboBox
    property alias startButton: startButton

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
            height: parent.height * 0.55 // 전체 높이의 55% (50% → 55%)
            visible: true
            color: "#00ffffff"
            radius: 25
            border.color: "#3d3d3d"
            border.width: 2

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: 15
                clip: true // 스크롤 시 내용 잘리도록
                spacing: 5 // 아이템 간 간격
                model: ListModel {
                    ListElement {
                        name: "これはテストです"
                        colorCode: "#fa9393"
                    }
                    ListElement {
                        name: "日本語の文章"
                        colorCode: "#fa9393"
                    }
                    ListElement {
                        name: "サンプルテキスト"
                        colorCode: "#fa9393"
                    }
                }
                delegate: Row {
                    spacing: 10
                    width: listView.width

                    Rectangle {
                        width: 4
                        height: screen01Form.japaneseFontSize + 5
                        color: "#5a9fd4"
                        radius: 2
                    }

                    Text {
                        text: name
                        color: "#ffffff"
                        font.pixelSize: screen01Form.japaneseFontSize
                        font.family: "IPAexGothic"
                        verticalAlignment: Text.AlignVCenter
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
                color: "#00ffffff"
                radius: 25
                border.color: "#3d3d3d"
                border.width: 2

                ScrollView {
                    id: scrollView
                    anchors.fill: parent
                    anchors.margins: 10
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
                        color: button1.pressed ? "#8a7ac7" : (button1.hovered ? "#a88aff" : "#9a7aff")
                        radius: 8
                        border.color: "#ffffff"
                        border.width: button1.hovered ? 2 : 0
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
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
                color: "#00ffffff"
                radius: 25
                border.color: "#3d3d3d"
                border.width: 2

                // GridView 내부를 Column으로 정리
                Column {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 15 // spacing 늘림 (10 → 15)

                    Text {
                        id: text3
                        width: parent.width
                        height: 30 // 고정 높이
                        text: qsTr("캡쳐할 프로그램")
                        font.pixelSize: 20
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: "Maplestory OTF"
                        color: "#ffffff" // 다크모드용 흰색
                    }

                    ComboBox {
                        id: comboBox
                        width: parent.width
                        height: 45 // 약간 줄임
                        font.family: "Maplestory OTF"
                    }

                    Item {
                        width: parent.width
                        height: 5 // 추가 간격
                    }

                    // 가로 배치: 왼쪽(슬라이더) + 오른쪽(버튼 2개)
                    Row {
                        id: row
                        width: parent.width
                        spacing: 10

                        // 왼쪽: 화면 캡쳐 간격
                        Column {
                            width: parent.width / 2 - 5
                            spacing: 5

                            Text {
                                id: text1
                                width: parent.width
                                height: 30 // 고정 높이
                                text: qsTr("화면 캡쳐 간격: %1초").arg(slider.value.toFixed(1))
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.family: "Maplestory OTF"
                                color: "#ffffff" // 다크모드용 흰색
                            }

                            Slider {
                                id: slider
                                width: parent.width
                                height: 40 // 고정 높이
                                from: 0.1
                                to: 5.0
                                value: 1.0
                                stepSize: 0.1
                            }
                        }

                        // 오른쪽: 버튼 2개 세로 배치
                        Column {
                            width: parent.width / 2 - 5
                            spacing: 10

                            Button {
                                id: button
                                width: parent.width
                                height: 40
                                text: qsTr("캡쳐 영역 선택")
                                font.pointSize: 12
                                font.family: "Maplestory OTF"
                                font.bold: true
                                
                                onClicked: screen01Form.showRegionSelector = true
                                
                                background: Rectangle {
                                    color: button.pressed ? "#c75a7a" : (button.hovered ? "#e67799" : "#d66b88")
                                    radius: 8
                                    border.color: "#ffffff"
                                    border.width: button.hovered ? 2 : 0
                                    
                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }
                                }
                                
                                contentItem: Text {
                                    text: button.text
                                    font: button.font
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
                                
                                background: Rectangle {
                                    color: startButton.checked ? "#e74c3c" : (startButton.pressed ? "#27ae60" : (startButton.hovered ? "#3fca7a" : "#2ecc71"))
                                    radius: 8
                                    border.color: "#ffffff"
                                    border.width: startButton.hovered ? 2 : 0
                                    
                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
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
}
