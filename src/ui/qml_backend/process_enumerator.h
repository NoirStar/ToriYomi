#pragma once

#include <QString>
#include <vector>
#include <windows.h>

namespace toriyomi {
namespace ui {

struct ProcessInfo {
    QString displayText;
    HWND windowHandle = nullptr;
};

class ProcessEnumerator {
public:
    static std::vector<ProcessInfo> EnumerateVisibleWindows(DWORD currentProcessId);
};

}  // namespace ui
}  // namespace toriyomi
