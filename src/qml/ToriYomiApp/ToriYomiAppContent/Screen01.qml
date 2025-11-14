import QtQuick
import ToriYomiApp

// Screen01Form의 로직 래퍼
Screen01Form {
    id: root
    
    Component.onCompleted: {
        appBackend.logMessage.connect(onLogMessage)
        appBackend.sentenceDetected.connect(onSentenceDetected)
        appBackend.processListChanged.connect(onProcessListChanged)
        appBackend.refreshProcessList()
    }
    
    function onProcessListChanged() {
        comboBox.model = appBackend.processList
    }
    
    function onLogMessage(message) {
        if (debugLogWindow) {
            debugLogWindow.addLog(message)
        }
    }
    
    function onSentenceDetected(originalText, tokens) {
        // TODO: ListView
    }
    
    comboBox.onCurrentIndexChanged: {
        if (comboBox.currentIndex >= 0) {
            appBackend.selectProcess(comboBox.currentIndex)
        }
    }
    
    startButton.onClicked: {
        if (startButton.checked) {
            appBackend.startCapture()
        } else {
            appBackend.stopCapture()
        }
    }
    
    regionSelector.onRegionSelected: function(region) {
        appBackend.selectRoi(region.x, region.y, region.width, region.height)
    }
    
    onShowRegionSelectorChanged: {
        if (showRegionSelector) {
            regionSelector.visible = true
        }
    }
    
    onShowDebugLogChanged: {
        if (showDebugLog) {
            debugLogWindow.visible = true
        }
    }
}
