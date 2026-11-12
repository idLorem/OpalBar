#define GDIPVER 0x0110
#define NOMINMAX
#include <algorithm>
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <dwmapi.h>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <urlmon.h>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "../shared/opal_config.h"
#include "../shared/opal_app.h"
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
using namespace Gdiplus;

enum Kind { kSlider, kToggle, kColor, kButton };
struct Item {
    Kind kind; const wchar_t* key; const wchar_t* label;
    double min, max, step, def;
    const wchar_t* fmt; double shown;
    double v = 0; RectF box{}; bool enabled = true;
};
static Item Slider(cfg::Range r, const wchar_t* label, double step, const wchar_t* fmt, double shown)
{
    return { kSlider, r.key, label, r.min, r.max, step, r.def, fmt, shown };
}
static bool g_ru = false;
static const wchar_t* const kRu[][2] = {
    { L"Fit width to icons", L"Ширина по иконкам" },
    { L"Full screen width", L"На всю ширину экрана" },
    { L"Fixed width", L"Фиксированная ширина" },
    { L"Padding around icons", L"Поля вокруг иконок" },
    { L"Corner radius", L"Скругление углов" },
    { L"Button highlight radius", L"Скругление подсветки кнопок" },
    { L"Blur", L"Размытие" },
    { L"Edge refraction", L"Преломление по краю" },
    { L"Top highlight", L"Блик сверху" },
    { L"Bottom glow", L"Свечение снизу" },
    { L"Tint density", L"Плотность оттенка" },
    { L"Tint color", L"Цвет оттенка" },
    { L"Auto", L"Авто" },
    { L"Start with Windows", L"Запускать вместе с Windows" },
    { L"Export", L"Экспорт" },
    { L"Import", L"Импорт" },
    { L"Reset", L"Сбросить" },
    { L"Wallpaper", L"Обои" },
    { L"Image", L"Картинка" },
    { L"Color", L"Цвет" },
    { L"Check for updates", L"Проверить обновления" },
    { L"Presets", L"Пресеты" },
    { L"Enabled", L"Включён" },
    { L"Disabled", L"Выключен" },
    { L"Panel tint density", L"Плотность оттенка панелей" },
    { L"Vertical offset", L"Сдвиг по вертикали" },
    { L"Bar height", L"Высота панели" },
    { L"Paste a settings code here", L"Вставьте сюда код настроек" },
    { L"Apply", L"Применить" },
    { L"Preset ", L"Пресет " },
    { L"All 8 slots are taken, delete a preset first", L"Все 8 мест заняты, удалите ненужный пресет" },
    { L"Preset saved: ", L"Сохранён пресет: " },
    { L"Preset deleted: ", L"Удалён пресет: " },
    { L"This preset is damaged", L"Этот пресет повреждён" },
    { L"Updates are not connected yet", L"Обновления пока не подключены" },
    { L"Downloading the update…", L"Скачиваю обновление…" },
    { L"Checking for updates…", L"Проверяю обновления…" },
    { L"Saver", L"Экономия" },
    { L"Geometry", L"Геометрия" },
    { L"Glass", L"Стекло" },
    { L"Background", L"Фон" },
    { L"Save current settings", L"Сохранить текущие настройки" },
    { L"Preset name, then Enter", L"Имя пресета, затем Enter" },
    { L"OpalBar is on", L"OpalBar включён" },
    { L"OpalBar is off, Explorer is restarting", L"OpalBar выключен, проводник перезапускается" },
    { L"Code copied to the clipboard", L"Код скопирован в буфер обмена" },
    { L"Settings from the code applied", L"Настройки из кода применены" },
    { L"No OpalBar code in the clipboard", L"В буфере обмена нет кода OpalBar" },
    { L"This is not an OpalBar code", L"Это не код OpalBar" },
    { L"Images\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff\0", L"Изображения\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff\0" },
    { L"Could not check for updates", L"Не удалось проверить обновления" },
    { L"You have the latest version", L"У вас последняя версия" },
    { L"No installer in the new version ", L"В новой версии нет установщика: " },
    { L"Install update", L"Установить обновление" },
    { L"New version available: ", L"Доступна версия " },
    { L"Could not download the update", L"Не удалось скачать обновление" },
    { L"OpalBar Settings", L"Настройки OpalBar" },
    { L"Details", L"Детали" },
    { L"Glide animation", L"Анимация сдвига" },
    { L"Rim color", L"Цвет ободка" },
    { L"Rim width", L"Толщина ободка" },
    { L"Shadow", L"Тень" },
    { L"Hover color", L"Цвет подсветки" },
    { L"Hover brightness", L"Яркость подсветки" },
    { L"Running indicator", L"Индикатор запущенных" },
    { L"Indicator color", L"Цвет индикатора" },
    { L"Clock size", L"Размер часов" },
    { L"Bold clock", L"Жирные часы" },
    { L"Clock color", L"Цвет часов" },
    { L"Reload", L"Перезапуск" },
    { L"Uninstall", L"Удалить" },
    { L"Tray icon", L"Значок в трее" },
    { L"Restarting Explorer…", L"Перезапускаю проводник…" },
    { L"Remove OpalBar from this PC? Your settings and presets will be deleted. Explorer will restart.",
      L"Удалить OpalBar с этого компьютера? Настройки и пресеты будут удалены. Проводник перезапустится." },
    { L"%.0f ms", L"%.0f мс" },
};
static const wchar_t* Tr(const wchar_t* en)
{
    if (g_ru) for (auto& p : kRu) if (!wcscmp(p[0], en)) return p[1];
    return en;
}

