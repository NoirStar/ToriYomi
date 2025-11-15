import QtQuick
import QtQuick.Controls
import QtQuick.Window

// 디버그 로그 창
Window {
    id: debugWindow
    
    width: 600
    height: 400
    title: qsTr("디버그 로그")
    flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint
    color: "transparent"
    
    // 외부에서 컨트롤할 property
    property var parentController: null
    
    // 로그 메시지 모델
    property var logMessages: []
    
    // 로그 추가 함수
    function addLog(message) {
        var timestamp = new Date().toLocaleTimeString('ko-KR')
        logMessages.push("[" + timestamp + "] " + message)
        logListModel.append({"text": "[" + timestamp + "] " + message})
        
        // 자동 스크롤
        logListView.positionViewAtEnd()
    }
    
    // 로그 클리어
    function clearLogs() {
        logMessages = []
        logListModel.clear()
    }
    
    // 창이 닫힐 때 호출되는 핸들러
    onClosing: (close) => {
        console.log("DebugLogWindow: onClosing")
        visible = false
    }
    
    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"
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
            
            // 하단 모서리만 직각으로
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 10
                color: parent.color
            }
            
            // 타이틀 텍스트
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 15
                text: qsTr("🐛 디버그 로그")
                color: "#fa9393"
                font.pixelSize: 16
                font.family: "Maplestory OTF"
                font.bold: true
            }
            
            // 닫기 버튼
            Rectangle {
                id: closeButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 5
                width: 30
                height: 30
                color: closeButtonArea.pressed ? "#c0392b" : (closeButtonArea.containsMouse ? "#e74c3c" : "#3d3d3d")
                radius: 15
                z: 10
                
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
                    onClicked: {
                        debugWindow.visible = false
                        if (debugWindow.parentController) {
                            debugWindow.parentController.showDebugLog = false
                        }
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
                    if (pressed) {
                        var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                        debugWindow.x += delta.x
                        debugWindow.y += delta.y
                    }
                }
            }
        }
        
        Column {
            anchors.fill: parent
            anchors.margins: 10
            anchors.topMargin: 50
            spacing: 10
            
            // 상단 버튼 영역
            Row {
                width: parent.width
                spacing: 10
                
                Button {
                    text: qsTr("로그 지우기")
                    onClicked: debugWindow.clearLogs()
                    
                    background: Rectangle {
                        color: parent.pressed ? "#c75a7a" : (parent.hovered ? "#e67799" : "#d66b88")
                        radius: 5
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: "Maplestory OTF"
                    }
                }

                Button {
                    text: qsTr("ROI 스냅샷 저장")
                    onClicked: {
                        var path = appBackend.saveCurrentRoiSnapshot()
                        if (path && path.length > 0) {
                            debugWindow.addLog(qsTr("ROI 스냅샷 저장: %1").arg(path))
                        } else {
                            debugWindow.addLog(qsTr("ROI 스냅샷 저장 실패"))
                        }
                    }

                    background: Rectangle {
                        color: parent.pressed ? "#5c50d6" : (parent.hovered ? "#7b6fff" : "#6a5def")
                        radius: 5
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: "Maplestory OTF"
                    }
                }
                
                Text {
                    text: qsTr("총 %1개 로그").arg(logListModel.count)
                    color: "#ffffff"
                    font.family: "Maplestory OTF"
                    verticalAlignment: Text.AlignVCenter
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            
            // 로그 리스트
            Rectangle {
                width: parent.width
                height: parent.height - 50
                color: "#2b2b2b"
                border.color: "#fa9393"
                border.width: 1
                radius: 5
                
                ListView {
                    id: logListView
                    anchors.fill: parent
                    anchors.margins: 5
                    clip: true
                    
                    model: ListModel {
                        id: logListModel
                    }
                    
                    delegate: Rectangle {
                        width: logListView.width
                        height: logText.height + 10
                        color: index % 2 === 0 ? "#2b2b2b" : "#333333"
                        
                        Text {
                            id: logText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 5
                            anchors.rightMargin: 5
                            text: model.text
                            color: "#00ff00"
                            font.family: "Maplestory OTF"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                    
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                    }
                }
            }
        }
    }
    
    Component.onCompleted: {
        addLog("디버그 로그 창 시작")
    }
}
