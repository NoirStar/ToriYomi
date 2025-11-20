#include "ui/qml_backend/process_enumerator.h"

#include <algorithm>
#include <psapi.h>
#include <utility>
#include <vector>

namespace toriyomi {
namespace ui {

namespace {
bool IsWindowCandidate(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    wchar_t title[256] = {0};
    if (GetWindowTextW(hwnd, title, 256) == 0) {
        return false;
    }

    return true;
}

QString BuildDisplayName(HWND hwnd, DWORD selfProcessId) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == selfProcessId) {
        return QString();
    }

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!processHandle) {
        return QString();
    }

    wchar_t processName[MAX_PATH] = {0};
    HMODULE moduleHandle = nullptr;
    DWORD bytesNeeded = 0;
    if (EnumProcessModules(processHandle, &moduleHandle, sizeof(moduleHandle), &bytesNeeded)) {
        GetModuleBaseNameW(processHandle, moduleHandle, processName, MAX_PATH);
    }
    CloseHandle(processHandle);

    wchar_t title[256] = {0};
    GetWindowTextW(hwnd, title, 256);

    QString text = QString::fromWCharArray(title);
    if (wcslen(processName) > 0) {
        text += QStringLiteral(" (%1)").arg(QString::fromWCharArray(processName));
    }

    RECT rect{};
    if (GetWindowRect(hwnd, &rect)) {
        const int width = std::max<LONG>(1, rect.right - rect.left);
        const int height = std::max<LONG>(1, rect.bottom - rect.top);
        text += QStringLiteral(" [%1x%2]").arg(width).arg(height);
    }

    text += QStringLiteral(" [PID %1 | HWND 0x%2]")
                .arg(QString::number(processId))
                .arg(QString::number(reinterpret_cast<qulonglong>(hwnd), 16).toUpper());
    return text;
}

}  // namespace

std::vector<ProcessInfo> ProcessEnumerator::EnumerateVisibleWindows(DWORD currentProcessId) {
    std::vector<ProcessInfo> processes;

    struct EnumContext {
        std::vector<ProcessInfo>* list;
        DWORD selfPid;
    } context{&processes, currentProcessId};

    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto* ctx = reinterpret_cast<EnumContext*>(param);
        if (!IsWindowCandidate(hwnd)) {
            return TRUE;
        }

        QString displayText = BuildDisplayName(hwnd, ctx->selfPid);
        if (displayText.isEmpty()) {
            return TRUE;
        }

        ProcessInfo info;
        info.displayText = displayText;
        info.windowHandle = hwnd;
        ctx->list->push_back(std::move(info));
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    return processes;
}

}  // namespace ui
}  // namespace toriyomi
