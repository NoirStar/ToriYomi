import QtQuick
import ToriYomiApp

Window {
    id: appWindow
    width: Constants.width
    height: Constants.height

    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    
    // 일본어 폰트 로드
    FontLoader {
        id: japaneseFont
        source: "ipaexg.ttf"
    }
    
    Rectangle {
        anchors.fill: parent
        color: Constants.backgroundColor
        border.color: "#2b2b2b"
        border.width: 0
        radius: 10
        
        // 커스텀 타이틀바
        Rectangle {
            id: titleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 40
            color: "#181818"
            radius: 10
            z: 100
            
            // 하단 모서리만 직각으로
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 10
                color: parent.color
            }
            
            // 타이틀 텍스트
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 15
                spacing: 10
                
                Text {
                    text: "🌱"
                    font.pixelSize: 20
                    anchors.verticalCenter: parent.verticalCenter
                }
                
                Text {
                    text: qsTr("ToriYomi - 일본어 학습 도구")
                    color: "#fa9393"
                    font.pixelSize: 16
                    font.family: "Maplestory OTF"
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            
            // 우측 버튼들
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 5
                spacing: 5
                
                // 최소화 버튼
                Rectangle {
                    id: minimizeButton
                    width: 30
                    height: 30
                    color: minimizeButtonArea.pressed ? "#7f8c8d" : (minimizeButtonArea.containsMouse ? "#95a5a6" : "#3d3d3d")
                    radius: 15
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "─"
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.family: "Maplestory OTF"
                    }
                    
                    MouseArea {
                        id: minimizeButtonArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: appWindow.showMinimized()
                    }
                }
                
                // 최대화/복원 버튼
                Rectangle {
                    id: maximizeButton
                    width: 30
                    height: 30
                    color: maximizeButtonArea.pressed ? "#7f8c8d" : (maximizeButtonArea.containsMouse ? "#95a5a6" : "#3d3d3d")
                    radius: 15
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: appWindow.visibility === Window.Maximized ? "❐" : "□"
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.family: "Maplestory OTF"
                    }
                    
                    MouseArea {
                        id: maximizeButtonArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (appWindow.visibility === Window.Maximized) {
                                appWindow.showNormal()
                            } else {
                                appWindow.showMaximized()
                            }
                        }
                    }
                }
                
                // 닫기 버튼
                Rectangle {
                    id: closeButton
                    width: 30
                    height: 30
                    color: closeButtonArea.pressed ? "#c0392b" : (closeButtonArea.containsMouse ? "#e74c3c" : "#3d3d3d")
                    radius: 15
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.family: "Maplestory OTF"
                    }
                    
                    MouseArea {
                        id: closeButtonArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: Qt.quit()
                    }
                }
            }
            
            // 드래그 영역
            MouseArea {
                anchors.fill: parent
                property point clickPos: Qt.point(0, 0)
                z: -1
                
                onPressed: (mouse) => {
                    clickPos = Qt.point(mouse.x, mouse.y)
                }
                
                onPositionChanged: (mouse) => {
                    if (pressed && appWindow.visibility !== Window.Maximized) {
                        var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                        appWindow.x += delta.x
                        appWindow.y += delta.y
                    }
                }
                
                onDoubleClicked: {
                    if (appWindow.visibility === Window.Maximized) {
                        appWindow.showNormal()
                    } else {
                        appWindow.showMaximized()
                    }
                }
            }
        }

        Screen01 {
            id: mainScreen
            anchors.fill: parent
            anchors.topMargin: 40
            visible: true
        }
    }
}