static std::vector<Item> g_items = {
    { kToggle, cfg::DynamicWidth.key, L"Fit width to icons", 0, 1, 1, cfg::DynamicWidth.def, nullptr, 1 },
    { kToggle, cfg::FullWidth.key, L"Full screen width", 0, 1, 1, cfg::FullWidth.def, nullptr, 1 },
    Slider(cfg::Width, L"Fixed width", 10, L"%.0f px", 1),
    Slider(cfg::Padding, L"Padding around icons", 1, L"%.0f px", 1),
    Slider(cfg::CornerRadius, L"Corner radius", 1, L"%.0f px", 1),
    Slider(cfg::HighlightRadius, L"Button highlight radius", 1, L"%.0f px", 1),
    Slider(cfg::BlurAmount, L"Blur", 0.1, L"%.1f", 1),
    Slider(cfg::Refraction, L"Edge refraction", 0.01, L"%.1f px", 10),
    Slider(cfg::Specular, L"Top highlight", 0.01, L"%.0f %%", 100),
    Slider(cfg::BorderGlow, L"Bottom glow", 0.01, L"%.0f %%", 100),
    Slider(cfg::TintOpacity, L"Tint density", 0.01, L"%.0f %%", 100),
    { kColor, cfg::TintColorKey, L"Tint color", 0, 0, 0, cfg::TintColorDef, nullptr, 1 },
    { kButton, nullptr, L"Auto", 0, 0, 0, 0, nullptr, 1 },
    { kToggle, nullptr, L"Start with Windows", 0, 1, 1, 0, nullptr, 1 },
    { kButton, nullptr, L"Export", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Import", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Reset", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Wallpaper", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Image", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Color", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Check for updates", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Presets", 0, 0, 0, 0, nullptr, 1 },
    { kToggle, nullptr, L"Enabled", 0, 1, 1, 1, nullptr, 1 },
    Slider(cfg::PanelTintOpacity, L"Panel tint density", 0.01, L"%.0f %%", 100),
    Slider(cfg::OffsetY, L"Vertical offset", 1, L"%.0f px", 1),
    { kButton, nullptr, L"Paste a settings code here", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Apply", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Русский", 0, 0, 0, 0, nullptr, 1 },
    { kSlider, nullptr, L"Bar height", 48 - 2 * cfg::MarginV.max, 48 - 2 * cfg::MarginV.min, 2, 48 - 2 * cfg::MarginV.def, L"%.0f px", 1 },
    { kButton, nullptr, L"Reload", 0, 0, 0, 0, nullptr, 1 },
    { kButton, nullptr, L"Uninstall", 0, 0, 0, 0, nullptr, 1 },
    Slider(cfg::AnimationTime, L"Glide animation", 10, L"%.0f ms", 1),
    { kColor, cfg::RimColorKey, L"Rim color", 0, 0, 0, cfg::RimColorDef, nullptr, 1 },
    Slider(cfg::RimWidth, L"Rim width", 0.1, L"%.1f px", 1),
    Slider(cfg::Shadow, L"Shadow", 0.01, L"%.0f %%", 100),
    { kColor, cfg::HoverColorKey, L"Hover color", 0, 0, 0, cfg::Stock, nullptr, 1 },
    Slider(cfg::HoverStrength, L"Hover brightness", 0.05, L"%.0f %%", 100),
    { kToggle, cfg::ShowIndicator.key, L"Running indicator", 0, 1, 1, cfg::ShowIndicator.def, nullptr, 1 },
    { kColor, cfg::IndicatorColorKey, L"Indicator color", 0, 0, 0, cfg::Stock, nullptr, 1 },
    Slider(cfg::ClockSize, L"Clock size", 0.05, L"%.0f %%", 100),
    { kToggle, cfg::ClockBold.key, L"Bold clock", 0, 1, 1, cfg::ClockBold.def, nullptr, 1 },
    { kColor, cfg::ClockColorKey, L"Clock color", 0, 0, 0, cfg::Stock, nullptr, 1 },
    { kToggle, nullptr, L"Tray icon", 0, 1, 1, 1, nullptr, 1 },
};
enum { iDynamic, iFull, iWidth, iPadding, iRadius, iHl, iBlur, iRefraction, iSpecular, iGlow, iTintOpacity, iTint, iAuto,
       iAutostart, iExport, iImport, iReset, iBgWall, iBgImage, iBgColor, iUpdate, iPresets, iEnabled, iPanelTint, iOffset,
       iCode, iApply, iLang, iHeight, iReload, iUninstall, iAnim, iRimColor, iRimWidth, iShadow, iHoverColor, iHover,
       iIndicator, iIndColor, iClockSize, iClockBold, iClockColor, iTray, iCount };
static_assert(iCount <= 64, "g_pending is a 64-bit mask");
static const int kCoded[] = { iDynamic, iFull, iWidth, iPadding, iRadius, iHl, iBlur, iRefraction, iSpecular, iGlow,
                              iTintOpacity, iTint, iPanelTint, iOffset, iHeight, iAnim, iRimColor, iRimWidth, iShadow,
                              iHoverColor, iHover, iIndicator, iIndColor, iClockSize, iClockBold, iClockColor };
constexpr int kFirstOptional = 12;

static std::wstring g_dir, g_ini;
static float g_s = 1, g_fit = 1;
static int g_focus = 0, g_hover = -1, g_drag = -1;
static bool g_keyboard = false;
static unsigned long long g_pending = 0;
static std::unique_ptr<Bitmap> g_strip;
static std::unique_ptr<Bitmap> g_base, g_back;
static std::wstring g_toast;
static HWND g_edit;
constexpr int kMaxCode = 64;
constexpr COLORREF kWell = RGB(26, 26, 28);
static std::wstring CodeText() { wchar_t b[kMaxCode + 1] = {}; GetWindowTextW(g_edit, b, kMaxCode + 1); return b; }
static std::unique_ptr<Bitmap> g_logo;

static float S(float dip) { return dip * g_s; }
static double& V(int i) { return g_items[i].v; }
static double MarginV() { return (48 - V(iHeight)) / 2; }

static std::wstring ToHex(const std::wstring& s)
{
    std::wstring h; wchar_t b[8];
    for (wchar_t c : s) { swprintf_s(b, L"%04X", (unsigned)c); h += b; }
    return h;
}
static std::wstring FromHex(const std::wstring& h)
{
    std::wstring s;
    for (size_t i = 0; i + 4 <= h.size(); i += 4) s += (wchar_t)wcstoul(h.substr(i, 4).c_str(), nullptr, 16);
    return s;
}

static void Load()
{
    for (auto& it : g_items) {
        if (!it.key) continue;
        wchar_t b[64];
        GetPrivateProfileStringW(L"Taskbar", it.key, L"", b, 64, g_ini.c_str());
        if (it.kind == kColor) it.v = *b ? (double)(wcstoul(b + (*b == L'#'), nullptr, 16) & 0xFFFFFF) : it.def;
        else it.v = (*b ? std::clamp(_wtof(b), it.min, it.max) : it.def) + 0.0;
    }
    V(iAutostart) = RegGetValueW(HKEY_CURRENT_USER, app::RunKey, app::Name, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    V(iEnabled) = GetPrivateProfileIntW(L"App", L"Enabled", 1, g_ini.c_str()) != 0;
    V(iTray) = GetPrivateProfileIntW(L"App", L"TrayIcon", 1, g_ini.c_str()) != 0;
    wchar_t b[32];
    GetPrivateProfileStringW(L"Taskbar", cfg::MarginV.key, L"", b, 32, g_ini.c_str());
    V(iHeight) = 48 - 2 * std::round(*b ? std::clamp(_wtof(b), cfg::MarginV.min, cfg::MarginV.max) : cfg::MarginV.def);
}

static void ReloadShell();

static void WriteItem(int i)
{
    auto& it = g_items[i];
    wchar_t b[32];
    if (i == iAutostart) {
        HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, app::RunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
        if (it.v) {
            std::wstring v = L"\"" + g_dir + app::LoaderExe + L"\"";
            RegSetValueExW(k, app::Name, 0, REG_SZ, (const BYTE*)v.c_str(), DWORD((v.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(k, app::Name);
        }
        RegCloseKey(k);
        return;
    }
    if (i == iEnabled) {
        WritePrivateProfileStringW(L"App", L"Enabled", it.v ? L"1" : L"0", g_ini.c_str());
        if (it.v) ShellExecuteW(nullptr, nullptr, (g_dir + app::LoaderExe).c_str(), nullptr, g_dir.c_str(), SW_HIDE);
        else ReloadShell();
        return;
    }
    if (i == iTray) {
        WritePrivateProfileStringW(L"App", L"TrayIcon", it.v ? L"1" : L"0", g_ini.c_str());
        if (HWND t = FindWindowW(app::TrayClass, nullptr)) PostMessageW(t, app::TrayChanged, 0, 0);
        return;
    }
    if (i == iHeight) {
        swprintf_s(b, L"%g", MarginV());
        WritePrivateProfileStringW(L"Taskbar", cfg::MarginV.key, b, g_ini.c_str());
        return;
    }
    if (!it.key) return;
    if (it.kind == kColor && it.v < 0) { WritePrivateProfileStringW(L"Taskbar", it.key, nullptr, g_ini.c_str()); return; }
    if (it.kind == kColor) swprintf_s(b, L"#%06X", (unsigned)it.v);
    else swprintf_s(b, L"%g", it.v);
    WritePrivateProfileStringW(L"Taskbar", it.key, b, g_ini.c_str());
}

static void Change(HWND w, int i, double v)
{
    auto& it = g_items[i];
    if (it.kind == kSlider) v = std::clamp(std::round(v / it.step) * it.step, it.min, it.max);
    if (i == iOffset) v = std::clamp(v, -MarginV(), MarginV());
    v += 0.0;
    if (v == it.v) return;
    it.v = v;
    g_pending |= 1ull << i;
    SetTimer(w, 1, 120, nullptr);
    InvalidateRect(w, nullptr, FALSE);
    if (i == iHeight) Change(w, iOffset, V(iOffset));
}

constexpr wchar_t kCodePrefix[] = L"GB1-";

static std::wstring Encode()
{
    std::vector<BYTE> d;
    for (int i : kCoded) {
        auto& it = g_items[i];
        if (it.kind == kSlider) { auto n = (unsigned)std::lround((it.v - it.min) / it.step); d.push_back(BYTE(n >> 8)); d.push_back(BYTE(n)); }
        else if (it.kind == kToggle) d.push_back(it.v != 0);
        else {
            auto c = (unsigned)std::max(0.0, it.v);
            if (it.def < 0) d.push_back(it.v < 0);
            d.push_back(BYTE(c >> 16)); d.push_back(BYTE(c >> 8)); d.push_back(BYTE(c));
        }
    }
    BYTE sum = 0x5A;
    for (BYTE b : d) sum = BYTE(sum * 31 + b);
    d.push_back(sum);
    wchar_t out[256]; DWORD n = 256;
    CryptBinaryToStringW(d.data(), (DWORD)d.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out, &n);
    return kCodePrefix + std::wstring(out, n);
}

static bool Parse(std::wstring code, std::vector<double>& vals)
{
    vals.clear();
    std::erase_if(code, [](wchar_t c) { return c <= L' '; });
    if (code.rfind(kCodePrefix, 0) != 0) return false;
    BYTE d[128]; DWORD n = sizeof(d);
    if (!CryptStringToBinaryW(code.c_str() + wcslen(kCodePrefix), 0, CRYPT_STRING_BASE64, d, &n, nullptr, nullptr) || n < 2) return false;
    BYTE sum = 0x5A;
    for (DWORD i = 0; i + 1 < n; i++) sum = BYTE(sum * 31 + d[i]);
    if (sum != d[n - 1]) return false;
    DWORD at = 0;
    for (int k = 0; k < (int)std::size(kCoded); k++) {
        int i = kCoded[k];
        auto& it = g_items[i];
        if (at == n - 1 && k >= kFirstOptional) {
            vals.push_back(i == iPanelTint ? vals[std::find(std::begin(kCoded), std::end(kCoded), iTintOpacity) - std::begin(kCoded)] : it.def);
            continue;
        }
        bool autoColor = it.kind == kColor && it.def < 0;
        DWORD need = it.kind == kSlider ? 2 : it.kind == kToggle ? 1 : autoColor ? 4 : 3;
        if (at + need > n - 1) return false;
        if (it.kind == kSlider) vals.push_back(it.min + ((d[at] << 8) | d[at + 1]) * it.step);
        else if (it.kind == kToggle) vals.push_back(d[at] != 0);
        else if (autoColor && d[at]) vals.push_back(-1);
        else { BYTE* c = d + at + autoColor; vals.push_back((c[0] << 16) | (c[1] << 8) | c[2]); }
        at += need;
    }
    return at == n - 1;
}

static bool Decode(HWND w, const std::wstring& code)
{
    std::vector<double> vals;
    if (!Parse(code, vals)) return false;
    for (size_t k = vals.size(); k-- > 0;) Change(w, kCoded[k], vals[k]);
    return true;
}

static bool SameAsCurrent(const std::wstring& code)
{
    std::vector<double> vals;
    if (!Parse(code, vals)) return false;
    for (size_t k = 0; k < vals.size(); k++)
        if (std::abs(vals[k] - V(kCoded[k])) > 1e-9) return false;
    return true;
}

constexpr wchar_t kUpdateRepo[] = L"idLorem/OpalBar";
constexpr UINT WM_UPDATE = WM_APP;
constexpr UINT WM_UPDATE_READY = WM_APP + 1;
struct Update { bool ok = false; std::wstring version, installer; };
static std::wstring g_updateUrl;
static bool g_updating = false;

static bool Newer(const std::wstring& a, const std::wstring& b)
{
    auto next = [](const wchar_t*& p) {
        while (*p && !iswdigit(*p)) p++;
        wchar_t* e; unsigned long n = wcstoul(p, &e, 10); p = e;
        return n;
    };
    const wchar_t *pa = a.c_str(), *pb = b.c_str();
    while (*pa || *pb) {
        auto x = next(pa), y = next(pb);
        if (x != y) return x > y;
    }
    return false;
}

static std::vector<std::wstring> JsonStrings(const std::string& json, const char* key)
{
    std::vector<std::wstring> out;
    const auto npos = std::string::npos;
    std::string k = "\"" + std::string(key) + "\"";
    for (auto at = json.find(k); at != npos; at = json.find(k, at + 1)) {
        auto c = json.find(':', at + k.size());
        auto a = c == npos ? npos : json.find('"', c);
        auto e = a == npos ? npos : json.find('"', a + 1);
        if (e == npos) break;
        out.emplace_back(json.begin() + a + 1, json.begin() + e);
    }
    return out;
}

static Update ParseRelease(const std::string& json, const std::wstring& repo)
{
    Update u;
    auto tags = JsonStrings(json, "tag_name");
    if (tags.empty()) return u;
    u.ok = true;
    u.version = tags[0];
    std::wstring prefix = L"https://github.com/" + repo + L"/releases/download/", suffix = std::wstring(L"/") + app::InstallerExe;
    for (auto& url : JsonStrings(json, "browser_download_url"))
        if (url.rfind(prefix, 0) == 0 && url.size() > suffix.size() && !url.compare(url.size() - suffix.size(), suffix.size(), suffix))
            u.installer = url;
    return u;
}

static bool OwnDir(const std::wstring& dir, const std::wstring& local)
{
    return !local.empty() && !_wcsicmp((local + L"\\" + app::Name + L"\\").c_str(), dir.c_str());
}

static void ReloadShell()
{
    if (HWND t = FindWindowW(app::TrayClass, nullptr)) PostMessageW(t, app::ShellReload, 0, 0);
    else std::thread(app::RestartShell).detach();
}

static bool BuiltInsValid();
static bool TemplateIsMain(const wchar_t* ini);

static int SelfTest(const wchar_t* tmpl)
{
    if (tmpl && !TemplateIsMain(tmpl)) return 11;
    for (int i : kCoded) V(i) = g_items[i].kind == kColor ? 0xA1B2C3 : g_items[i].max;
    V(iPanelTint) = 0.25;
    V(iHoverColor) = -1;
    auto code = Encode();
    std::vector<double> vals;
    if (!Parse(code, vals) || !SameAsCurrent(code)) return 1;
    V(iPanelTint) = 0.5;
    if (SameAsCurrent(code)) return 2;
    code[8] = code[8] == L'A' ? L'B' : L'A';
    if (Parse(code, vals)) return 3;
    if (!Newer(L"v1.10.0", L"1.9.3") || Newer(L"1.0.0", L"1.0") || Newer(L"0.9", L"1.0.0")) return 4;
    auto u = ParseRelease("{\"tag_name\":\"v1.2.0\",\"assets\":[{\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.2.0/notes.txt\"},"
                          "{\"browser_download_url\": \"https://github.com/o/r/releases/download/v1.2.0/OpalBarInstall.exe\"}]}", L"o/r");
    if (!u.ok || u.version != L"v1.2.0" || u.installer != L"https://github.com/o/r/releases/download/v1.2.0/OpalBarInstall.exe") return 5;
    if (!ParseRelease("{\"tag_name\":\"v2\",\"browser_download_url\":\"https://evil.example/OpalBarInstall.exe\"}", L"o/r").installer.empty() ||
        !ParseRelease("{\"tag_name\":\"v2\",\"browser_download_url\":\"https://github.com/x/y/releases/download/v2/OpalBarInstall.exe\"}", L"o/r").installer.empty() ||
        ParseRelease("oops", L"o/r").ok) return 6;
    auto at = [&](int i) { return vals[std::find(std::begin(kCoded), std::end(kCoded), i) - std::begin(kCoded)]; };
    if (!Parse(L"GB1-AQAAKQAMABQADgAKAA8ACgAAAB4pKSnY", vals) || vals.size() != std::size(kCoded) || std::abs(at(iPanelTint) - 0.3) > 1e-9 ||
        at(iOffset) != 0 || at(iHeight) != 40 || at(iAnim) != cfg::AnimationTime.def || at(iHoverColor) != -1 || at(iRimColor) != cfg::RimColorDef) return 7;
    std::wstring local = L"C:\\Users\\a\\AppData\\Local";
    if (!OwnDir(local + L"\\OpalBar\\", local) || !OwnDir(L"c:\\users\\a\\appdata\\local\\opalbar\\", local) || OwnDir(L"C:\\", local) ||
        OwnDir(local + L"\\OpalBar2\\", local) || OwnDir(local + L"\\", local) || OwnDir(L"\\OpalBar\\", L"")) return 12;
    if (!BuiltInsValid()) return 8;
    if (FromHex(ToHex(L"Тест Æ 漢")) != L"Тест Æ 漢") return 9;
    for (auto& p : kRu) if (!*p[0] || !*p[1] || (g_ru = true, Tr(p[0]) != p[1])) return 10;
    g_ru = false;
    return 0;
}

static void CopyText(HWND w, const std::wstring& s)
{
    if (!OpenClipboard(w)) return;
    EmptyClipboard();
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t))) {
        memcpy(GlobalLock(h), s.c_str(), (s.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(h);
        if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
    }
    CloseClipboard();
}

static std::wstring PasteText(HWND w)
{
    std::wstring s;
    if (!OpenClipboard(w)) return s;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto p = (const wchar_t*)GlobalLock(h)) { s.assign(p, wcsnlen(p, 512)); GlobalUnlock(h); }
    }
    CloseClipboard();
    return s;
}

static void Toast(HWND w, const std::wstring& text)
{
    g_toast = text;
    SetTimer(w, 2, 3000, nullptr);
    InvalidateRect(w, nullptr, FALSE);
}

struct Preset { int n; std::wstring name, code; };
static const Preset kBuiltIn[] = {
    { 0, L"Main", L"GB1-AQAAKQANABQADgAPAA8ABQABACEgICAAMgAFAA8AFP///wAAAAABAAAAACgBAQAAAAAFAAEAAABc" },
    { 0, L"Matte", L"GB1-AQEAKQALAAAABAAUAAAAAAAAAB4pKSkANwAEABDw" },
    { 0, L"TintedGlass", L"GB1-AQEAKQALAAAABQAAAB4ABQAAADIpKSkAMgAEABAH" },
    { 0, L"TintedGlassRound", L"GB1-AQAAKQAMABQADgAAAA8ABAACAC0pKSkALQAEABA4" },
};
constexpr int kMaxPresets = 8, kMaxName = 24;
static std::vector<Preset> g_presets;
static bool g_menu = false;
static int g_menuHot = -1;
static bool g_naming = false;
static std::wstring g_name;

static bool BuiltInsValid()
{
    std::vector<double> vals;
    for (auto& p : kBuiltIn) if (!Parse(p.code, vals)) return false;
    return true;
}

static bool TemplateIsMain(const wchar_t* ini)
{
    g_ini = ini;
    Load();
    return SameAsCurrent(kBuiltIn[0].code);
}

static std::wstring PresetKey(int n) { return L"P" + std::to_wstring(n); }

static void LoadPresets()
{
    g_presets.assign(std::begin(kBuiltIn), std::end(kBuiltIn));
    for (int n = 1; n <= kMaxPresets; n++) {
        wchar_t b[256];
        GetPrivateProfileStringW(L"Presets", PresetKey(n).c_str(), L"", b, 256, g_ini.c_str());
        if (!*b) continue;
        std::wstring v = b;
        auto bar = v.find(L'|');
        std::wstring name = bar == std::wstring::npos ? std::wstring() : FromHex(v.substr(bar + 1));
        g_presets.push_back({ n, name.empty() ? Tr(L"Preset ") + std::to_wstring(n) : name, v.substr(0, bar) });
    }
}

static void SavePreset(HWND w, std::wstring name)
{
    int n = 1;
    while (n <= kMaxPresets && std::any_of(g_presets.begin(), g_presets.end(), [&](auto& p) { return p.n == n; })) n++;
    if (n > kMaxPresets) { Toast(w, Tr(L"All 8 slots are taken, delete a preset first")); return; }
    if (name.empty()) name = Tr(L"Preset ") + std::to_wstring(n);
    WritePrivateProfileStringW(L"Presets", PresetKey(n).c_str(), (Encode() + L"|" + ToHex(name)).c_str(), g_ini.c_str());
    LoadPresets();
    Toast(w, Tr(L"Preset saved: ") + name);
}

static void MenuAct(HWND w, int row, bool del)
{
    int save = (int)g_presets.size();
    if (row == save && !g_naming) {
        g_naming = true;
        g_name.clear();
        g_menuHot = row;
        InvalidateRect(w, nullptr, FALSE);
        return;
    }
    bool named = g_naming;
    g_menu = g_naming = false;
    if (row == save && named) SavePreset(w, g_name);
    else if (row >= 0 && row < save) {
        Preset pr = g_presets[row];
        if (del && pr.n) {
            WritePrivateProfileStringW(L"Presets", PresetKey(pr.n).c_str(), nullptr, g_ini.c_str());
            LoadPresets();
            Toast(w, Tr(L"Preset deleted: ") + pr.name);
        } else if (!del && !Decode(w, pr.code)) Toast(w, Tr(L"This preset is damaged"));
    }
    InvalidateRect(w, nullptr, FALSE);
}

static void CheckUpdate(HWND w)
{
    if (!*kUpdateRepo) { Toast(w, Tr(L"Updates are not connected yet")); return; }
    if (g_updating) return;
    g_updating = true;
    if (!g_updateUrl.empty()) {
        Toast(w, Tr(L"Downloading the update…"));
        std::thread([w, url = g_updateUrl] {
            wchar_t tmp[MAX_PATH];
            GetTempPathW(MAX_PATH, tmp);
            bool ok = URLDownloadToFileW(nullptr, url.c_str(), (std::wstring(tmp) + app::InstallerExe).c_str(), 0, nullptr) == S_OK;
            PostMessageW(w, WM_UPDATE_READY, ok, 0);
        }).detach();
        return;
    }
    Toast(w, Tr(L"Checking for updates…"));
    std::thread([w] {
        auto u = new Update;
        wchar_t file[MAX_PATH];
        auto api = std::wstring(L"https://api.github.com/repos/") + kUpdateRepo + L"/releases/latest";
        if (URLDownloadToCacheFileW(nullptr, api.c_str(), file, MAX_PATH, 0, nullptr) == S_OK) {
            std::ifstream f(file, std::ios::binary);
            *u = ParseRelease(std::string(std::istreambuf_iterator<char>(f), {}), kUpdateRepo);
        }
        if (!PostMessageW(w, WM_UPDATE, 0, (LPARAM)u)) delete u;
    }).detach();
}

static std::unique_ptr<Bitmap> LoadPng(int id)
{
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    HGLOBAL h = r ? LoadResource(nullptr, r) : nullptr;
    IStream* st = h ? SHCreateMemStream((const BYTE*)LockResource(h), SizeofResource(nullptr, r)) : nullptr;
    if (!st) return nullptr;
    std::unique_ptr<Bitmap> out;
    {
        Bitmap src(st);
        if (src.GetLastStatus() == Ok) {
            out = std::make_unique<Bitmap>(src.GetWidth(), src.GetHeight(), PixelFormat32bppPARGB);
            Graphics(out.get()).DrawImage(&src, 0, 0, src.GetWidth(), src.GetHeight());
        }
    }
    st->Release();
    return out;
}

static void RoundRect(GraphicsPath& p, RectF r, float rad)
{
    rad = std::min(rad, std::min(r.Width, r.Height) / 2);
    float d = rad * 2;
    if (d < 1) { p.AddRectangle(r); return; }
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}

static void FillRound(Graphics& g, RectF r, float rad, Color c)
{
    GraphicsPath p; RoundRect(p, r, rad);
    SolidBrush b(c); g.FillPath(&b, &p);
}

static void StrokeRound(Graphics& g, RectF r, float rad, Color c, float width)
{
    GraphicsPath p; RoundRect(p, r, rad);
    Pen pen(c, width); g.DrawPath(&pen, &p);
}

enum Face { kRegular, kSemibold };
static const wchar_t* FaceName(Face f) { return f == kSemibold ? L"Segoe UI Semibold" : L"Segoe UI"; }

static void Text(Graphics& g, const std::wstring& s, RectF r, float size, Color c, Face face = kRegular,
                 StringAlignment h = StringAlignmentNear, StringAlignment v = StringAlignmentCenter)
{
    FontFamily fam(FaceName(face));
    Font f(&fam, S(size), FontStyleRegular, UnitPixel);
    SolidBrush b(c);
    StringFormat fmt; fmt.SetAlignment(h); fmt.SetLineAlignment(v); fmt.SetFormatFlags(StringFormatFlagsNoWrap); fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(s.c_str(), -1, &f, r, &fmt, &b);
}

static Color White(BYTE a) { return Color(a, 255, 255, 255); }
static Color Rgb(double v, BYTE a) { auto c = (unsigned)v; return Color(a, BYTE(c >> 16), BYTE(c >> 8), BYTE(c)); }
static BYTE Alpha(double a) { return BYTE(std::clamp(a, 0.0, 1.0) * 255); }
static const Color kText = White(0xF2), kSub = White(0xB0), kMuted = White(0x60), kInk(255, 24, 24, 26);
constexpr BYTE kTile = 0x20, kTileHot = 0x38, kEdge = 0x30, kTrackA = 0x48, kFillA = 0xE6, kHair = 0x1C;
static Color g_accent(255, 76, 194, 255);

static void LoadAccent()
{
    BYTE pal[32]; DWORD n = sizeof(pal);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", L"AccentPalette",
                     RRF_RT_REG_BINARY, nullptr, pal, &n) == ERROR_SUCCESS && n >= 8) {
        BYTE r = pal[4], g = pal[5], b = pal[6];
        if (std::max({ r, g, b }) - std::min({ r, g, b }) >= 48) g_accent = Color(255, r, g, b);
    }
}

