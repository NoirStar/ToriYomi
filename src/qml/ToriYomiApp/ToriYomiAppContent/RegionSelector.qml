import QtQuick
import QtQuick.Controls
import QtQuick.Window

// 영역 선택 별도 창 컴포넌트
Window {
    id: regionSelector
    
    title: qsTr("캡쳐 영역 선택")
    flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    color: "#111214"
    
    // 외부에서 컨트롤할 property
    property var parentController: null
    
    // 외부에서 접근 가능한 프로퍼티
    property rect selectedRegion: Qt.rect(0, 0, 0, 0)
    property bool isSelecting: false
    property point dragStart: Qt.point(0, 0)
    property size captureWindowSize: Qt.size(appBackend.previewImageSize.width,
                                             appBackend.previewImageSize.height)
    property var scaledPreviewSize: calculateScaledSize()
    width: scaledPreviewSize.width
    height: scaledPreviewSize.height

    function calculateScaledSize() {
        var sourceWidth = captureWindowSize.width > 0 ? captureWindowSize.width : 800
        var sourceHeight = captureWindowSize.height > 0 ? captureWindowSize.height : 600
        var maxWidth = 1280
        var maxHeight = 900
        var minWidth = 360
        var minHeight = 240
        var scale = Math.min(1.0, Math.min(maxWidth / sourceWidth, maxHeight / sourceHeight))
        var scaledWidth = sourceWidth * scale
        var scaledHeight = sourceHeight * scale
        var aspect = sourceHeight / sourceWidth
        if (scaledWidth < minWidth && sourceWidth > minWidth) {
            scaledWidth = minWidth
            scaledHeight = minWidth * aspect
        }
        if (scaledHeight < minHeight && sourceHeight > minHeight) {
            scaledHeight = minHeight
            scaledWidth = minHeight / aspect
        }
        scaledWidth = Math.min(scaledWidth, sourceWidth)
        scaledHeight = Math.min(scaledHeight, sourceHeight)
        return {
            width: Math.round(scaledWidth),
            height: Math.round(scaledHeight)
        }
    }
    
    // visible이 false로 변경될 때 완전히 초기화
    onVisibleChanged: {
        console.log("RegionSelector.onVisibleChanged:", visible)
        if (visible) {
            isSelecting = false
            selectedRegion = Qt.rect(0, 0, 0, 0)
            dragStart = Qt.point(0, 0)
            console.log("RegionSelector: Reset state")
        }
    }
    
    // 창이 닫힐 때 호출되는 핸들러
    onClosing: (close) => {
        console.log("RegionSelector: onClosing")
        visible = false
    }

    function imageBounds() {
        var paintedWidth = previewImage.paintedWidth > 0 ? previewImage.paintedWidth : mouseArea.width
        var paintedHeight = previewImage.paintedHeight > 0 ? previewImage.paintedHeight : mouseArea.height
        var offsetX = (mouseArea.width - paintedWidth) / 2
        var offsetY = (mouseArea.height - paintedHeight) / 2
        return {
            x: offsetX,
            y: offsetY,
            width: paintedWidth,
            height: paintedHeight
        }
    }

    function clampPointToImage(x, y) {
        var bounds = imageBounds()
        var minX = bounds.x
        var maxX = bounds.x + bounds.width
        var minY = bounds.y
        var maxY = bounds.y + bounds.height
        var clampedX = Math.max(minX, Math.min(maxX, x))
        var clampedY = Math.max(minY, Math.min(maxY, y))
        return Qt.point(clampedX, clampedY)
    }
    
    // 시그널
    signal regionSelected(rect region)
    signal selectionCancelled()
    
    // 배경 컨테이너
    Rectangle {
        anchors.fill: parent
        color: "#151515"
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
            z: 1000
            
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
                text: qsTr("📐 캡쳐 영역 선택")
                color: "#fa9393"
                font.pixelSize: 16
                font.family: "Maplestory OTF"
                font.bold: true
            }
            
            // 선택 영역 크기 표시
            Text {
                id: sizeDisplayText
                anchors.right: closeButton.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 15
                text: regionSelector.isSelecting ? qsTr("%1 × %2").arg(Math.round(selectionRect.width)).arg(Math.round(selectionRect.height)) : ""
                color: "#ffffff"
                font.pixelSize: 14
                font.family: "Maplestory OTF"
                visible: regionSelector.isSelecting
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
                        regionSelector.visible = false
                        if (regionSelector.parentController) {
                            regionSelector.parentController.showRegionSelector = false
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
                        regionSelector.x += delta.x
                        regionSelector.y += delta.y
                    }
                }
            }
        }
        
        // 메인 컨텐츠 영역
        Rectangle {
            id: contentArea
            anchors.fill: parent
            anchors.topMargin: 40
            color: "transparent"
            radius: 10
            clip: true  // 내용이 밖으로 나가지 않도록
            
            // 미리보기 이미지 (C++에서 제공)
            Image {
                id: previewImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter
                asynchronous: true
                cache: false
                source: appBackend.previewImageData
                opacity: 1.0
            }
            
            // 선택 영역 표시
            Rectangle {
                id: selectionRect
                color: "transparent"
                border.color: "#fa9393"
                border.width: 2
                visible: regionSelector.isSelecting
                
                property point currentMouse: regionSelector.clampPointToImage(mouseArea.mouseX, mouseArea.mouseY)
                property real clampedStartX: regionSelector.dragStart.x
                property real clampedStartY: regionSelector.dragStart.y
                
                x: Math.min(clampedStartX, currentMouse.x)
                y: Math.min(clampedStartY, currentMouse.y)
                width: Math.abs(currentMouse.x - clampedStartX)
                height: Math.abs(currentMouse.y - clampedStartY)
            
                // 선택 영역 내부 밝게 표시
                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(1, 1, 1, 0.2)
                }
            }
        }
        
        // 마우스 영역
        MouseArea {
            id: mouseArea
            anchors.fill: contentArea
            hoverEnabled: true
            cursorShape: Qt.CrossCursor
            
            onPressed: (mouse) => {
                var clampedPoint = regionSelector.clampPointToImage(mouse.x, mouse.y)
                regionSelector.dragStart = clampedPoint
                regionSelector.isSelecting = true
            }
            
            onReleased: (mouse) => {
                if (regionSelector.isSelecting) {
                    var bounds = regionSelector.imageBounds()
                    var endPoint = regionSelector.clampPointToImage(mouse.x, mouse.y)
                    var x = Math.min(regionSelector.dragStart.x, endPoint.x)
                    var y = Math.min(regionSelector.dragStart.y, endPoint.y)
                    var w = Math.abs(endPoint.x - regionSelector.dragStart.x)
                    var h = Math.abs(endPoint.y - regionSelector.dragStart.y)
                    
                    // 최소 크기 체크 (10x10 픽셀)
                    if (w > 10 && h > 10) {
            var scaleX = (regionSelector.captureWindowSize.width > 0 && bounds.width > 0)
                                ? regionSelector.captureWindowSize.width / bounds.width : 1
            var scaleY = (regionSelector.captureWindowSize.height > 0 && bounds.height > 0)
                                ? regionSelector.captureWindowSize.height / bounds.height : 1
            var offsetX = bounds.x
            var offsetY = bounds.y
            var actualX = Math.round((x - offsetX) * scaleX)
            var actualY = Math.round((y - offsetY) * scaleY)
            var actualW = Math.round(w * scaleX)
            var actualH = Math.round(h * scaleY)
                        regionSelector.selectedRegion = Qt.rect(actualX, actualY, actualW, actualH)
                        regionSelector.regionSelected(regionSelector.selectedRegion)
                        console.log("Selected region (display): x=" + x + " y=" + y + " w=" + w + " h=" + h)
                        console.log("Selected region (actual): x=" + actualX + " y=" + actualY + " w=" + actualW + " h=" + actualH)
                        regionSelector.visible = false
                        if (regionSelector.parentController) {
                            regionSelector.parentController.showRegionSelector = false
                        }
                    } else {
                        console.log("Selection too small, cancelled")
                        regionSelector.selectionCancelled()
                        regionSelector.visible = false
                        if (regionSelector.parentController) {
                            regionSelector.parentController.showRegionSelector = false
                        }
                    }
                    
                    regionSelector.isSelecting = false
                }
            }
        }
        
        // 안내 텍스트
        Rectangle {
            anchors.centerIn: parent
            width: instructionText.width + 40
            height: instructionText.height + 20
            color: Qt.rgba(0, 0, 0, 0.8)
            radius: 8
            visible: !regionSelector.isSelecting
            
            Text {
                id: instructionText
                anchors.centerIn: parent
                text: qsTr("드래그하여 캡쳐 영역을 선택하세요\n(ESC: 취소)")
                color: "#ffffff"
                font.pixelSize: 16
                font.family: "Maplestory OTF"
                horizontalAlignment: Text.AlignHCenter
            }
        }
        }
    
    // ESC 키로 취소
    Shortcut {
        sequence: "Escape"
        onActivated: {
            regionSelector.selectionCancelled()
            regionSelector.visible = false
            if (regionSelector.parentController) {
                regionSelector.parentController.showRegionSelector = false
            }
        }
    }
}
