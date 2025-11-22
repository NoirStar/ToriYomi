#include "ui/qml_backend/process_enumerator.h"

#include <algorithm>
#include <array>
#include <psapi.h>
#include <utility>
#include <vector>
#include <cwchar>

#include "common/windows/window_visibility.h"

namespace toriyomi {
namespace ui {

namespace {
bool IsWindowMinimized(HWND hwnd) {
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &placement)) {
        if (placement.showCmd == SW_SHOWMINIMIZED || placement.showCmd == SW_SHOWMINNOACTIVE) {
            return true;
        }
    }
    return IsIconic(hwnd) != FALSE;
}

bool IsToolOrChildWindow(HWND hwnd) {
    const LONG style = GetWindowLong(hwnd, GWL_STYLE);
    const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (style & WS_CHILD) {
        return true;
    }
    if (exStyle & WS_EX_TOOLWINDOW) {
        return true;
    }
    return false;
}

bool IsWindowCandidate(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    if (IsWindowMinimized(hwnd)) {
        return false;
    }

    if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return false;
    }

    if (IsToolOrChildWindow(hwnd)) {
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }
    if ((rect.right - rect.left) <= 30 || (rect.bottom - rect.top) <= 30) {
        return false;
    }

    return true;
}

bool IsBlockedProcess(DWORD processId) {
    static const std::array<const wchar_t*, 2> kBlockedProcesses = {
        L"EXPLORER.EXE",
        L"TEXTINPUTHOST.EXE"
    };

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!processHandle) {
        return false;
    }

    bool isBlocked = false;
    wchar_t processName[MAX_PATH] = {0};
    HMODULE moduleHandle = nullptr;
    DWORD bytesNeeded = 0;
    if (EnumProcessModules(processHandle, &moduleHandle, sizeof(moduleHandle), &bytesNeeded)) {
        if (GetModuleBaseNameW(processHandle, moduleHandle, processName, MAX_PATH) > 0) {
            for (const auto* blocked : kBlockedProcesses) {
                if (_wcsicmp(processName, blocked) == 0) {
                    isBlocked = true;
                    break;
                }
            }
        }
    }

    CloseHandle(processHandle);
    return isBlocked;
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

    const QString windowTitle = QString::fromWCharArray(title).trimmed();
    const QString processLabel = QString::fromWCharArray(processName).trimmed();

    QString baseLabel = windowTitle;
    if (baseLabel.isEmpty()) {
        baseLabel = processLabel;
    }
    if (baseLabel.isEmpty()) {
        baseLabel = QStringLiteral("알 수 없는 창");
    }

    QString text = baseLabel;
    if (!processLabel.isEmpty() && !baseLabel.contains(processLabel, Qt::CaseInsensitive)) {
        text += QStringLiteral(" (%1)").arg(processLabel);
    }

    if (toriyomi::win::HasSignificantOcclusion(hwnd, 0.25)) {
        text = QStringLiteral("[가려짐] %1").arg(text);
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

        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        if (processId == 0 || processId == ctx->selfPid) {
            return TRUE;
        }

        if (IsBlockedProcess(processId)) {
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