static std::wstring PreviewBg()
{
    wchar_t b[MAX_PATH * 4 + 8];
    GetPrivateProfileStringW(L"Preview", L"Background", L"", b, ARRAYSIZE(b), g_ini.c_str());
    std::wstring s = b;
    return s.rfind(L"u:", 0) == 0 ? FromHex(s.substr(2)) : s;
}

static void LoadWallpaper()
{
    g_back.reset();
    auto bg = PreviewBg();
    wchar_t path[MAX_PATH] = {};
    if (bg.empty()) SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0);
    else if (bg[0] != L'#') wcscpy_s(path, bg.c_str());
    std::unique_ptr<Bitmap> src(*path ? Bitmap::FromFile(path) : nullptr);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), h = sh;
    g_strip = std::make_unique<Bitmap>(sw, h, PixelFormat32bppPARGB);
    Graphics g(g_strip.get());
    if (!bg.empty() && bg[0] == L'#') {
        unsigned c = wcstoul(bg.c_str() + 1, nullptr, 16);
        g.Clear(Color(255, BYTE(c >> 16), BYTE(c >> 8), BYTE(c)));
        return;
    }
    if (!src || src->GetLastStatus() != Ok) {
        LinearGradientBrush b(Point(0, 0), Point(sw, h), Color(255, 40, 90, 140), Color(255, 150, 70, 120));
        g.FillRectangle(&b, 0, 0, sw, h);
        return;
    }
    float k = std::max(sw / (float)src->GetWidth(), sh / (float)src->GetHeight());
    float dw = src->GetWidth() * k, dh = src->GetHeight() * k;
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(src.get(), RectF((sw - dw) / 2, (sh - dh) / 2, dw, dh));
}

