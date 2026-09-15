#include "settings_ui.hpp"
#include "icons.hpp"
#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <objbase.h>

namespace beer {
namespace {
enum Id {
    Tabs = 300,
    Glass,
    Liquid,
    Foam,
    Waves,
    Bubbles,
    Transition,
    TextSize,
    Size,
    Color,
    TextColor,
    Shape,
    Performance,
    Decor,
    Percent,
    PresetName,
    SavePreset,
    LoadPreset,
    Defaults,
    Top,
    Lock,
    Clicks,
    Snap,
    Autorun,
    Hotkey,
    ApplyHotkey,
    Restore,
    Show,
    Group,
    MainWindow,
    Refresh,
    Demo,
    Status,
    RefreshInterval
};
std::wstring to_wide(const std::string &s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n);
    return result;
}
std::string to_utf8(const std::wstring &s) {
    int n =
        WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n, nullptr, nullptr);
    return result;
}
} // namespace
SettingsWindow::SettingsWindow(Settings &settings, SettingsActions actions)
    : settings_(settings), actions_(std::move(actions)) {}
SettingsWindow::~SettingsWindow() {
    if (window_)
        DestroyWindow(window_);
    if (font_)
        DeleteObject(font_);
}
void SettingsWindow::open() {
    if (!window_) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CodexBeerWidget.Preferences";
        wc.lpfnWndProc = procedure;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = app_icon();
        wc.hIconSm = app_icon(true);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassExW(&wc);
        dpi_ = GetDpiForSystem();
        window_ = CreateWindowExW(0, wc.lpszClassName, L"Настройки Codex Beer Widget",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                                  MulDiv(720, dpi_, 96), MulDiv(665, dpi_, 96), nullptr, nullptr,
                                  wc.hInstance, this);
        if (!window_)
            throw std::runtime_error("Settings window creation failed");
    }
    ShowWindow(window_, SW_SHOWNORMAL);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    SetForegroundWindow(window_);
}
HWND SettingsWindow::add(const wchar_t *type, const wchar_t *text, DWORD style, int id, int page, int x,
                         int y, int w, int h) {
    HWND control = CreateWindowExW(
        type == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0,
        0, 0, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!control)
        throw std::runtime_error("Settings control creation failed");
    controls_.push_back({control, page, x, y, w, h});
    return control;
}
void SettingsWindow::slider(int id, int p, const wchar_t *label, int x, int y, int low, int high) {
    add(L"STATIC", label, 0, 0, p, x, y, 300, 22);
    auto control = add(TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | WS_TABSTOP, id, p, x, y + 25, 300, 32);
    SendMessageW(control, TBM_SETRANGE, TRUE, MAKELPARAM(low, high));
    SendMessageW(control, TBM_SETTICFREQ, (high - low) / 4, 0);
}
void SettingsWindow::create_controls() {
    tabs_ = add(WC_TABCONTROLW, L"", WS_TABSTOP, Tabs, -1, 16, 12, 680, 36);
    for (const auto *title : {L"Внешний вид", L"Окно", L"Лимиты"}) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(title);
        TabCtrl_InsertItem(tabs_, TabCtrl_GetItemCount(tabs_), &item);
    }
    slider(Glass, 0, L"Непрозрачность стекла (0–100%)", 24, 75, 0, 100);
    slider(Liquid, 0, L"Плотность цвета жидкости (0–100%)", 24, 145, 0, 100);
    slider(Foam, 0, L"Количество пены", 24, 215, 0, 100);
    slider(Waves, 0, L"Интенсивность волн", 24, 285, 0, 100);
    slider(Bubbles, 0, L"Пузырьки (0–24)", 24, 355, 0, 24);
    slider(Transition, 0, L"Время перехода (0,1–5 секунд)", 24, 425, 1, 50);
    add(L"BUTTON", L"Цвет жидкости…", BS_PUSHBUTTON | WS_TABSTOP, Color, 0, 368, 75, 300, 32);
    add(L"BUTTON", L"Цвет текста…", BS_PUSHBUTTON | WS_TABSTOP, TextColor, 0, 368, 118, 300, 32);
    add(L"STATIC", L"Оформление", 0, 0, 0, 368, 166, 300, 22);
    auto shape = add(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, Shape, 0, 368, 191, 300, 100);
    SendMessageW(shape, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Пивная кружка"));
    SendMessageW(shape, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Минималистическое кольцо"));
    add(L"STATIC", L"Режим анимации", 0, 0, 0, 368, 238, 300, 22);
    auto mode = add(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, Performance, 0, 368, 263, 300, 120);
    for (const auto *label :
         {L"Экономичный — только смена уровня", L"Обычный — до 30 кадров/с", L"Плавный — до 60 кадров/с"})
        SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    slider(TextSize, 0, L"Размер текста (10–28)", 368, 325, 10, 28);
    add(L"BUTTON", L"Показывать проценты", BS_AUTOCHECKBOX | WS_TABSTOP, Percent, 0, 368, 410, 300, 28);
    add(L"BUTTON", L"Декоративная анимация", BS_AUTOCHECKBOX | WS_TABSTOP, Decor, 0, 368, 447, 300, 28);
    add(L"STATIC", L"Изменения сразу видны на виджете", 0, 0, 0, 24, 500, 640, 24);
    auto name = add(WC_COMBOBOXW, L"", CBS_DROPDOWN | WS_TABSTOP, PresetName, 0, 24, 538, 300, 150);
    SendMessageW(name, CB_LIMITTEXT, 64, 0);
    add(L"BUTTON", L"Сохранить пресет", BS_PUSHBUTTON | WS_TABSTOP, SavePreset, 0, 338, 538, 158, 30);
    add(L"BUTTON", L"Загрузить", BS_PUSHBUTTON | WS_TABSTOP, LoadPreset, 0, 510, 538, 158, 30);
    add(L"BUTTON", L"Стандартное оформление", BS_PUSHBUTTON | WS_TABSTOP, Defaults, 0, 24, 580, 300, 28);

    slider(Size, 1, L"Высота виджета: 80–400 DIP", 24, 75, 80, 400);
    const std::pair<int, const wchar_t *> checks[] = {
        {Top, L"Поверх остальных окон"},
        {Lock, L"Зафиксировать положение"},
        {Clicks, L"Пропускать клики к приложениям под виджетом"},
        {Snap, L"Привязывать к краям экрана"},
        {Autorun, L"Запускать при входе в Windows"}};
    int y = 158;
    for (const auto &[id, label] : checks) {
        add(L"BUTTON", label, BS_AUTOCHECKBOX | WS_TABSTOP, id, 1, 24, y, 630, 28);
        y += 38;
    }
    add(L"STATIC", L"Горячая клавиша показа / скрытия", 0, 0, 1, 24, 367, 600, 24);
    add(HOTKEY_CLASSW, L"", WS_TABSTOP, Hotkey, 1, 24, 400, 300, 30);
    add(L"BUTTON", L"Применить клавишу", BS_PUSHBUTTON | WS_TABSTOP, ApplyHotkey, 1, 350, 400, 300, 30);
    add(L"BUTTON", L"Вернуть на основной экран", BS_PUSHBUTTON | WS_TABSTOP, Restore, 1, 24, 470, 300, 32);
    add(L"BUTTON", L"Показать / скрыть", BS_PUSHBUTTON | WS_TABSTOP, Show, 1, 350, 470, 300, 32);
    add(L"STATIC", L"Трей и повторный запуск помогают вернуть скрытый виджет.", 0, 0, 1, 24, 530, 640, 48);

    add(L"STATIC", L"Группа лимитов", 0, 0, 2, 24, 75, 620, 24);
    add(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, Group, 2, 24, 106, 640, 140);
    add(L"STATIC", L"Основной показатель", 0, 0, 2, 24, 157, 620, 24);
    auto main = add(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, MainWindow, 2, 24, 188, 640, 110);
    for (const auto *label : {L"Короткое окно", L"Длинное окно"})
        SendMessageW(main, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    add(L"STATIC", L"Обновление при показе", 0, 0, 2, 350, 231, 290, 24);
    auto frequency =
        add(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, RefreshInterval, 2, 350, 258, 290, 160);
    for (int seconds : refresh_choices) {
        auto label = std::to_wstring(seconds) + L" с";
        SendMessageW(frequency, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    add(L"BUTTON", L"Обновить сейчас", BS_PUSHBUTTON | WS_TABSTOP, Refresh, 2, 24, 239, 300, 32);
    add(L"BUTTON", L"Демонстрация без обращения к Codex", BS_AUTOCHECKBOX | WS_TABSTOP, Demo, 2, 24, 290, 640,
        28);
    add(L"EDIT", L"Ожидание данных…", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
        Status, 2, 24, 340, 640, 200);
    add(L"STATIC", L"Остаток — процент лимита. В точные токены он не пересчитывается.", 0, 0, 2, 24, 558, 640,
        45);
    presets();
    sync();
    layout();
    page();
}
void SettingsWindow::layout() {
    dpi_ = GetDpiForWindow(window_);
    HFONT old = font_;
    font_ =
        CreateFontW(-MulDiv(10, dpi_, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    for (const auto &c : controls_) {
        SetWindowPos(c.window, nullptr, MulDiv(c.x, dpi_, 96), MulDiv(c.y, dpi_, 96), MulDiv(c.w, dpi_, 96),
                     MulDiv(c.h, dpi_, 96), SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    if (old)
        DeleteObject(old);
}
void SettingsWindow::page() {
    int selected = TabCtrl_GetCurSel(tabs_);
    for (const auto &c : controls_)
        ShowWindow(c.window, c.page < 0 || c.page == selected ? SW_SHOW : SW_HIDE);
}
void SettingsWindow::sync() {
    const auto &t = settings_.theme;
    const std::pair<int, int> values[] = {{Glass, int(t.glass_alpha * 100)},
                                          {Liquid, int(t.liquid_alpha * 100)},
                                          {Foam, int(t.foam * 100)},
                                          {Waves, int(t.waves * 100)},
                                          {Bubbles, t.bubbles},
                                          {Transition, int(t.transition_seconds * 10)},
                                          {TextSize, int(t.text_size)},
                                          {Size, settings_.height}};
    for (auto [id, value] : values)
        SendDlgItemMessageW(window_, id, TBM_SETPOS, TRUE, value);
    const std::pair<int, bool> checks[] = {{Percent, t.show_percent},
                                           {Decor, t.decoration},
                                           {Top, settings_.top},
                                           {Lock, settings_.locked},
                                           {Clicks, settings_.click_through},
                                           {Snap, settings_.snap},
                                           {Autorun, settings_.autorun},
                                           {Demo, actions_.is_demo()}};
    for (auto [id, value] : checks)
        CheckDlgButton(window_, id, value ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(window_, Shape, CB_SETCURSEL, t.ring ? 1 : 0, 0);
    SendDlgItemMessageW(window_, Performance, CB_SETCURSEL, settings_.performance, 0);
    SendDlgItemMessageW(window_, MainWindow, CB_SETCURSEL, settings_.window_index, 0);
    const auto frequency =
        std::find(refresh_choices.begin(), refresh_choices.end(), settings_.refresh_seconds);
    SendDlgItemMessageW(window_, RefreshInterval, CB_SETCURSEL, frequency - refresh_choices.begin(), 0);
    unsigned modifiers = ((settings_.hotkey_modifiers & MOD_CONTROL) ? HOTKEYF_CONTROL : 0) |
                         ((settings_.hotkey_modifiers & MOD_ALT) ? HOTKEYF_ALT : 0) |
                         ((settings_.hotkey_modifiers & MOD_SHIFT) ? HOTKEYF_SHIFT : 0);
    SendDlgItemMessageW(window_, Hotkey, HKM_SETHOTKEY, MAKEWORD(settings_.hotkey, modifiers), 0);
}
void SettingsWindow::change(bool persist) {
    actions_.changed(persist);
}
void SettingsWindow::status(const std::wstring &text,
                            const std::vector<std::pair<std::string, std::wstring>> &groups) {
    if (!window_)
        return;
    std::wstring multiline;
    multiline.reserve(text.size() + 16);
    for (const auto character : text) {
        if (character == L'\n')
            multiline += L'\r';
        multiline += character;
    }
    SetDlgItemTextW(window_, Status, multiline.c_str());
    auto available = groups;
    if (!settings_.group.empty() && std::none_of(available.begin(), available.end(), [&](const auto &entry) {
            return entry.first == settings_.group;
        }))
        available.emplace_back(settings_.group, to_wide(settings_.group) + L" (нет данных)");
    if (available == groups_ && SendDlgItemMessageW(window_, Group, CB_GETCOUNT, 0, 0) > 0)
        return;
    groups_ = std::move(available);
    SendDlgItemMessageW(window_, Group, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessageW(window_, Group, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Автоматически"));
    int selected = 0;
    for (size_t i = 0; i < groups_.size(); i++) {
        SendDlgItemMessageW(window_, Group, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(groups_[i].second.c_str()));
        if (groups_[i].first == settings_.group)
            selected = static_cast<int>(i) + 1;
    }
    SendDlgItemMessageW(window_, Group, CB_SETCURSEL, selected, 0);
}
void SettingsWindow::choose_color(bool liquid) {
    static COLORREF custom[16]{};
    auto rgb = liquid ? settings_.theme.liquid_color : settings_.theme.text_color;
    CHOOSECOLORW choice{sizeof(choice)};
    choice.hwndOwner = window_;
    choice.lpCustColors = custom;
    choice.Flags = CC_FULLOPEN | CC_RGBINIT;
    choice.rgbResult = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    if (ChooseColorW(&choice)) {
        rgb = (GetRValue(choice.rgbResult) << 16) | (GetGValue(choice.rgbResult) << 8) |
              GetBValue(choice.rgbResult);
        if (liquid)
            settings_.theme.liquid_color = rgb;
        else
            settings_.theme.text_color = rgb;
        change();
    }
}
void SettingsWindow::presets() {
    preset_files_.clear();
    SendDlgItemMessageW(window_, PresetName, CB_RESETCONTENT, 0, 0);
    auto directory = settings_directory() / L"presets";
    std::filesystem::create_directories(directory);
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != L".json" || entry.file_size() > 65536)
            continue;
        try {
            std::ifstream stream(entry.path());
            Json j;
            stream >> j;
            const auto title = to_wide(j.at("name").get<std::string>());
            if (title.size() > 64)
                continue;
            SendDlgItemMessageW(window_, PresetName, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(title.c_str()));
            preset_files_.push_back(entry.path());
        } catch (...) {
        }
    }
    SetDlgItemTextW(window_, PresetName, L"Мой пресет");
}
void SettingsWindow::save_preset() {
    wchar_t name[65]{};
    GetDlgItemTextW(window_, PresetName, name, 65);
    if (!*name) {
        MessageBoxW(window_, L"Введите название пресета.", L"Пресет", MB_ICONINFORMATION);
        return;
    }
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid)))
        throw std::runtime_error("Cannot create preset id");
    wchar_t id[40]{};
    StringFromGUID2(guid, id, 40);
    auto path = settings_directory() / L"presets" / (std::wstring(id) + L".json");
    std::ofstream stream(path, std::ios::binary);
    stream << Json{{"version", 1}, {"name", to_utf8(name)}, {"theme", settings_to_json(settings_)["theme"]}}
                  .dump(2);
    stream.flush();
    if (!stream)
        throw std::runtime_error("Cannot save preset");
    stream.close();
    presets();
    SetDlgItemTextW(window_, PresetName, name);
}
void SettingsWindow::load_preset() {
    const auto index = SendDlgItemMessageW(window_, PresetName, CB_GETCURSEL, 0, 0);
    if (index < 0 || static_cast<size_t>(index) >= preset_files_.size()) {
        MessageBoxW(window_, L"Выберите сохранённый пресет из списка.", L"Пресет", MB_ICONINFORMATION);
        return;
    }
    std::ifstream stream(preset_files_[index]);
    Json j;
    stream >> j;
    settings_.theme = settings_from_json({{"theme", j.at("theme")}}).theme;
    sync();
    change();
}
LRESULT CALLBACK SettingsWindow::procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto *self = reinterpret_cast<SettingsWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<SettingsWindow *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(window, message, w, l);
    try {
        return self->message(message, w, l);
    } catch (const std::exception &) {
        MessageBoxW(window,
                    L"Не удалось выполнить действие. Настройки и файлы должны быть доступны для записи.",
                    L"Codex Beer Widget", MB_ICONWARNING);
        return message == WM_CREATE ? -1 : 0;
    }
}
LRESULT SettingsWindow::message(UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_CREATE:
        create_controls();
        return 0;
    case WM_DPICHANGED: {
        auto *rc = reinterpret_cast<RECT *>(l);
        SetWindowPos(window_, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        layout();
        return 0;
    }
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR *>(l)->idFrom == Tabs &&
            reinterpret_cast<NMHDR *>(l)->code == TCN_SELCHANGE)
            page();
        return 0;
    case WM_HSCROLL: {
        auto control = reinterpret_cast<HWND>(l);
        int value = static_cast<int>(SendMessageW(control, TBM_GETPOS, 0, 0));
        switch (GetDlgCtrlID(control)) {
        case Glass:
            settings_.theme.glass_alpha = value / 100.f;
            break;
        case Liquid:
            settings_.theme.liquid_alpha = value / 100.f;
            break;
        case Foam:
            settings_.theme.foam = value / 100.f;
            break;
        case Waves:
            settings_.theme.waves = value / 100.f;
            break;
        case Bubbles:
            settings_.theme.bubbles = value;
            break;
        case Transition:
            settings_.theme.transition_seconds = value / 10.f;
            break;
        case TextSize:
            settings_.theme.text_size = static_cast<float>(value);
            break;
        case Size:
            settings_.height = value;
            break;
        default:
            return 0;
        }
        change(LOWORD(w) != TB_THUMBTRACK);
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(w);
        const auto checked = [&](int control) { return IsDlgButtonChecked(window_, control) == BST_CHECKED; };
        if (HIWORD(w) == CBN_SELCHANGE) {
            if (id == RefreshInterval) {
                const auto index = SendDlgItemMessageW(window_, RefreshInterval, CB_GETCURSEL, 0, 0);
                if (index >= 0 && index < static_cast<LRESULT>(refresh_choices.size()))
                    settings_.refresh_seconds = refresh_choices[index];
            } else if (id == Shape)
                settings_.theme.ring = SendDlgItemMessageW(window_, Shape, CB_GETCURSEL, 0, 0) == 1;
            else if (id == Performance)
                settings_.performance =
                    static_cast<int>(SendDlgItemMessageW(window_, Performance, CB_GETCURSEL, 0, 0));
            else if (id == MainWindow)
                settings_.window_index =
                    static_cast<int>(SendDlgItemMessageW(window_, MainWindow, CB_GETCURSEL, 0, 0));
            else if (id == Group) {
                const auto index = SendDlgItemMessageW(window_, Group, CB_GETCURSEL, 0, 0);
                settings_.group =
                    index > 0 && static_cast<size_t>(index) <= groups_.size() ? groups_[index - 1].first : "";
            } else
                return 0;
            change();
            return 0;
        }
        if (HIWORD(w) != BN_CLICKED)
            return 0;
        switch (id) {
        case Color:
            choose_color(true);
            return 0;
        case TextColor:
            choose_color(false);
            return 0;
        case SavePreset:
            save_preset();
            return 0;
        case LoadPreset:
            load_preset();
            return 0;
        case Defaults:
            settings_.theme = Theme{};
            sync();
            break;
        case Percent:
            settings_.theme.show_percent = checked(id);
            break;
        case Decor:
            settings_.theme.decoration = checked(id);
            break;
        case Top:
            settings_.top = checked(id);
            break;
        case Lock:
            settings_.locked = checked(id);
            break;
        case Clicks:
            settings_.click_through = checked(id);
            break;
        case Snap:
            settings_.snap = checked(id);
            break;
        case Autorun:
            try {
                set_autorun(checked(id));
                settings_.autorun = checked(id);
            } catch (...) {
                sync();
                throw;
            }
            break;
        case ApplyHotkey: {
            const auto value = SendDlgItemMessageW(window_, Hotkey, HKM_GETHOTKEY, 0, 0);
            const auto flags = HIBYTE(value);
            const unsigned modifiers = ((flags & HOTKEYF_CONTROL) ? MOD_CONTROL : 0) |
                                       ((flags & HOTKEYF_ALT) ? MOD_ALT : 0) |
                                       ((flags & HOTKEYF_SHIFT) ? MOD_SHIFT : 0);
            if (!actions_.hotkey(modifiers, LOBYTE(value))) {
                MessageBoxW(window_, L"Комбинация занята или недопустима. Используйте Ctrl или Alt.",
                            L"Горячая клавиша", MB_ICONWARNING);
                return 0;
            }
            break;
        }
        case Restore:
            actions_.restore();
            sync();
            return 0;
        case Show:
            actions_.toggle_visibility();
            return 0;
        case Refresh:
            actions_.refresh();
            return 0;
        case Demo:
            actions_.demonstration(checked(id));
            return 0;
        default:
            return 0;
        }
        change();
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor(reinterpret_cast<HDC>(w), GetSysColor(COLOR_WINDOW));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_DESTROY:
        controls_.clear();
        tabs_ = nullptr;
        actions_.closed();
        return 0;
    case WM_NCDESTROY: {
        HWND old = window_;
        window_ = nullptr;
        return DefWindowProcW(old, message, w, l);
    }
    }
    return DefWindowProcW(window_, message, w, l);
}
} // namespace beer
