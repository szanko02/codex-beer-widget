#include "diagnostics.hpp"
#include "renderer.hpp"
#include "settings.hpp"
#include "settings_ui.hpp"
#include "worker.hpp"
#include <algorithm>
#include <commctrl.h>
#include <fstream>
#include <iomanip>
#include <memory>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <windowsx.h>
#include <wtsapi32.h>

namespace {
constexpr wchar_t window_class[] = L"CodexBeerWidget.Window";
enum Command {
    ToggleTop = 101,
    ToggleLock,
    ToggleClicks,
    Quit,
    ToggleShow,
    OpenSettings,
    Restore,
    Refresh,
    SizeSlider = 201,
    HotkeyControl,
    ApplySettings,
    AutorunControl,
    SnapControl
};
constexpr UINT TrayMessage = WM_APP + 1, RestoreMessage = WM_APP + 7;
constexpr UINT AnimationTimer = 10;
constexpr UINT BenchmarkTimer = 20;
std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n =
        WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string r(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), r.data(), n, nullptr, nullptr);
    return r;
}
std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), r.data(), n);
    return r;
}
std::wstring duration(const beer::QuotaWindow &w) {
    if (!w.minutes)
        return L"окно без длительности";
    auto n = *w.minutes;
    if (n % 1440 == 0)
        return std::to_wstring(n / 1440) + L" дн.";
    if (n % 60 == 0)
        return std::to_wstring(n / 60) + L" ч";
    return std::to_wstring(n) + L" мин";
}
std::wstring timestamp(int64_t value) {
    time_t raw = value;
    tm local{};
    if (localtime_s(&local, &raw))
        return L"неизвестно";
    wchar_t text[80]{};
    wcsftime(text, 80, L"%d.%m %H:%M", &local);
    return text;
}
struct App {
    std::string benchmark;
    int benchmark_seconds = 30;
    uint64_t frame_count = 0, benchmark_frames = 0;
    bool benchmark_measuring = false;
    beer::Json benchmark_before;
    std::chrono::steady_clock::time_point startup = std::chrono::steady_clock::now(), benchmark_start{};
    double first_frame_ms = -1;
    HWND window{}, overlay{};
    HWND settings_window{};
    std::unique_ptr<beer::Renderer> renderer;
    std::unique_ptr<beer::QuotaWorker> worker;
    beer::QuotaState state;
    beer::LevelTransition transition;
    std::optional<beer::QuotaWindow> previous_window;
    std::string previous_group;
    bool demo = false, suspended = false, session_locked = false, display_off = false;
    int animation_interval = 0;
    double current_remaining = -1, other_remaining = -1;
    HWND tooltip{};
    std::wstring tooltip_text;
    HPOWERNOTIFY power_notify{};
    std::chrono::steady_clock::time_point phase_start = std::chrono::steady_clock::now();
    beer::Settings settings;
    std::unique_ptr<beer::SettingsWindow> panel;
    beer::Theme &theme = settings.theme;
    bool &top = settings.top;
    bool &locked = settings.locked;
    bool &clicks = settings.click_through;
    int &height = settings.height;
    int hotkey_id = 1;
    bool hotkey_ok = false, quitting = false;
    UINT taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    bool active() const { return settings.visible && !suspended && !session_locked && !display_off; }
    void benchmark_tick() {
        if (!benchmark_measuring) {
            benchmark_measuring = true;
            benchmark_before = beer::process_resources();
            benchmark_frames = frame_count;
            benchmark_start = std::chrono::steady_clock::now();
            SetTimer(window, BenchmarkTimer, benchmark_seconds * 1000, nullptr);
            return;
        }
        KillTimer(window, BenchmarkTimer);
        const double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - benchmark_start).count();
        auto after = beer::process_resources();
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        beer::Json report = {
            {"mode", benchmark},
            {"seconds", seconds},
            {"heightDip", height},
            {"dpi", GetDpiForWindow(window)},
            {"softwareRenderer", renderer->software()},
            {"firstFrameMs", first_frame_ms},
            {"frames", frame_count - benchmark_frames},
            {"framesPerSecond", (frame_count - benchmark_frames) / seconds},
            {"before", benchmark_before},
            {"after", after},
            {"logicalProcessors", system.dwNumberOfProcessors},
            {"averageCpuOneCorePercent",
             100 * (after["cpuSeconds"].get<double>() - benchmark_before["cpuSeconds"].get<double>()) /
                 seconds},
            {"serviceProcesses", worker ? worker->resources() : beer::Json{{"activeProcesses", 0}}}};
        std::filesystem::create_directories(".local");
        std::ofstream output(std::filesystem::path(".local") / ("benchmark-" + benchmark + ".json"));
        output << report.dump(2);
        output.close();
        DestroyWindow(window);
    }
    void schedule() {
        const bool animated =
            active() && ((current_remaining >= 0 && transition.active()) ||
                         (current_remaining > 0 && theme.decoration && settings.performance > 0 &&
                          (theme.bubbles > 0 || theme.waves > 0)));
        const int interval = animated ? (settings.performance == 2 ? 17 : 34) : 0;
        if (interval != animation_interval) {
            KillTimer(window, AnimationTimer);
            animation_interval = interval;
            if (interval)
                SetTimer(window, AnimationTimer, interval, nullptr);
        }
    }
    void lifecycle() {
        if (worker)
            worker->set_visible(active());
        schedule();
        if (active())
            paint();
    }
    void update_view() {
        const auto *group = state.select_group(settings.group);
        const beer::QuotaWindow *selected = nullptr;
        other_remaining = -1;
        if (group && !group->windows.empty()) {
            const size_t index =
                std::min(static_cast<size_t>(settings.window_index), group->windows.size() - 1);
            selected = &group->windows[index];
            if (group->windows.size() > 1)
                other_remaining = group->windows[index == 0 ? 1 : 0].remaining.value_or(-1);
        }
        const double next = selected ? selected->remaining.value_or(-1) : -1;
        const bool changed = !selected || !previous_window || selected->slot != previous_window->slot ||
                             !group || group->id != previous_group;
        if (next >= 0 && (next != current_remaining || changed)) {
            const bool reset =
                !changed && previous_window && beer::confirmed_reset(*previous_window, *selected);
            transition.set(next, reset ? std::min(1.2f, theme.transition_seconds) : theme.transition_seconds,
                           changed || current_remaining < 0);
        }
        current_remaining = next;
        if (selected) {
            previous_window = *selected;
            previous_group = group->id;
        } else {
            previous_window.reset();
            previous_group.clear();
        }
        tooltip_text = demo ? L"Демонстрационные данные\n" : L"Лимиты Codex\n";
        if (group)
            for (const auto &quota : group->windows) {
                tooltip_text +=
                    duration(quota) + L": " +
                    (quota.remaining
                         ? std::to_wstring(static_cast<int>(std::lround(*quota.remaining))) + L"% осталось"
                         : L"нет данных");
                tooltip_text +=
                    L"\nСброс: " + (quota.resets_at ? timestamp(*quota.resets_at) : L"неизвестно") + L"\n";
            }
        if (state.updated.time_since_epoch().count())
            tooltip_text +=
                L"Обновлено: " + timestamp(std::chrono::system_clock::to_time_t(state.updated)) + L"\n";
        if (state.stale)
            tooltip_text += L"Устарело: " + wide(state.error);
        if (tooltip) {
            TOOLINFOW info{sizeof(info)};
            info.hwnd = window;
            info.uId = 1;
            info.lpszText = tooltip_text.data();
            SendMessageW(tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&info));
        }
        if (panel) {
            std::vector<std::pair<std::string, std::wstring>> groups;
            for (const auto &[id, entry] : state.groups)
                groups.emplace_back(id, wide(entry.name));
            panel->status(tooltip_text, groups);
        }
        schedule();
        paint();
    }
    void tip(bool visible) {
        if (!tooltip)
            return;
        TOOLINFOW info{sizeof(info)};
        info.hwnd = window;
        info.uId = 1;
        if (visible) {
            POINT p{};
            GetCursorPos(&p);
            SendMessageW(tooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(p.x + 14, p.y + 18));
        }
        SendMessageW(tooltip, TTM_TRACKACTIVATE, visible, reinterpret_cast<LPARAM>(&info));
    }
    void save() {
        if (!benchmark.empty())
            return;
        try {
            RECT rc{};
            GetWindowRect(window, &rc);
            settings.x = rc.left;
            settings.y = rc.top;
            MONITORINFOEXW monitor{};
            monitor.cbSize = sizeof(monitor);
            GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
            settings.monitor = utf8(monitor.szDevice);
            beer::save_settings(settings);
        } catch (const std::exception &) {
            MessageBoxW(settings_window ? settings_window : window, L"Не удалось сохранить настройки.",
                        L"Codex Beer Widget", MB_ICONWARNING);
        }
    }
    void tray(bool remove = false) {
        NOTIFYICONDATAW icon{};
        icon.cbSize = sizeof(icon);
        icon.hWnd = window;
        icon.uID = 1;
        icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        icon.uCallbackMessage = TrayMessage;
        icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(icon.szTip, L"Codex Beer Widget — показать / скрыть");
        Shell_NotifyIconW(remove ? NIM_DELETE : NIM_ADD, &icon);
        if (!remove) {
            icon.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &icon);
        }
    }
    void show(bool visible, bool persist = true) {
        settings.visible = visible;
        ShowWindow(window, visible && !clicks ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible && !clicks)
            SetWindowPos(window, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        if (visible && clicks)
            paint();
        ShowWindow(overlay, visible && clicks ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible)
            paint();
        else
            tip(false);
        lifecycle();
        if (persist)
            save();
    }
    void recover(bool primary = false) {
        RECT rc{};
        GetWindowRect(window, &rc);
        HMONITOR monitor = primary ? MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY)
                                   : MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(monitor, &info);
        int x = std::clamp(rc.left, info.rcWork.left,
                           (std::max)(info.rcWork.left, info.rcWork.right - (rc.right - rc.left)));
        int y = std::clamp(rc.top, info.rcWork.top,
                           (std::max)(info.rcWork.top, info.rcWork.bottom - (rc.bottom - rc.top)));
        if (primary) {
            x = info.rcWork.right - (rc.right - rc.left) - 24;
            y = info.rcWork.bottom - (rc.bottom - rc.top) - 24;
        }
        SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        size(height);
        if (primary) {
            clicks = false;
            show(true);
        }
        save();
    }
    bool register_hotkey(unsigned modifiers, unsigned key) {
        if (hotkey_ok && modifiers == settings.hotkey_modifiers && key == settings.hotkey)
            return true;
        const int candidate = hotkey_id == 1 ? 2 : 1;
        if (!(modifiers & (MOD_CONTROL | MOD_ALT)) || !key ||
            !RegisterHotKey(window, candidate, modifiers | MOD_NOREPEAT, key))
            return false;
        if (hotkey_ok)
            UnregisterHotKey(window, hotkey_id);
        hotkey_id = candidate;
        hotkey_ok = true;
        settings.hotkey_modifiers = modifiers;
        settings.hotkey = key;
        return true;
    }
    void open_settings() {
        if (!panel) {
            beer::SettingsActions actions;
            actions.changed = [this](bool persist) {
                size(height);
                show(settings.visible, false);
                update_view();
                if (persist)
                    save();
            };
            actions.hotkey = [this](unsigned modifiers, unsigned key) {
                return register_hotkey(modifiers, key);
            };
            actions.refresh = [this] {
                if (worker)
                    worker->refresh();
            };
            actions.restore = [this] { recover(true); };
            actions.toggle_visibility = [this] { show(!settings.visible); };
            actions.closed = [this] { settings_window = nullptr; };
            actions.is_demo = [this] { return demo; };
            actions.demonstration = [this](bool value) {
                worker.reset();
                demo = value;
                state = beer::QuotaState{};
                previous_window.reset();
                current_remaining = -1;
                update_view();
                worker = std::make_unique<beer::QuotaWorker>(window, demo, active());
            };
            panel = std::make_unique<beer::SettingsWindow>(settings, std::move(actions));
        }
        panel->open();
        settings_window = panel->window();
        SetWindowPos(settings_window, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        update_view();
    }
    void region() {
        RECT rc{};
        GetClientRect(window, &rc);
        const double sx = rc.right / 240., sy = rc.bottom / 300.;
        auto round = [&](int l, int t, int r, int b, int radius) {
            return CreateRoundRectRgn(int(l * sx), int(t * sy), int(r * sx) + 1, int(b * sy) + 1,
                                      int(radius * sx), int(radius * sy));
        };
        HRGN result = round(37, 32, 178, 234, 42), handle = round(158, 77, 214, 184, 40),
             hole = round(172, 91, 201, 170, 26);
        if (theme.ring) {
            DeleteObject(result);
            result = CreateEllipticRgn(int(35 * sx), int(45 * sy), int(205 * sx) + 1, int(215 * sy) + 1);
            HRGN center = CreateEllipticRgn(int(50 * sx), int(60 * sy), int(190 * sx), int(200 * sy));
            CombineRgn(result, result, center, RGN_DIFF);
            DeleteObject(center);
        } else {
            CombineRgn(handle, handle, hole, RGN_DIFF);
            CombineRgn(result, result, handle, RGN_OR);
        }
        HRGN label = round(26, 240, 214, 300, 25);
        CombineRgn(result, result, label, RGN_OR);
        DeleteObject(handle);
        DeleteObject(hole);
        DeleteObject(label);
        if (!SetWindowRgn(window, result, TRUE))
            DeleteObject(result);
    }
    void paint() {
        if (!renderer || !active())
            return;
        std::wstring label =
            theme.show_percent
                ? (current_remaining >= 0
                       ? std::to_wstring(static_cast<int>(std::lround(current_remaining))) + L"%"
                       : L"нет данных")
                : L"Codex";
        if (demo)
            label = L"Демо · " + label;
        std::wstring caption = state.stale       ? L"устарело"
                               : previous_window ? duration(*previous_window) + L" · остаток"
                                                 : L"ожидание данных";
        auto drawing = theme;
        // Economy freezes decorative positions even during a level transition.
        drawing.decoration = theme.decoration && settings.performance > 0;
        const double phase =
            drawing.decoration
                ? std::chrono::duration<double>(std::chrono::steady_clock::now() - phase_start).count()
                : 0;
        renderer->draw(drawing,
                       current_remaining >= 0 ? transition.value(std::chrono::steady_clock::now()) : -1,
                       other_remaining, label, caption, phase, clicks ? overlay : nullptr);
        ++frame_count;
        if (first_frame_ms < 0)
            first_frame_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startup).count();
    }
    void size(int value) {
        height = std::clamp(value, 80, 400);
        const UINT dpi = GetDpiForWindow(window);
        const int h = MulDiv(height, dpi, 96), w = MulDiv(h, 240, 300);
        SetWindowPos(window, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE);
        RECT rc{};
        GetWindowRect(window, &rc);
        SetWindowPos(overlay, top ? HWND_TOPMOST : HWND_NOTOPMOST, rc.left, rc.top, w, h, SWP_NOACTIVATE);
        if (settings_window && IsWindowVisible(settings_window))
            SetWindowPos(settings_window, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        region();
        paint();
    }
    void click_mode() {
        clicks = !clicks;
        RECT rc{};
        GetWindowRect(window, &rc);
        SetWindowPos(overlay, top ? HWND_TOPMOST : HWND_NOTOPMOST, rc.left, rc.top, rc.right - rc.left,
                     rc.bottom - rc.top, SWP_NOACTIVATE);
        show(settings.visible);
    }
    void menu() {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, ToggleShow, settings.visible ? L"Скрыть" : L"Показать");
        AppendMenuW(menu, MF_STRING, OpenSettings, L"Настройки…");
        AppendMenuW(menu, MF_STRING, Refresh, L"Обновить лимиты");
        AppendMenuW(menu, MF_STRING | (top ? MF_CHECKED : 0), ToggleTop, L"Поверх остальных окон");
        AppendMenuW(menu, MF_STRING | (locked ? MF_CHECKED : 0), ToggleLock, L"Зафиксировать положение");
        AppendMenuW(menu, MF_STRING | (clicks ? MF_CHECKED : 0), ToggleClicks, L"Пропускать клики");
        AppendMenuW(menu, MF_STRING, Restore, L"Вернуть на основной экран");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, Quit, L"Выход");
        POINT pt{};
        GetCursorPos(&pt);
        SetForegroundWindow(window);
        const int command =
            TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, window, nullptr);
        DestroyMenu(menu);
        if (command == ToggleTop) {
            top = !top;
            size(height);
        }
        if (command == ToggleLock)
            locked = !locked;
        if (command == ToggleShow)
            show(!settings.visible);
        if (command == OpenSettings)
            open_settings();
        if (command == Restore)
            recover(true);
        if (command == Refresh && worker)
            worker->refresh();
        if (command == ToggleClicks)
            click_mode();
        if (command == Quit)
            DestroyWindow(window);
        if (command != Quit)
            save();
        PostMessageW(window, WM_NULL, 0, 0);
    }
};
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto *app = reinterpret_cast<App *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->window = window;
    }
    if (!app)
        return DefWindowProcW(window, message, w, l);
    if (message == app->taskbar_created) {
        app->tray();
        return 0;
    }
    try {
        switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ScreenToClient(window, &pt);
            RECT rc{};
            GetClientRect(window, &rc);
            return !app->locked && pt.y < rc.bottom * 240 / 300 ? HTCAPTION : HTCLIENT;
        }
        case WM_MOUSEWHEEL:
            if (GET_KEYSTATE_WPARAM(w) & MK_CONTROL) {
                app->size(app->height + GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 10);
                return 0;
            }
            break;
        case WM_DPICHANGED: {
            auto *rc = reinterpret_cast<RECT *>(l);
            SetWindowPos(window, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            app->size(app->height);
            return 0;
        }
        case WM_SIZE:
            if (app->renderer) {
                app->renderer->resize(LOWORD(l), HIWORD(l));
                app->region();
                app->paint();
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(window, &ps);
            EndPaint(window, &ps);
            app->paint();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CONTEXTMENU:
            app->menu();
            return 0;
        case WM_LBUTTONUP:
            app->settings.window_index = app->settings.window_index ? 0 : 1;
            app->update_view();
            app->save();
            return 0;
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tracking{
                sizeof(tracking),
                static_cast<DWORD>(TME_LEAVE | (message == WM_NCMOUSEMOVE ? TME_NONCLIENT : 0)), window, 0};
            TrackMouseEvent(&tracking);
            app->tip(true);
            break;
        }
        case WM_NCMOUSELEAVE:
        case WM_MOUSELEAVE:
            app->tip(false);
            return 0;
        case beer::DataMessage:
            if (app->worker) {
                if (auto state = app->worker->take()) {
                    app->state = std::move(*state);
                    app->update_view();
                }
            }
            return 0;
        case WM_TIMER:
            if (w == BenchmarkTimer) {
                app->benchmark_tick();
                return 0;
            }
            if (w == AnimationTimer) {
                app->paint();
                app->schedule();
            }
            return 0;
        case WM_WTSSESSION_CHANGE:
            if (w == WTS_SESSION_LOCK)
                app->session_locked = true;
            if (w == WTS_SESSION_UNLOCK)
                app->session_locked = false;
            app->lifecycle();
            return 0;
        case WM_POWERBROADCAST:
            if (w == PBT_APMSUSPEND)
                app->suspended = true;
            if (w == PBT_APMRESUMEAUTOMATIC || w == PBT_APMRESUMESUSPEND) {
                app->suspended = false;
                if (app->worker)
                    app->worker->refresh();
            }
            if (w == PBT_POWERSETTINGCHANGE) {
                const auto *setting = reinterpret_cast<POWERBROADCAST_SETTING *>(l);
                if (IsEqualGUID(setting->PowerSetting, GUID_CONSOLE_DISPLAY_STATE) &&
                    setting->DataLength == sizeof(DWORD)) {
                    DWORD value{};
                    memcpy(&value, setting->Data, sizeof(value));
                    app->display_off = value == 0;
                }
            }
            app->lifecycle();
            return TRUE;
        case WM_HOTKEY:
            app->show(!app->settings.visible);
            return 0;
        case TrayMessage:
            if (LOWORD(l) == WM_CONTEXTMENU)
                app->menu();
            else if (LOWORD(l) == NIN_SELECT || LOWORD(l) == NIN_KEYSELECT)
                app->show(!app->settings.visible);
            return 0;
        case RestoreMessage:
            app->recover(true);
            return 0;
        case WM_EXITSIZEMOVE:
            app->recover();
            return 0;
        case WM_MOVING:
            if (app->settings.snap) {
                auto *rc = reinterpret_cast<RECT *>(l);
                MONITORINFO info{sizeof(info)};
                GetMonitorInfoW(MonitorFromRect(rc, MONITOR_DEFAULTTONEAREST), &info);
                const int distance = MulDiv(12, GetDpiForWindow(window), 96), width = rc->right - rc->left,
                          height = rc->bottom - rc->top;
                if (abs(rc->left - info.rcWork.left) < distance) {
                    rc->left = info.rcWork.left;
                    rc->right = rc->left + width;
                }
                if (abs(rc->right - info.rcWork.right) < distance) {
                    rc->right = info.rcWork.right;
                    rc->left = rc->right - width;
                }
                if (abs(rc->top - info.rcWork.top) < distance) {
                    rc->top = info.rcWork.top;
                    rc->bottom = rc->top + height;
                }
                if (abs(rc->bottom - info.rcWork.bottom) < distance) {
                    rc->bottom = info.rcWork.bottom;
                    rc->top = rc->bottom - height;
                }
            }
            return TRUE;
        case WM_DISPLAYCHANGE:
            app->recover();
            return 0;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            app->quitting = true;
            KillTimer(window, AnimationTimer);
            KillTimer(window, BenchmarkTimer);
            app->worker.reset();
            app->save();
            app->tray(true);
            if (app->tooltip)
                DestroyWindow(app->tooltip);
            if (app->settings_window)
                DestroyWindow(app->settings_window);
            WTSUnRegisterSessionNotification(window);
            if (app->power_notify)
                UnregisterPowerSettingNotification(app->power_notify);
            UnregisterHotKey(window, app->hotkey_id);
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception &) {
        MessageBoxW(window, L"Не удалось создать или обновить графику Direct2D.", L"Codex Beer Widget",
                    MB_ICONERROR);
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR arguments, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\CodexBeerWidget.Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(window_class, nullptr))
            PostMessageW(existing,
                         std::wstring(arguments).find(L"--quit") != std::wstring::npos ? WM_CLOSE
                                                                                       : RestoreMessage,
                         0, 0);
        if (singleton)
            CloseHandle(singleton);
        CoUninitialize();
        return 0;
    }
    if (std::wstring(arguments).find(L"--quit") != std::wstring::npos) {
        if (singleton)
            CloseHandle(singleton);
        CoUninitialize();
        return 0;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_HOTKEY_CLASS | ICC_TAB_CLASSES |
                                                        ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    App app;
    std::string settings_error;
    app.settings = beer::load_settings(settings_error);
    app.demo = std::wstring(arguments).find(L"--demo") != std::wstring::npos;
    const std::wstring command_line = arguments;
    if (auto position = command_line.find(L"--benchmark="); position != std::wstring::npos) {
        const auto start = position + 12, end = command_line.find(L' ', start);
        app.benchmark = utf8(command_line.substr(start, end - start));
        const std::vector<std::string> modes = {"hidden", "static",       "normal",
                                                "smooth", "clickthrough", "live"};
        if (std::find(modes.begin(), modes.end(), app.benchmark) == modes.end()) {
            CloseHandle(singleton);
            CoUninitialize();
            return 2;
        }
        app.settings = beer::Settings{};
        app.demo = app.benchmark != "live";
        settings_error.clear();
        app.settings.visible = app.benchmark != "hidden";
        app.settings.performance =
            app.benchmark == "smooth"
                ? 2
                : (app.benchmark == "normal" || app.benchmark == "clickthrough" ? 1 : 0);
        app.settings.click_through = app.benchmark == "clickthrough";
        if (auto at = command_line.find(L"--seconds="); at != std::wstring::npos) {
            try {
                app.benchmark_seconds = std::clamp(std::stoi(command_line.substr(at + 10)), 2, 600);
            } catch (...) {
            }
        }
    }
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance;
    wc.lpszClassName = window_class;
    wc.lpfnWndProc = procedure;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    const bool inspect = std::wstring(arguments).find(L"--inspect") != std::wstring::npos;
    HWND window = CreateWindowExW((inspect ? WS_EX_APPWINDOW : (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) |
                                      WS_EX_NOREDIRECTIONBITMAP,
                                  window_class, L"Codex Beer Widget", WS_POPUP, app.settings.x,
                                  app.settings.y, 192, 240, nullptr, nullptr, instance, &app);
    if (!window) {
        CoUninitialize();
        return 1;
    }
    app.overlay = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                  L"STATIC", L"Codex Beer Widget — click through", WS_POPUP, 120, 120, 192,
                                  240, nullptr, nullptr, instance, nullptr);
    try {
        app.renderer = std::make_unique<beer::Renderer>(window);
        app.tray();
        if (!app.register_hotkey(app.settings.hotkey_modifiers, app.settings.hotkey))
            MessageBoxW(window,
                        L"Сохранённая горячая клавиша занята. Выберите другую в настройках через трей.",
                        L"Codex Beer Widget", MB_ICONWARNING);
        app.size(app.height);
        app.recover();
        app.show(app.settings.visible);
        app.tooltip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
                                      WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, window, nullptr,
                                      instance, nullptr);
        TOOLINFOW info{sizeof(info)};
        info.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        info.hwnd = window;
        info.uId = 1;
        info.lpszText = const_cast<LPWSTR>(L"Ожидание данных");
        SendMessageW(app.tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
        SendMessageW(app.tooltip, TTM_SETMAXTIPWIDTH, 0, 420);
        WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
        app.power_notify = RegisterPowerSettingNotification(window, &GUID_CONSOLE_DISPLAY_STATE,
                                                            DEVICE_NOTIFY_WINDOW_HANDLE);
        if (app.benchmark.empty() || app.benchmark == "live")
            app.worker = std::make_unique<beer::QuotaWorker>(window, app.demo, app.active());
        else {
            app.state.accept({{"rateLimits",
                               {{"limitId", "codex"},
                                {"primary", {{"usedPercent", 30}, {"windowDurationMins", 300}}},
                                {"secondary", {{"usedPercent", 45}, {"windowDurationMins", 10080}}}}}});
            app.update_view();
        }
        if (!app.benchmark.empty())
            SetTimer(window, BenchmarkTimer, 2000, nullptr);
        if (std::wstring(arguments).find(L"--settings") != std::wstring::npos)
            app.open_settings();
        if (!settings_error.empty())
            MessageBoxW(window, L"Не удалось прочитать настройки. Использованы значения по умолчанию.",
                        L"Codex Beer Widget", MB_ICONWARNING);
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!app.settings_window || !IsDialogMessageW(app.settings_window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    } catch (const std::exception &) {
        MessageBoxW(window, L"Не удалось запустить Direct2D/DirectComposition.", L"Codex Beer Widget",
                    MB_ICONERROR);
    }
    DestroyWindow(app.overlay);
    app.renderer.reset();
    if (IsWindow(window))
        DestroyWindow(window);
    if (singleton)
        CloseHandle(singleton);
    CoUninitialize();
    return 0;
}