static Bitmap* BlurredRegion(Bitmap& base, Rect r, float radius)
{
    int m = (int)std::ceil(radius) + 1;
    Rect src(std::max(0, r.X - m), std::max(0, r.Y - m), 0, 0);
    src.Width = std::min((int)base.GetWidth(), r.GetRight() + m) - src.X;
    src.Height = std::min((int)base.GetHeight(), r.GetBottom() + m) - src.Y;
    Bitmap* c = base.Clone(src, PixelFormat32bppPARGB);
    if (radius >= 1) {
        Blur blur;
        BlurParams p{ std::min(radius, 255.f), FALSE };
        blur.SetParameters(&p);
        RECT rc{ 0, 0, src.Width, src.Height };
        c->ApplyEffect(&blur, &rc);
    }
    c->SetResolution(base.GetHorizontalResolution(), base.GetVerticalResolution());
    return c;
}

enum Glass { gBar, gFlyout, gPanel };
static void DrawGlass(Graphics& g, Bitmap& base, RectF r, float rad, Glass kind = gBar)
{
    Rect ri((int)r.X, (int)r.Y, (int)r.Width, (int)r.Height);
    auto drawBlurred = [&](float radius) {
        std::unique_ptr<Bitmap> b(BlurredRegion(base, ri, radius));
        int m = (int)std::ceil(radius) + 1;
        g.DrawImage(b.get(), (float)std::max(0, ri.X - m), (float)std::max(0, ri.Y - m));
    };
    GraphicsPath outer; RoundRect(outer, r, rad);
    double blur = kind == gPanel ? 28 * g_s : V(iBlur) * 2 * g_s * (kind == gFlyout ? 5 / 3.0 : 1);

    g.SetClip(&outer);
    drawBlurred((float)blur);
    COLORREF t = (COLORREF)V(iTint);
    SolidBrush tint(kind == gPanel ? Color(0xA6, 22, 22, 24) : Color(BYTE(V(kind == gFlyout ? iPanelTint : iTintOpacity) * 255), BYTE(t >> 16), BYTE(t >> 8), BYTE(t)));
    g.FillPath(&tint, &outer);

    float band = (float)(V(iRefraction) * 10) * g_s;
    if (band >= 0.5f && kind == gBar) {
        GraphicsPath inner; RoundRect(inner, RectF(r.X + band, r.Y + band, r.Width - 2 * band, r.Height - 2 * band), std::max(0.f, rad - band));
        Region ring(&outer); ring.Exclude(&inner);
        g.SetClip(&ring);
        drawBlurred((float)(V(iBlur) * 3 + 12) * 2 * g_s);
        SolidBrush light(Color(BYTE(V(iRefraction) * 18), 255, 255, 255));
        g.FillPath(&light, &outer);
    }
    g.ResetClip();

    double spec = kind == gPanel ? cfg::Specular.def : V(iSpecular), glow = kind == gPanel ? cfg::BorderGlow.def : V(iGlow);
    double rc = kind == gPanel ? 0xFFFFFF : V(iRimColor);
    float rw = kind == gPanel ? S(1.5f) : S((float)V(iRimWidth));
    LinearGradientBrush rimBrush(PointF(0, r.Y), PointF(0, r.GetBottom()), White(0), White(0));
    Color cols[] = { Rgb(rc, Alpha(cfg::RimAlpha(cfg::Specular, spec))), Rgb(rc, Alpha(cfg::RimMid(spec, glow))),
                     Rgb(rc, Alpha(cfg::RimAlpha(cfg::BorderGlow, glow))) };
    REAL pos[] = { 0, 0.5f, 1 };
    rimBrush.SetInterpolationColors(cols, pos, 3);
    Pen rim(&rimBrush, rw);
    GraphicsPath edge; RoundRect(edge, RectF(r.X + rw / 2, r.Y + rw / 2, r.Width - rw, r.Height - rw), rad);
    g.DrawPath(&rim, &edge);
}

