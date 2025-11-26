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
        appBackend.screenChanged.connect(onScreenChanged)
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
        if (appBackend.processList.length > 0) {
            comboBox.currentIndex = 0
        } else {
            comboBox.currentIndex = -1
        }
    }
    
    function onLogMessage(message) {
        if (debugLogWindow) {
            debugLogWindow.addLog(message)
        }
    }
    
    function onScreenChanged() {
        // 화면이 변경되면 기존 문장 목록을 초기화
        if (sentenceListModel) {
            sentenceListModel.clear()
        }
        if (tokenListModel) {
            tokenListModel.clear()
        }
        console.log("[Screen01] 화면 변경 감지 - 문장 목록 초기화")
    }
    
    function onSentenceDetected(originalText, tokens) {
        appendSentences(originalText, tokens)
    }

    function appendSentences(originalText, tokens) {
        if (!sentenceListModel) {
            return
        }

        // 줄 단위로 분리하여 개별 문장으로 추가
        var lines = originalText.split('\n')
        console.log("[DEBUG] Original text:", originalText)
        console.log("[DEBUG] Lines count:", lines.length)
        for (var j = 0; j < lines.length; j++) {
            console.log("[DEBUG] Line " + j + ":", lines[j])
        }
        var tokensPerLine = distributeTokens(lines, tokens)

        // 역순으로 추가하여 원래 순서대로 보이도록 함
        for (var i = lines.length - 1; i >= 0; i--) {
            var line = lines[i].trim()
            if (line.length === 0) {
                continue
            }

            sentenceListModel.insert(0, {
                "text": line,
                "colorCode": "#fa9393",
                "tokens": tokensPerLine[i] || []
            })
        }

        if (sentenceListModel.count > maxSentenceEntries) {
            sentenceListModel.remove(maxSentenceEntries, sentenceListModel.count - maxSentenceEntries)
        }

        if (sentencesListView) {
            sentencesListView.currentIndex = 0
            sentencesListView.positionViewAtBeginning()
        }
    }

    // 토큰을 각 줄에 맞게 분배
    function distributeTokens(lines, tokens) {
        var result = []
        for (var i = 0; i < lines.length; i++) {
            result.push([])
        }

        if (!tokens || tokens.length === 0) {
            return result
        }

        var tokenIndex = 0
        for (var lineIdx = 0; lineIdx < lines.length; lineIdx++) {
            var line = lines[lineIdx].trim()
            var lineTokens = []
            var charPos = 0

            while (tokenIndex < tokens.length && charPos < line.length) {
                var token = tokens[tokenIndex]
                var surface = token.surface || ""

                // 현재 토큰이 이 줄에 포함되는지 확인
                var foundPos = line.indexOf(surface, charPos)
                if (foundPos !== -1 && foundPos <= charPos + 2) {
                    lineTokens.push(token)
                    charPos = foundPos + surface.length
                    tokenIndex++
                } else {
                    // 토큰이 이 줄에 없으면 다음 줄로
                    break
                }
            }

            result[lineIdx] = lineTokens
        }

        return result
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
