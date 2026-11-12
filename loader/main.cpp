#include <windows.h>
#include <map>
#include <set>
#include <string>
#include "../shared/opal_app.h"

static HWND WindowOf(const wchar_t* exeName)
{
    struct Ctx { const wchar_t* name; HWND found; } ctx{ exeName, nullptr };
    EnumWindows([](HWND w, LPARAM l) -> BOOL {
        auto& c = *(Ctx*)l;
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!p) return TRUE;
        wchar_t path[MAX_PATH]; DWORD n = MAX_PATH;
        bool match = QueryFullProcessImageNameW(p, 0, path, &n) && !_wcsicmp(wcsrchr(path, L'\\') + 1, c.name);
        CloseHandle(p);
        if (match) c.found = w;
        return !match;
    }, (LPARAM)&ctx);
    return ctx.found;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    dir = dir.substr(0, dir.rfind(L'\\') + 1);
    std::wstring dll = dir + app::TapDll, ini = dir + L"config.ini";

    if (!GetPrivateProfileIntW(L"App", L"Enabled", 1, ini.c_str())) return 0;

    CreateMutexW(nullptr, TRUE, app::LoaderMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, app::StopEvent);

    HMODULE mod = LoadLibraryW(dll.c_str());
    auto hookProc = mod ? (HOOKPROC)GetProcAddress(mod, "GetMsgProc") : nullptr;
    if (!hookProc) return 1;
    std::set<DWORD> injected;
    std::map<DWORD, HANDLE> pending;
    for (;;) {
        for (HWND w : { FindWindowW(L"Shell_TrayWnd", nullptr), WindowOf(L"ShellHost.exe") }) {
            DWORD pid = 0, tid = w ? GetWindowThreadProcessId(w, &pid) : 0;
            if (!tid || injected.count(pid)) continue;
            HANDLE& ready = pending[pid];
            if (!ready) ready = CreateEventW(nullptr, FALSE, FALSE, (app::ReadyPrefix + std::to_wstring(pid)).c_str());
            if (!ready) continue;
            bool done = WaitForSingleObject(ready, 0) == WAIT_OBJECT_0;
            if (!done) {
                HHOOK hook = SetWindowsHookExW(WH_GETMESSAGE, hookProc, mod, tid);
                PostMessageW(w, WM_NULL, 0, 0);
                HANDLE both[] = { ready, stop };
                DWORD r = hook ? WaitForMultipleObjects(2, both, FALSE, 10000) : WAIT_TIMEOUT;
                if (hook) UnhookWindowsHookEx(hook);
                if (r == WAIT_OBJECT_0 + 1) return 0;
                done = r == WAIT_OBJECT_0;
            }
            if (done) {
                injected.insert(pid);
                CloseHandle(ready);
                pending.erase(pid);
            }
        }
        auto alive = [](DWORD pid) {
            HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
            bool a = p && WaitForSingleObject(p, 0) == WAIT_TIMEOUT;
            if (p) CloseHandle(p);
            return a;
        };
        for (auto it = injected.begin(); it != injected.end();) it = alive(*it) ? std::next(it) : injected.erase(it);
        for (auto it = pending.begin(); it != pending.end();) {
            if (alive(it->first)) { ++it; continue; }
            if (it->second) CloseHandle(it->second);
            it = pending.erase(it);
        }
        if (WaitForSingleObject(stop, 3000) == WAIT_OBJECT_0) return 0;
    }
}