static void DrawDesk(Graphics& o, Bitmap& base, float W, float H)
{
    float barH = S(48), mv = S((float)MarginV()), rad = S((float)V(iRadius));
    float pillY = H - barH + mv - S((float)std::clamp(V(iOffset), -MarginV(), MarginV())), pillH = barH - 2 * mv;
    const int n = 7;
    float slot = S(44), icon = S(24), content = n * slot;
    float trayW = S(150), gap = S(24);
    bool full = V(iFull) != 0;
    float pillW = V(iDynamic) ? content + 2 * S((float)V(iPadding)) : std::min(S((float)V(iWidth)), W - trayW - 3 * gap);
    pillW = std::max(pillW, content * 0.5f);
    RectF pill((W - trayW - gap - pillW) / 2, pillY, pillW, pillH);
    RectF tray(W - gap - trayW, pillY, trayW, pillH);
    if (full) pill = RectF(0, pillY, W, pillH);
    auto shadow = [&](RectF r) {
        for (int k = 1; k <= 6; k++) {
            double a = V(iShadow) * 0.5 * std::pow(1 - (k - 1) / 6.0, 2);
            float d = S(k - 0.5f);
            if (a > 0) StrokeRound(o, RectF(r.X - d, r.Y - d, r.Width + 2 * d, r.Height + 2 * d), rad + S((float)k), Color(Alpha(a), 0, 0, 0), S(1));
        }
    };
    shadow(pill);
    if (!full) shadow(tray);
    DrawGlass(o, base, pill, rad);
    if (!full) DrawGlass(o, base, tray, rad);

    {
        RectF fly(W - gap - S(216), H - barH - S(8) - S(132), S(216), S(132));
        DrawGlass(o, base, fly, S(24 * (float)V(iRadius) / 20), gFlyout);
        const wchar_t* names[] = { L"Wi-Fi", L"Bluetooth", Tr(L"Saver") };
        float tw = S(56), tg = (fly.Width - S(32) - 3 * tw) / 2;
        for (int i = 0; i < 3; i++) {
            RectF t(fly.X + S(16) + i * (tw + tg), fly.Y + S(16), tw, S(34));
            FillRound(o, t, S(6), i == 0 ? g_accent : White(kTile));
            if (i) StrokeRound(o, t, S(6), White(kEdge), S(1));
            Text(o, names[i], RectF(t.X - S(8), t.GetBottom() + S(4), tw + S(16), S(16)), 11, White(0xEB), kRegular, StringAlignmentCenter);
        }
        RectF tr(fly.X + S(16), fly.GetBottom() - S(26), fly.Width - S(32), S(4));
        FillRound(o, tr, S(2), White(kTrackA));
        FillRound(o, RectF(tr.X, tr.Y, tr.Width * 0.4f, tr.Height), S(2), White(kFillA));
        SolidBrush knob(White(255));
        o.FillEllipse(&knob, tr.X + tr.Width * 0.4f - S(6), tr.Y + S(2) - S(6), S(12), S(12));
    }

    float x0 = pill.X + (pill.Width - content) / 2 + (slot - icon) / 2;
    {
        RectF hl(x0 + slot - (slot - icon) / 2 + S(2), pill.Y + S(2), slot - S(4), pill.Height - S(4));
        BYTE ha = Alpha(0x24 / 255.0 * V(iHover));
        FillRound(o, hl, S((float)std::min(V(iHl), std::max(4.0, V(iRadius) - 2))), V(iHoverColor) < 0 ? White(ha) : Rgb(V(iHoverColor), ha));
    }
    for (int i = 0; i < n; i++) {
        RectF ir(x0 + i * slot, pill.Y + (pill.Height - icon) / 2 - S(1), icon, icon);
        FillRound(o, ir, S(6), Color(224, 237, 237, 237));
        FillRound(o, RectF(ir.X, ir.Y + ir.Height / 2, ir.Width, ir.Height / 2), S(6), Color(40, 20, 20, 21));
        if (V(iIndicator) && (i == 2 || i == 4 || i == 5)) {
            bool focused = i == 4;
            float w = S(focused ? 16.f : 6.f);
            Color c = focused ? g_accent : Color(200, 237, 237, 237);
            if (V(iIndColor) >= 0) c = Rgb(V(iIndColor), c.GetA());
            FillRound(o, RectF(ir.X + (icon - w) / 2, pill.GetBottom() - S(5), w, S(3)), S(1.5f), c);
        }
    }
    if (full) return;
    for (int i = 0; i < 3; i++)
        FillRound(o, RectF(tray.X + S(16) + i * S(24), tray.Y + tray.Height / 2 - S(6), S(12), S(12)), S(4), Color(210, 237, 237, 237));
    Text(o, L"12:00", RectF(tray.X + S(72), tray.Y, S(64), tray.Height), float(12 * V(iClockSize)),
         V(iClockColor) < 0 ? White(0xF0) : Rgb(V(iClockColor), 0xF0), V(iClockBold) ? kSemibold : kRegular, StringAlignmentFar);
}

static float TextW(const wchar_t* s, float size)
{
    Bitmap bmp(1, 1);
    Graphics g(&bmp);
    FontFamily fam(FaceName(kRegular));
    Font f(&fam, S(size), FontStyleRegular, UnitPixel);
    RectF r;
    g.MeasureString(s, -1, &f, PointF(0, 0), &r);
    return r.Width;
}

static RectF g_pane, g_cardL, g_cardR, g_cardX, g_cardStart, g_bgPane;
static constexpr float kWinW = 1100, kWinH = 924;

static void Layout()
{
    float m = S(24), col = (S(kWinW) - 2 * m) / 3, top = S(20), head = S(56), rows = S(52 + 9 * 52 + 8), foot = S(108);
    float bgLabel = S(g_ru ? 40.f : 84.f);
    g_pane = RectF(m, top, 3 * col, head + rows + foot);
    g_cardL = RectF(m, top + head, col, rows);
    g_cardR = RectF(m + col, top + head, col, rows);
    g_cardX = RectF(m + 2 * col, top + head, col, rows);
    g_cardStart = RectF(m, top + head + rows, 3 * col, foot);
    g_bgPane = RectF(m, g_pane.GetBottom() + S(16), S(12 + 3 * 84 + 2 * 8 + 12) + bgLabel, S(44));
    auto row = [&](RectF c, int i) { return RectF(c.X + S(20), c.Y + S(52) + i * S(52), c.Width - S(40), S(40)); };
    for (int i = iDynamic; i <= iHl; i++) g_items[i].box = row(g_cardL, i - iDynamic);
    for (int i = iBlur; i <= iTintOpacity; i++) g_items[i].box = row(g_cardR, i - iBlur);
    g_items[iPanelTint].box = row(g_cardR, 5);
    g_items[iHeight].box = row(g_cardL, 6);
    g_items[iOffset].box = row(g_cardL, 7);
    g_items[iTint].box = row(g_cardR, 6);
    g_items[iRimColor].box = row(g_cardR, 7);
    g_items[iRimWidth].box = row(g_cardR, 8);
    int details[] = { iAnim, iShadow, iHoverColor, iHover, iIndicator, iIndColor, iClockSize, iClockBold, iClockColor };
    for (int k = 0; k < (int)std::size(details); k++) g_items[details[k]].box = row(g_cardX, k);
    auto toggleW = [](int i) { return std::ceil(TextW(Tr(g_items[i].label), 13)) + S(16 + 40); };
    g_items[iAutostart].box = RectF(g_cardStart.X + S(20), g_cardStart.Y + S(12), toggleW(iAutostart), S(40));
    g_items[iTray].box = RectF(g_items[iAutostart].box.GetRight() + S(32), g_cardStart.Y + S(12), toggleW(iTray), S(40));
    g_items[iImport].box = RectF(g_cardStart.GetRight() - S(20) - S(96), g_cardStart.Y + S(16), S(96), S(32));
    g_items[iExport].box = RectF(g_items[iImport].box.X - S(8) - S(96), g_cardStart.Y + S(16), S(96), S(32));
    g_items[iLang].box = RectF(g_items[iExport].box.X - S(8) - S(96), g_cardStart.Y + S(16), S(96), S(32));
    g_items[iUninstall].box = RectF(g_items[iLang].box.X - S(8) - S(96), g_cardStart.Y + S(16), S(96), S(32));
    g_items[iApply].box = RectF(g_items[iImport].box.X, g_cardStart.Y + S(60), S(96), S(32));
    g_items[iCode].box = RectF(g_cardStart.X + S(20), g_cardStart.Y + S(60), g_items[iApply].box.X - S(8) - g_cardStart.X - S(20), S(32));
    g_items[iReset].box = RectF(g_pane.GetRight() - S(20) - S(112), top + S(12), S(112), S(32));
    g_items[iUpdate].box = RectF(g_items[iReset].box.X - S(8) - S(192), top + S(12), S(192), S(32));
    g_items[iPresets].box = RectF(g_items[iUpdate].box.X - S(8) - S(160), top + S(12), S(160), S(32));
    g_items[iEnabled].box = RectF(g_pane.X + S(200), top + S(8), S(112), S(40));
    g_items[iReload].box = RectF(g_pane.X + S(328), top + S(12), S(120), S(32));
    g_items[iAuto].box = RectF(g_cardR.GetRight() - S(20) - S(72), g_cardR.Y + S(8), S(72), S(32));
    for (int i = iBgWall; i <= iBgColor; i++)
        g_items[i].box = RectF(g_bgPane.X + S(12) + bgLabel + (i - iBgWall) * S(92), g_bgPane.Y + S(8), S(84), S(28));
    bool full = V(iFull) != 0, dyn = V(iDynamic) != 0;
    g_items[iDynamic].enabled = !full;
    g_items[iWidth].enabled = !full && !dyn;
    g_items[iPadding].enabled = !full && dyn;
    g_items[iOffset].enabled = MarginV() > 0;
    g_items[iIndColor].enabled = V(iIndicator) != 0;
}

