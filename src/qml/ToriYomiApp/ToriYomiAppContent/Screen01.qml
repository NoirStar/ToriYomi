import QtQuick
import ToriYomiApp

// Screen01Form의 로직 래퍼
Screen01Form {
    id: root
    
    // showRegionSelector 변경 시 창 열기
    onShowRegionSelectorChanged: {
        console.log("showRegionSelector changed:", showRegionSelector)
        if (showRegionSelector) {
            regionSelector.visible = true
        }
    }
    
    // showDebugLog 변경 시 창 열기
    onShowDebugLogChanged: {
        console.log("showDebugLog changed:", showDebugLog)
        if (showDebugLog) {
            debugLogWindow.visible = true
        }
    }
}
