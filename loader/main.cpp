#include <windows.h>
#include <shellapi.h>
#include <map>
#include <set>
#include <string>
#include <thread>
#include "../shared/opal_app.h"

static std::wstring g_dir, g_ini;
static HMODULE g_mod;
static HOOKPROC g_hook;
static HANDLE g_stop;
static NOTIFYICONDATAW g_nid{ sizeof(g_nid) };
static UINT g_taskbarCreated;
constexpr UINT WM_TRAY = WM_APP + 1;
enum { cmdSettings = 1, cmdEnabled, cmdReload, cmdHide };

static bool Enabled() { return GetPrivateProfileIntW(L"App", L"Enabled", 1, g_ini.c_str()) != 0; }

static bool TrayOn() { return GetPrivateProfileIntW(L"App", L"TrayIcon", 1, g_ini.c_str()) != 0; }

static void ShowIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (TrayOn()) Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static bool Russian()
{
    wchar_t b[8];
    GetPrivateProfileStringW(L"App", L"Language", L"en", b, 8, g_ini.c_str());
    return !wcscmp(b, L"ru");
}

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

static void Inject()
{
    std::set<DWORD> injected;
    std::map<DWORD, HANDLE> pending;
    for (;;) {
        for (HWND w : { FindWindowW(L"Shell_TrayWnd", nullptr), WindowOf(L"ShellHost.exe") }) {
            if (!Enabled()) break;
            DWORD pid = 0, tid = w ? GetWindowThreadProcessId(w, &pid) : 0;
            if (!tid || injected.count(pid)) continue;
            HANDLE& ready = pending[pid];
            if (!ready) ready = CreateEventW(nullptr, FALSE, FALSE, (app::ReadyPrefix + std::to_wstring(pid)).c_str());
            if (!ready) continue;
            bool done = WaitForSingleObject(ready, 0) == WAIT_OBJECT_0;
            if (!done) {
                HHOOK hook = SetWindowsHookExW(WH_GETMESSAGE, g_hook, g_mod, tid);
                PostMessageW(w, WM_NULL, 0, 0);
                HANDLE both[] = { ready, g_stop };
                DWORD r = hook ? WaitForMultipleObjects(2, both, FALSE, 10000) : WAIT_TIMEOUT;
                if (hook) UnhookWindowsHookEx(hook);
                if (r == WAIT_OBJECT_0 + 1) return;
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
        if (WaitForSingleObject(g_stop, 3000) == WAIT_OBJECT_0) return;
    }
}

static void Restart() { std::thread([] { app::RestartShell(); }).detach(); }

static void OpenSettings()
{
    ShellExecuteW(nullptr, nullptr, (g_dir + app::SettingsExe).c_str(), nullptr, g_dir.c_str(), SW_SHOWNORMAL);
}

static void Menu(HWND w)
{
    bool ru = Russian(), on = Enabled();
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, cmdSettings, ru ? L"Настройки" : L"Settings");
    AppendMenuW(m, MF_STRING | (on ? MF_CHECKED : 0), cmdEnabled, ru ? L"Включён" : L"Enabled");
    AppendMenuW(m, MF_STRING, cmdReload, ru ? L"Перезапустить" : L"Reload");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, cmdHide, ru ? L"Скрыть" : L"Hide");
    SetMenuDefaultItem(m, cmdSettings, FALSE);
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(w);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, w, nullptr);
    PostMessageW(w, WM_NULL, 0, 0);
    DestroyMenu(m);
    if (cmd == cmdSettings) OpenSettings();
    else if (cmd == cmdReload) Restart();
    else if (cmd == cmdHide) {
        WritePrivateProfileStringW(L"App", L"TrayIcon", L"0", g_ini.c_str());
        ShowIcon();
    }
    else if (cmd == cmdEnabled) {
        WritePrivateProfileStringW(L"App", L"Enabled", on ? L"0" : L"1", g_ini.c_str());
        if (on) Restart();
    }
}

static LRESULT CALLBACK TrayProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TRAY) {
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) Menu(w);
        else if (lp == WM_LBUTTONUP) OpenSettings();
        return 0;
    }
    if (msg == app::ShellReload) {
        Restart();
        return 0;
    }
    if (msg == g_taskbarCreated || msg == app::TrayChanged) {
        ShowIcon();
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_dir = exe;
    g_dir = g_dir.substr(0, g_dir.rfind(L'\\') + 1);
    g_ini = g_dir + L"config.ini";

    CreateMutexW(nullptr, TRUE, app::LoaderMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    g_stop = CreateEventW(nullptr, TRUE, FALSE, app::StopEvent);

    g_mod = LoadLibraryW((g_dir + app::TapDll).c_str());
    g_hook = g_mod ? (HOOKPROC)GetProcAddress(g_mod, "GetMsgProc") : nullptr;
    if (!g_hook || !g_stop) return 1;
    std::thread(Inject).detach();

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayProc;
    wc.hInstance = inst;
    wc.lpszClassName = app::TrayClass;
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, app::Name, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    g_nid.hWnd = w;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wcscpy_s(g_nid.szTip, app::Name);
    ShowIcon();

    for (;;) {
        if (MsgWaitForMultipleObjects(1, &g_stop, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0) break;
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    return 0;
}