static RectF MenuRow(int i)
{
    auto& b = g_items[iPresets].box;
    return RectF(b.X, b.GetBottom() + S(8) + i * S(36), S(288), S(36));
}

static int MenuHit(float x, float y, bool* del = nullptr)
{
    for (int i = 0; i <= (int)g_presets.size(); i++) {
        RectF r = MenuRow(i);
        if (!r.Contains(x, y)) continue;
        if (del) *del = i < (int)g_presets.size() && g_presets[i].n && x >= r.GetRight() - S(36);
        return i;
    }
    return -1;
}

static void EnsureBackdrop(int W, int H)
{
    if (g_back && (int)g_back->GetWidth() == W && (int)g_back->GetHeight() == H) return;
    g_base = std::make_unique<Bitmap>(W, H, PixelFormat32bppPARGB);
    {
        Graphics bg(g_base.get());
        int sx = std::max(0, ((int)g_strip->GetWidth() - W) / 2), sy = std::max(0, (int)g_strip->GetHeight() - H);
        bg.DrawImage(g_strip.get(), Rect(0, 0, W, H), sx, sy, W, H, UnitPixel);
    }
    g_back = std::make_unique<Bitmap>(W, H, PixelFormat32bppPARGB);
    Graphics g(g_back.get());
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.DrawImage(g_base.get(), 0, 0);
    DrawGlass(g, *g_base, g_pane, S(20), gPanel);
    DrawGlass(g, *g_base, g_bgPane, S(16), gPanel);
}

static RectF Track(const Item& it) { return RectF(it.box.X, it.box.Y + S(30), it.box.Width, S(4)); }

static void DrawItem(Graphics& g, int i)
{
    auto& it = g_items[i];
    Color text = it.enabled ? kText : kMuted;
    bool hot = it.enabled && (i == g_hover || i == g_drag || (i == g_focus && g_keyboard));
    if (it.kind == kSlider) {
        Text(g, Tr(it.label), RectF(it.box.X, it.box.Y, it.box.Width, S(20)), 13, text);
        wchar_t v[32]; swprintf_s(v, Tr(it.fmt), it.v * it.shown);
        Text(g, v, RectF(it.box.X, it.box.Y, it.box.Width, S(20)), 12, it.enabled ? kSub : kMuted, kRegular, StringAlignmentFar);
        RectF tr = Track(it);
        float f = (float)((it.v - it.min) / (it.max - it.min));
        FillRound(g, tr, S(2), White(it.enabled ? kTrackA : 0x20));
        FillRound(g, RectF(tr.X, tr.Y, tr.Width * f, tr.Height), S(2), White(it.enabled ? kFillA : 0x50));
        float cx = tr.X + tr.Width * f, cy = tr.Y + tr.Height / 2, kr = S(hot ? 8.f : 6.f);
        SolidBrush knob(it.enabled ? White(255) : Color(255, 128, 128, 132));
        g.FillEllipse(&knob, cx - kr, cy - kr, 2 * kr, 2 * kr);
    } else if (it.kind == kToggle) {
        Text(g, Tr(it.label), it.box, 13, text);
        RectF sw(it.box.GetRight() - S(40), it.box.Y + (it.box.Height - S(20)) / 2, S(40), S(20));
        bool on = it.v != 0;
        if (on) FillRound(g, sw, S(10), White(it.enabled ? kFillA : 0x50));
        else { FillRound(g, sw, S(10), White(hot ? kTileHot : 0x10)); StrokeRound(g, sw, S(10), White(it.enabled ? 0x80 : 0x40), S(1)); }
        float kd = S(hot ? 14.f : 12.f), kx = on ? sw.GetRight() - S(4) - kd : sw.X + S(4);
        SolidBrush k(on ? kInk : White(it.enabled ? 0xD0 : 0x60));
        g.FillEllipse(&k, kx, sw.Y + (sw.Height - kd) / 2, kd, kd);
    } else if (it.kind == kColor) {
        Text(g, Tr(it.label), it.box, 13, text);
        RectF sw(it.box.GetRight() - S(40), it.box.Y + (it.box.Height - S(24)) / 2, S(40), S(24));
        bool stock = it.v < 0;
        FillRound(g, sw, S(6), stock ? White(it.enabled ? 0x18 : 0x08) : Rgb(it.v, it.enabled ? 255 : 0x60));
        StrokeRound(g, sw, S(6), White(hot ? 0x90 : 0x40), S(1));
        wchar_t hex[16]; swprintf_s(hex, L"#%06X", (unsigned)std::max(0.0, it.v));
        Text(g, stock ? Tr(L"Auto") : hex, RectF(it.box.X, it.box.Y, it.box.Width - S(52), it.box.Height), 12, it.enabled ? kSub : kMuted, kRegular, StringAlignmentFar);
    } else if (i == iCode) {
        FillRound(g, it.box, S(8), Color(255, GetRValue(kWell), GetGValue(kWell), GetBValue(kWell)));
        StrokeRound(g, it.box, S(8), White(GetFocus() == g_edit ? 0x80 : kEdge), S(1));
        if (GetWindowTextLengthW(g_edit)) {
            Pen ink(hot ? kText : kSub, S(1.5f));
            float x = it.box.GetRight() - S(18), y = it.box.Y + it.box.Height / 2;
            g.DrawLine(&ink, x - S(4), y - S(4), x + S(4), y + S(4));
            g.DrawLine(&ink, x - S(4), y + S(4), x + S(4), y - S(4));
        }
    } else {
        FillRound(g, it.box, S(8), White(hot ? kTileHot : kTile));
        StrokeRound(g, it.box, S(8), White(kEdge), S(1));
        RectF tb = it.box;
        if (i == iPresets) { tb.X += S(12); tb.Width -= S(40); }
        Text(g, Tr(it.label), tb, 13, kText, kRegular, StringAlignmentCenter);
    }
    if (i == g_focus && g_keyboard)
        StrokeRound(g, RectF(it.box.X - S(8), it.box.Y - S(4), it.box.Width + S(16), it.box.Height + S(8)), S(10), White(kFillA), S(2));
}

