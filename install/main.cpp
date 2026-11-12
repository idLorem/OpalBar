#include <windows.h>
#include <shlobj.h>
#include <string>
#include "../shared/opal_app.h"
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

static bool Unpack(int id, const std::wstring& path)
{
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    HGLOBAL h = r ? LoadResource(nullptr, r) : nullptr;
    if (!h) return false;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD n = 0, size = SizeofResource(nullptr, r);
    bool ok = WriteFile(f, LockResource(h), size, &n, nullptr) && n == size;
    CloseHandle(f);
    return ok;
}

static void Shortcut(const std::wstring& target, const std::wstring& dir)
{
    wchar_t desk[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desk))) return;
    CoInitialize(nullptr);
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) {
        link->SetPath(target.c_str());
        link->SetWorkingDirectory(dir.c_str());
        link->SetDescription(L"OpalBar Settings");
        IPersistFile* file = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
            file->Save((std::wstring(desk) + L"\\OpalBar.lnk").c_str(), TRUE);
            file->Release();
        }
        link->Release();
    }
    CoUninitialize();
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd, int)
{
    bool update = wcsstr(cmd, L"/update") != nullptr;
    wchar_t local[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, local))) return 1;
    std::wstring dir = std::wstring(local) + L"\\OpalBar\\";
    std::wstring loader = dir + app::LoaderExe, dll = dir + app::TapDll, settings = dir + app::SettingsExe, ini = dir + L"config.ini";
    bool fresh = GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES;

    app::StopLoader();
    bool shell = app::KillByName(L"GlassBar.exe") > 0;
    app::KillByName(L"GlassBarSettings.exe");
    app::KillByName(app::SettingsExe);
    Sleep(500);
    app::KillByName(app::LoaderExe);
    Sleep(200);

    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring old = dll + L".old";
    DeleteFileW(old.c_str());
    if (GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(dll.c_str())) {
        MoveFileExW(dll.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
        shell = true;
    }
    bool ok = Unpack(1, loader) && Unpack(2, dll) && Unpack(3, settings) && (!fresh || Unpack(4, ini));
    if (!ok) {
        MoveFileExW(old.c_str(), dll.c_str(), MOVEFILE_REPLACE_EXISTING);
        MessageBoxW(nullptr, (L"Could not write the files to " + dir).c_str(), app::Name, MB_OK | MB_ICONERROR);
        return 1;
    }

    HKEY run;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, app::RunKey, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &run) == ERROR_SUCCESS) {
        bool had = RegQueryValueExW(run, L"GlassBar", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        bool has = RegQueryValueExW(run, app::Name, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        RegDeleteValueW(run, L"GlassBar");
        if (fresh || had || has) {
            std::wstring v = L"\"" + loader + L"\"";
            RegSetValueExW(run, app::Name, 0, REG_SZ, (const BYTE*)v.c_str(), DWORD((v.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(run);
    }
    Shortcut(settings, dir);

    if (shell) app::RestartShell();
    ShellExecuteW(nullptr, nullptr, loader.c_str(), nullptr, dir.c_str(), SW_HIDE);
    ShellExecuteW(nullptr, nullptr, settings.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
    if (!update)
        MessageBoxW(nullptr, L"OpalBar is installed.\nOpen its settings with the OpalBar shortcut on the desktop.", app::Name, MB_OK | MB_ICONINFORMATION);
    return 0;
}
