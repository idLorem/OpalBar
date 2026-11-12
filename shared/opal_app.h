#pragma once
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

namespace app {
inline constexpr wchar_t Name[] = L"OpalBar";
inline constexpr wchar_t Version[] = L"1.1.0";
inline constexpr wchar_t LoaderExe[] = L"OpalBar.exe", TapDll[] = L"OpalTap.dll", SettingsExe[] = L"OpalBarSettings.exe",
                         InstallerExe[] = L"OpalBarInstall.exe";
inline constexpr wchar_t LoaderMutex[] = L"Local\\OpalBarLoader";
inline constexpr wchar_t StopEvent[] = L"Local\\OpalBarStop";
inline constexpr wchar_t ReadyPrefix[] = L"Local\\OpalBarReady_";
inline constexpr wchar_t TrayClass[] = L"OpalBarTray";
inline constexpr UINT TrayChanged = WM_APP + 2;
inline constexpr UINT ShellReload = WM_APP + 3;
inline constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

inline int KillByName(const wchar_t* exe)
{
    int n = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e{ sizeof(e) };
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
        if (_wcsicmp(e.szExeFile, exe) || e.th32ProcessID == GetCurrentProcessId()) continue;
        if (HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, e.th32ProcessID)) {
            n += TerminateProcess(p, 1) != 0;
            CloseHandle(p);
        }
    }
    CloseHandle(snap);
    return n;
}

inline void StopLoader()
{
    if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, StopEvent)) { SetEvent(e); CloseHandle(e); }
}

inline void RestartShell()
{
    KillByName(L"ShellHost.exe");
    KillByName(L"explorer.exe");
    Sleep(1500);
    for (int i = 0; i < 16 && !FindWindowW(L"Shell_TrayWnd", nullptr); i++) Sleep(250);
    if (!FindWindowW(L"Shell_TrayWnd", nullptr)) ShellExecuteW(nullptr, nullptr, L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
}

}