static void Paint(HWND w, HDC dc)
{
    RECT rc; GetClientRect(w, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    Layout();
    EnsureBackdrop(rc.right, rc.bottom);
    Bitmap buf(rc.right, rc.bottom, PixelFormat32bppPARGB);
    Graphics g(&buf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.DrawImage(g_back.get(), 0, 0);
    DrawDesk(g, *g_base, (float)rc.right, (float)rc.bottom);

    if (g_logo) {
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(g_logo.get(), RectF(g_pane.X + S(20), g_pane.Y + S(14), S(28), S(28)));
    }
    Text(g, app::Name, RectF(g_pane.X + S(56), g_pane.Y, S(120), S(56)), 20, kText, kSemibold);
    Text(g, app::Version, RectF(g_pane.X + S(144), g_pane.Y + S(3), S(48), S(56)), 12, kSub);
    g_items[iEnabled].label = V(iEnabled) ? L"Enabled" : L"Disabled";
    g_items[iLang].label = g_ru ? L"English" : L"Русский";
    SolidBrush hair(White(kHair));
    g.FillRectangle(&hair, g_pane.X, g_cardL.Y, g_pane.Width, S(1));
    g.FillRectangle(&hair, g_pane.X, g_cardStart.Y, g_pane.Width, S(1));
    g.FillRectangle(&hair, g_cardR.X, g_cardL.Y, S(1), g_cardL.Height);
    g.FillRectangle(&hair, g_cardX.X, g_cardL.Y, S(1), g_cardL.Height);
    Text(g, Tr(L"Details"), RectF(g_cardX.X + S(20), g_cardX.Y + S(12), g_cardX.Width, S(24)), 14, kText, kSemibold);
    Text(g, Tr(L"Geometry"), RectF(g_cardL.X + S(20), g_cardL.Y + S(12), g_cardL.Width, S(24)), 14, kText, kSemibold);
    Text(g, Tr(L"Glass"), RectF(g_cardR.X + S(20), g_cardR.Y + S(12), g_cardR.Width, S(24)), 14, kText, kSemibold);
    Text(g, Tr(L"Background"), RectF(g_bgPane.X + S(12), g_bgPane.Y, S(84), g_bgPane.Height), 13, kText);
    static std::wstring presetLabel;
    presetLabel = Tr(L"Presets");
    for (auto& p : g_presets) if (SameAsCurrent(p.code)) { presetLabel = p.name; break; }
    g_items[iPresets].label = presetLabel.c_str();
    for (int i = 0; i < (int)g_items.size(); i++) DrawItem(g, i);
    Pen ink(kSub, S(1.5f));
    {
        auto& b = g_items[iPresets].box;
        float x = b.GetRight() - S(16), y = b.Y + b.Height / 2;
        g.DrawLine(&ink, x - S(4), y - S(2), x, y + S(2));
        g.DrawLine(&ink, x, y + S(2), x + S(4), y - S(2));
    }
    if (g_menu) {
        int rows = (int)g_presets.size() + 1;
        RectF first = MenuRow(0), m(first.X - S(4), first.Y - S(4), first.Width + S(8), rows * S(36) + S(8));
        DrawGlass(g, *g_base, m, S(12), gPanel);
        FillRound(g, RectF(m.X + S(1), m.Y + S(1), m.Width - S(2), m.Height - S(2)), S(11), Color(0x80, 16, 16, 18));
        for (int i = 0; i < rows; i++) {
            RectF r = MenuRow(i), tr(r.X + S(36), r.Y, r.Width - S(72), r.Height);
            bool save = i == rows - 1;
            if (i == g_menuHot) FillRound(g, r, S(8), White(kTile));
            float cx = r.X + S(18), cy = r.Y + r.Height / 2;
            if (save) {
                g.DrawLine(&ink, cx - S(5), cy, cx + S(5), cy);
                g.DrawLine(&ink, cx, cy - S(5), cx, cy + S(5));
                tr.Width = r.Width - S(44);
                if (!g_naming) Text(g, Tr(L"Save current settings"), tr, 13, kText);
                else if (g_name.empty()) Text(g, Tr(L"Preset name, then Enter"), tr, 13, kSub);
                else Text(g, g_name + L"|", tr, 13, kText);
                continue;
            }
            if (SameAsCurrent(g_presets[i].code)) {
                SolidBrush dot(kText);
                g.FillEllipse(&dot, cx - S(3), cy - S(3), S(6), S(6));
            }
            Text(g, g_presets[i].name, tr, 13, kText);
            if (g_presets[i].n) {
                float x = r.GetRight() - S(18);
                g.DrawLine(&ink, x - S(4), cy - S(4), x + S(4), cy + S(4));
                g.DrawLine(&ink, x - S(4), cy + S(4), x + S(4), cy - S(4));
            }
        }
    }
    if (!g_toast.empty()) {
        FontFamily fam(FaceName(kRegular));
        Font f(&fam, S(13), FontStyleRegular, UnitPixel);
        RectF m; g.MeasureString(g_toast.c_str(), -1, &f, PointF(0, 0), &m);
        RectF r((rc.right - m.Width) / 2 - S(14), rc.bottom - S(48 + 8 + 36), m.Width + S(28), S(36));
        DrawGlass(g, *g_base, r, S(12), gPanel);
        Text(g, g_toast, r, 13, kText, kRegular, StringAlignmentCenter);
    }

    Graphics(dc).DrawImage(&buf, 0, 0);
}

static int HitTest(float x, float y)
{
    for (int i = 0; i < (int)g_items.size(); i++) {
        auto& it = g_items[i];
        RectF r = it.box;
        if (it.kind == kSlider) r = RectF(r.X - S(10), r.Y + S(20), r.Width + S(20), S(28));
        if (it.enabled && r.Contains(x, y)) return i;
    }
    return -1;
}

static void SlideTo(HWND w, int i, float x)
{
    auto& it = g_items[i];
    RectF tr = Track(it);
    Change(w, i, it.min + std::clamp((x - tr.X) / tr.Width, 0.f, 1.f) * (it.max - it.min));
}

static void Uninstall(HWND w);

static void Activate(HWND w, int i)
{
    auto& it = g_items[i];
    if (it.kind == kToggle) {
        Change(w, i, it.v ? 0 : 1);
        if (i == iEnabled) Toast(w, Tr(it.v ? L"OpalBar is on" : L"OpalBar is off, Explorer is restarting"));
    }
    else if (it.kind == kColor) {
        static COLORREF custom[16];
        COLORREF c = (COLORREF)std::max(0.0, it.v);
        CHOOSECOLORW cc{ sizeof(cc), w };
        cc.rgbResult = RGB(c >> 16 & 255, c >> 8 & 255, c & 255);
        cc.lpCustColors = custom;
        cc.Flags = CC_RGBINIT | CC_FULLOPEN;
        if (ChooseColorW(&cc)) Change(w, i, (double)((GetRValue(cc.rgbResult) << 16) | (GetGValue(cc.rgbResult) << 8) | GetBValue(cc.rgbResult)));
    } else if (i == iAuto) {
        for (int j : { iBlur, iRefraction, iSpecular, iGlow, iTintOpacity, iTint, iPanelTint, iRimColor, iRimWidth }) Change(w, j, g_items[j].def);
    } else if (i == iReload) {
        ReloadShell();
        ShellExecuteW(nullptr, nullptr, (g_dir + app::LoaderExe).c_str(), nullptr, g_dir.c_str(), SW_HIDE);
        Toast(w, Tr(L"Restarting Explorer…"));
    } else if (i == iUninstall) {
        Uninstall(w);
    } else if (i == iReset) {
        Decode(w, kBuiltIn[0].code);
    } else if (i == iExport) {
        CopyText(w, Encode());
        SetWindowTextW(g_edit, Encode().c_str());
        Toast(w, Tr(L"Code copied to the clipboard"));
    } else if (i == iImport) {
        Toast(w, Tr(Decode(w, PasteText(w)) ? L"Settings from the code applied" : L"No OpalBar code in the clipboard"));
    } else if (i == iCode) {
        SetFocus(g_edit);
    } else if (i == iApply) {
        Toast(w, Tr(Decode(w, CodeText()) ? L"Settings from the code applied" : L"This is not an OpalBar code"));
    } else if (i == iLang) {
        g_ru = !g_ru;
        WritePrivateProfileStringW(L"App", L"Language", g_ru ? L"ru" : L"en", g_ini.c_str());
        SetWindowTextW(w, Tr(L"OpalBar Settings"));
        SendMessageW(g_edit, EM_SETCUEBANNER, TRUE, (LPARAM)Tr(g_items[iCode].label));
        g_back.reset();
        InvalidateRect(w, nullptr, FALSE);
    } else if (i == iUpdate) {
        CheckUpdate(w);
    } else if (i == iPresets) {
        LoadPresets();
        g_menu = true;
        g_menuHot = g_keyboard ? 0 : -1;
        g_hover = -1;
    } else if (i >= iBgWall && i <= iBgColor) {
        std::wstring bg;
        if (i == iBgImage) {
            wchar_t file[MAX_PATH] = {};
            OPENFILENAMEW of{ sizeof(of), w };
            of.lpstrFilter = Tr(L"Images\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff\0");
            of.lpstrFile = file;
            of.nMaxFile = MAX_PATH;
            of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetOpenFileNameW(&of)) return;
            bg = L"u:" + ToHex(file);
        } else if (i == iBgColor) {
            static COLORREF custom[16];
            CHOOSECOLORW cc{ sizeof(cc), w };
            cc.lpCustColors = custom;
            cc.Flags = CC_FULLOPEN;
            if (!ChooseColorW(&cc)) return;
            wchar_t hex[16]; swprintf_s(hex, L"#%02X%02X%02X", GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
            bg = hex;
        }
        WritePrivateProfileStringW(L"Preview", L"Background", bg.empty() ? nullptr : bg.c_str(), g_ini.c_str());
        LoadWallpaper();
        InvalidateRect(w, nullptr, FALSE);
    }
}

static void Uninstall(HWND w)
{
    if (MessageBoxW(w, Tr(L"Remove OpalBar from this PC? Your settings and presets will be deleted. Explorer will restart."), app::Name, MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    app::StopLoader();
    Sleep(300);
    app::KillByName(app::LoaderExe);
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, app::RunKey, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) { RegDeleteValueW(k, app::Name); RegCloseKey(k); }
    wchar_t desk[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desk))) DeleteFileW((std::wstring(desk) + L"\\OpalBar.lnk").c_str());
    app::RestartShell();
    wchar_t local[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) && OwnDir(g_dir, local)) {
        std::wstring cmd = L"cmd.exe /c ping -n 4 127.0.0.1 >nul & rmdir /s /q \"" + g_dir.substr(0, g_dir.size() - 1) + L"\"";
        STARTUPINFOW si{ sizeof(si) };
        PROCESS_INFORMATION pi;
        wchar_t dir[MAX_PATH];
        GetEnvironmentVariableW(L"TEMP", dir, MAX_PATH);
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    }
    g_pending = 0;
    KillTimer(w, 1);
    DestroyWindow(w);
}

static void MoveFocus(HWND w, int dir)
{
    int n = (int)g_items.size();
    do g_focus = (g_focus + dir + n) % n; while (!g_items[g_focus].enabled);
    g_keyboard = true;
    if (g_focus == iCode) SetFocus(g_edit);
    InvalidateRect(w, nullptr, FALSE);
}

static WNDPROC g_editProc;
static LRESULT CALLBACK EditProc(HWND e, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND w = GetParent(e);
    if (msg == WM_KEYDOWN && wp == VK_RETURN) { Activate(w, iApply); return 0; }
    if (msg == WM_KEYDOWN && wp == VK_TAB) {
        SetFocus(w);
        g_focus = iCode;
        MoveFocus(w, GetKeyState(VK_SHIFT) < 0 ? -1 : 1);
        return 0;
    }
    if (msg == WM_CHAR && (wp == L'\r' || wp == L'\t')) return 0;
    return CallWindowProcW(g_editProc, e, msg, wp, lp);
}

