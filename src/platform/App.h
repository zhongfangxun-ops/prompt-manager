#pragma once
#include <windows.h>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <vector>
inline constexpr UINT OpenMessage = WM_APP + 21;
inline constexpr UINT ExitMessage = WM_APP + 22;
inline constexpr UINT EdgeMessage = WM_APP + 23;
inline constexpr UINT RefreshEdge = WM_APP + 24;
inline std::wstring envValue(const wchar_t *key) {
    DWORD n = GetEnvironmentVariableW(key, nullptr, 0);
    if (!n)
        return {};
    std::wstring value(n, 0);
    value.resize(GetEnvironmentVariableW(key, value.data(), n));
    return value;
}
inline std::filesystem::path dataDir() {
    auto test = envValue(L"PROMPT_MANAGER_PROFILE");
    return test.empty() ? std::filesystem::path(envValue(L"LOCALAPPDATA")) / L"PromptManager"
                        : std::filesystem::path(test);
}
inline std::wstring suffix() {
    auto profile = envValue(L"PROMPT_MANAGER_PROFILE");
    return profile.empty() ? L"" : L"." + std::to_wstring(std::hash<std::wstring>{}(profile));
}
inline std::wstring settingsKey() { return L"Software\\PromptManager" + suffix(); }
inline std::wstring readSetting(const wchar_t *key, std::wstring fallback = {}) {
    wchar_t value[1024]{};
    DWORD bytes = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, settingsKey().c_str(), key, RRF_RT_REG_SZ, nullptr, value,
                        &bytes) == ERROR_SUCCESS
               ? value
               : fallback;
}
inline void writeSetting(const wchar_t *key, const std::wstring &value) {
    RegSetKeyValueW(HKEY_CURRENT_USER, settingsKey().c_str(), key, REG_SZ, value.c_str(),
                    (value.size() + 1) * sizeof(wchar_t));
}
inline std::wstring wideError(const std::exception &e) {
    std::string s = e.what();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    if (n)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
HWND openLibrary(HWND shell);
bool libraryKeys(HWND library, MSG &message);
int runDesktop();
void releaseLibrary();
