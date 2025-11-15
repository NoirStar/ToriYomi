import QtQuick
import QtQml
import QtQml.Models
import ToriYomiApp

// Screen01Form의 로직 래퍼
Screen01Form {
    id: root
    property int maxSentenceEntries: 200
    
    Component.onCompleted: {
        appBackend.logMessage.connect(onLogMessage)
        appBackend.sentenceDetected.connect(onSentenceDetected)
        appBackend.processListChanged.connect(onProcessListChanged)
        appBackend.refreshProcessList()
        sentenceListModel.clear()
        tokenListModel.clear()
        captureRegionButton.clicked.connect(handleCaptureButtonClicked)
        processRefreshButton.clicked.connect(handleProcessRefreshClicked)
        captureIntervalSlider.valueChanged.connect(handleCaptureIntervalChanged)
        captureIntervalSlider.value = appBackend.captureIntervalSeconds
        appBackend.setCaptureIntervalSeconds(captureIntervalSlider.value)
    }
    
    function onProcessListChanged() {
        comboBox.model = appBackend.processList
        comboBox.currentIndex = -1
    }
    
    function onLogMessage(message) {
        if (debugLogWindow) {
            debugLogWindow.addLog(message)
        }
    }
    
    function onSentenceDetected(originalText, tokens) {
        appendSentence(originalText, tokens)
    }

    function appendSentence(originalText, tokens) {
        if (!sentenceListModel) {
            return
        }

        sentenceListModel.insert(0, {
            "text": originalText,
            "colorCode": "#fa9393",
            "tokens": tokens
        })

        if (sentenceListModel.count > maxSentenceEntries) {
            sentenceListModel.remove(maxSentenceEntries, sentenceListModel.count - maxSentenceEntries)
        }

        if (sentencesListView) {
            sentencesListView.currentIndex = 0
            sentencesListView.positionViewAtBeginning()
        }
    }

    function updateTokenList(tokens) {
        if (!tokenListModel) {
            return
        }

        tokenListModel.clear()
        if (!tokens || tokens.length === 0) {
            return
        }

        for (var i = 0; i < tokens.length; ++i) {
            var token = tokens[i]
            tokenListModel.append({
                "surface": token.surface,
                "reading": token.reading,
                "baseForm": token.baseForm,
                "partOfSpeech": token.partOfSpeech
            })
        }
    }

    function showSentenceTokens(index) {
        if (!sentenceListModel || !tokenListModel) {
            return
        }

        if (index < 0 || index >= sentenceListModel.count) {
            tokenListModel.clear()
            return
        }

        var entry = sentenceListModel.get(index)
        if (entry && entry.tokens) {
            updateTokenList(entry.tokens)
        } else {
            tokenListModel.clear()
        }
    }

    function handleCaptureButtonClicked() {
        if (comboBox.currentIndex < 0) {
            appBackend.logMessage("[UI] 캡쳐할 프로세스를 먼저 선택하세요")
            return
        }

        appBackend.refreshPreviewImage()
        showRegionSelector = true
    }

    function handleProcessRefreshClicked() {
        appBackend.refreshProcessList()
    }

    function handleCaptureIntervalChanged() {
        appBackend.setCaptureIntervalSeconds(captureIntervalSlider.value)
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
        regionSelector.visible = showRegionSelector
    }

    sentencesListView.onCurrentIndexChanged: {
        showSentenceTokens(sentencesListView.currentIndex)
    }
    
    onShowDebugLogChanged: {
        if (showDebugLog) {
            debugLogWindow.visible = true
        }
    }
}