static void PlaceEdit()
{
    Layout();
    static HFONT font;
    HFONT old = font;
    font = CreateFontW(-(int)S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SendMessageW(g_edit, WM_SETFONT, (WPARAM)font, TRUE);
    if (old) DeleteObject(old);
    auto& b = g_items[iCode].box;
    int h = (int)S(20);
    MoveWindow(g_edit, (int)(b.X + S(12)), (int)(b.Y + (b.Height - h) / 2), (int)(b.Width - S(12 + 36)), h, TRUE);
}

static LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        Paint(w, dc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (g_menu) {
            bool del = false;
            int row = MenuHit((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp), &del);
            MenuAct(w, row, del);
            return 0;
        }
        int i = HitTest((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp));
        g_keyboard = false;
        SetFocus(w);
        if (i < 0) return 0;
        g_focus = i;
        if (i == iCode && GET_X_LPARAM(lp) >= g_items[i].box.GetRight() - S(36)) { SetWindowTextW(g_edit, L""); return 0; }
        if (g_items[i].kind == kSlider) { g_drag = i; SetCapture(w); SlideTo(w, i, (float)GET_X_LPARAM(lp)); }
        else Activate(w, i);
        InvalidateRect(w, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_drag >= 0) { SlideTo(w, g_drag, (float)GET_X_LPARAM(lp)); return 0; }
        if (g_menu) {
            int row = MenuHit((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp));
            if (row != g_menuHot) { g_menuHot = row; InvalidateRect(w, nullptr, FALSE); }
            return 0;
        }
        int h = HitTest((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp));
        if (h != g_hover) {
            g_hover = h;
            TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, w };
            TrackMouseEvent(&t);
            InvalidateRect(w, nullptr, FALSE);
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        int i = HitTest((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp));
        if (!g_menu && i >= 0 && g_items[i].kind == kColor) Change(w, i, g_items[i].def);
        return 0;
    }
    case WM_ACTIVATE:
        if (!(g_pending & (1ull << iEnabled))) V(iEnabled) = GetPrivateProfileIntW(L"App", L"Enabled", 1, g_ini.c_str()) != 0;
        if (!(g_pending & (1ull << iTray))) V(iTray) = GetPrivateProfileIntW(L"App", L"TrayIcon", 1, g_ini.c_str()) != 0;
        InvalidateRect(w, nullptr, FALSE);
        break;
    case WM_MOUSELEAVE:
        g_hover = -1;
        InvalidateRect(w, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (g_drag >= 0) { g_drag = -1; ReleaseCapture(); InvalidateRect(w, nullptr, FALSE); }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT p; GetCursorPos(&p); ScreenToClient(w, &p);
            SetCursor(LoadCursorW(nullptr, (g_menu ? MenuHit((float)p.x, (float)p.y) : HitTest((float)p.x, (float)p.y)) >= 0 ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_KEYDOWN: {
        if (g_menu) {
            int last = (int)g_presets.size();
            if (g_naming) {
                if (wp == VK_RETURN) MenuAct(w, last, false);
                else if (wp == VK_ESCAPE) g_menu = g_naming = false;
            }
            else if (wp == VK_DOWN) g_menuHot = std::min(g_menuHot + 1, last);
            else if (wp == VK_UP) g_menuHot = std::max(g_menuHot - 1, 0);
            else if (wp == VK_RETURN || wp == VK_SPACE) MenuAct(w, g_menuHot, false);
            else if (wp == VK_DELETE && g_menuHot >= 0 && g_menuHot < last) MenuAct(w, g_menuHot, true);
            else if (wp == VK_ESCAPE || wp == VK_TAB) g_menu = false;
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        }
        auto& it = g_items[g_focus];
        double step = it.step * (GetKeyState(VK_CONTROL) < 0 ? 10 : 1);
        if (wp == VK_TAB) MoveFocus(w, GetKeyState(VK_SHIFT) < 0 ? -1 : 1);
        else if (!it.enabled) return 0;
        else if (wp == VK_DOWN && it.kind != kSlider) MoveFocus(w, 1);
        else if (wp == VK_UP && it.kind != kSlider) MoveFocus(w, -1);
        else if (it.kind == kSlider && (wp == VK_LEFT || wp == VK_DOWN)) { g_keyboard = true; Change(w, g_focus, it.v - step); }
        else if (it.kind == kSlider && (wp == VK_RIGHT || wp == VK_UP)) { g_keyboard = true; Change(w, g_focus, it.v + step); }
        else if (wp == VK_SPACE || wp == VK_RETURN) { g_keyboard = true; Activate(w, g_focus); }
        return 0;
    }
    case WM_CHAR:
        if (g_naming) {
            if (wp == 8) { if (!g_name.empty()) g_name.pop_back(); }
            else if (wp >= 32 && (int)g_name.size() < kMaxName) g_name += (wchar_t)wp;
            InvalidateRect(w, nullptr, FALSE);
        }
        return 0;
    case WM_COMMAND:
        if ((HWND)lp == g_edit) InvalidateRect(w, nullptr, FALSE);
        return 0;
    case WM_CTLCOLOREDIT: {
        static HBRUSH well = CreateSolidBrush(kWell);
        SetTextColor((HDC)wp, RGB(242, 242, 242));
        SetBkColor((HDC)wp, kWell);
        return (LRESULT)well;
    }
    case WM_TIMER:
        if (wp == 2) { KillTimer(w, 2); g_toast.clear(); InvalidateRect(w, nullptr, FALSE); return 0; }
        KillTimer(w, 1);
        for (int i = 0; i < (int)g_items.size(); i++)
            if (g_pending & (1ull << i)) WriteItem(i);
        g_pending = 0;
        return 0;
    case WM_UPDATE: {
        std::unique_ptr<Update> u((Update*)lp);
        g_updating = false;
        if (!u->ok) Toast(w, Tr(L"Could not check for updates"));
        else if (!Newer(u->version, app::Version)) Toast(w, Tr(L"You have the latest version"));
        else if (u->installer.empty()) Toast(w, Tr(L"No installer in the new version ") + u->version);
        else {
            g_updateUrl = u->installer;
            g_items[iUpdate].label = L"Install update";
            Toast(w, Tr(L"New version available: ") + u->version);
        }
        return 0;
    }
    case WM_UPDATE_READY: {
        g_updating = false;
        if (!wp) { Toast(w, Tr(L"Could not download the update")); return 0; }
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        ShellExecuteW(nullptr, nullptr, (std::wstring(tmp) + app::InstallerExe).c_str(), L"/update", nullptr, SW_SHOWNORMAL);
        DestroyWindow(w);
        return 0;
    }
    case WM_DPICHANGED: {
        g_s = HIWORD(wp) / 96.f * g_fit;
        PlaceEdit();
        auto r = (RECT*)lp;
        SetWindowPos(w, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(w, nullptr, FALSE);
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (wp == SPI_SETDESKWALLPAPER && PreviewBg().empty()) { LoadWallpaper(); InvalidateRect(w, nullptr, FALSE); }
        return 0;
    case WM_DESTROY:
        SendMessageW(w, WM_TIMER, 1, 0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show)
{
    if (wcsstr(cmd, L"--selftest")) return SelfTest(__argc > 2 ? __wargv[2] : nullptr);
    if (HWND old = FindWindowW(L"OpalBarSettings", nullptr)) {
        ShowWindow(old, SW_RESTORE);
        SetForegroundWindow(old);
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ULONG_PTR gdip;
    GdiplusStartupInput gsi;
    GdiplusStartup(&gdip, &gsi, nullptr);

    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_dir = exe;
    g_dir = g_dir.substr(0, g_dir.rfind(L'\\') + 1);
    g_ini = g_dir + L"config.ini";
    Load();
    {
        wchar_t lang[8];
        GetPrivateProfileStringW(L"App", L"Language", L"en", lang, 8, g_ini.c_str());
        g_ru = !wcscmp(lang, L"ru");
    }
    LoadPresets();
    LoadAccent();
    LoadWallpaper();
    g_logo = LoadPng(2);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"OpalBarSettings";
    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    g_s = GetDpiForSystem() / 96.f;
    RECT r{ 0, 0, (LONG)S(kWinW), (LONG)S(kWinH) };
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForSystem());
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    g_fit = std::min({ 1.f, float(wa.bottom - wa.top) / (r.bottom - r.top), float(wa.right - wa.left) / (r.right - r.left) });
    g_s *= g_fit;
    r = { 0, 0, (LONG)S(kWinW), (LONG)S(kWinH) };
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForSystem());
    HWND w = CreateWindowExW(0, wc.lpszClassName, Tr(L"OpalBar Settings"), style, wa.left + (wa.right - wa.left - (r.right - r.left)) / 2, wa.top + (wa.bottom - wa.top - (r.bottom - r.top)) / 2,
                             r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    g_s = GetDpiForWindow(w) / 96.f * g_fit;
    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, w, nullptr, inst, nullptr);
    g_editProc = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)EditProc);
    SendMessageW(g_edit, EM_SETLIMITTEXT, kMaxCode, 0);
    SendMessageW(g_edit, EM_SETCUEBANNER, TRUE, (LPARAM)Tr(g_items[iCode].label));
    PlaceEdit();
    BOOL dark = TRUE;
    DwmSetWindowAttribute(w, 20, &dark, sizeof(dark));
    COLORREF caption = RGB(24, 24, 26);
    DwmSetWindowAttribute(w, 35, &caption, sizeof(caption));
    ShowWindow(w, show);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    g_base.reset(); g_back.reset(); g_logo.reset();
    g_strip.reset();
    GdiplusShutdown(gdip);
    return 0;
}
