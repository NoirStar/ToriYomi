#pragma once

#include <Windows.h>

namespace toriyomi::win {

inline double ClampDouble(double value, double minValue, double maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

inline bool IsOccludingCandidate(HWND hwnd, HWND target) {
    if (!hwnd || hwnd == target) {
        return false;
    }

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return false;
    }

    if (GetAncestor(hwnd, GA_ROOTOWNER) == target) {
        return false;
    }

    const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TRANSPARENT) != 0) {
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }

    if ((rect.right - rect.left) <= 0 || (rect.bottom - rect.top) <= 0) {
        return false;
    }

    return true;
}

inline double ComputeOcclusionRatio(HWND target, double intersectionMultiplier = 1.0) {
    if (!target || !IsWindow(target)) {
        return 0.0;
    }

    RECT targetRect{};
    if (!GetWindowRect(target, &targetRect)) {
        return 0.0;
    }

    const LONG rawWidth = targetRect.right - targetRect.left;
    const LONG rawHeight = targetRect.bottom - targetRect.top;
    const LONG width = rawWidth > 1 ? rawWidth : 1;
    const LONG height = rawHeight > 1 ? rawHeight : 1;
    const double targetArea = static_cast<double>(width) * static_cast<double>(height);

    double maxCoverage = 0.0;
    for (HWND walker = GetWindow(target, GW_HWNDPREV); walker != nullptr; walker = GetWindow(walker, GW_HWNDPREV)) {
        if (!IsOccludingCandidate(walker, target)) {
            continue;
        }

        RECT walkerRect{};
        if (!GetWindowRect(walker, &walkerRect)) {
            continue;
        }

        RECT intersection{};
        if (!IntersectRect(&intersection, &targetRect, &walkerRect)) {
            continue;
        }

    const LONG iRawWidth = intersection.right - intersection.left;
    const LONG iRawHeight = intersection.bottom - intersection.top;
    const LONG iWidth = iRawWidth > 0 ? iRawWidth : 0;
    const LONG iHeight = iRawHeight > 0 ? iRawHeight : 0;
        const double intersectionArea = static_cast<double>(iWidth) * static_cast<double>(iHeight) * intersectionMultiplier;

        const double coverage = intersectionArea / targetArea;
        if (coverage > maxCoverage) {
            maxCoverage = coverage;
        }

        if (maxCoverage >= 1.0) {
            break;
        }
    }

    return ClampDouble(maxCoverage, 0.0, 1.0);
}

inline bool HasSignificantOcclusion(HWND target, double threshold = 0.2) {
    return ComputeOcclusionRatio(target) >= ClampDouble(threshold, 0.01, 1.0);
}

}  // namespace toriyomi::win
