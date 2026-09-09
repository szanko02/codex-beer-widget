#include "settings.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace beer {
namespace {
template <class T> T number(const Json &j, const char *key, T fallback, T low, T high) {
    if (!j.contains(key) || !j[key].is_number())
        return fallback;
    double value = j[key].get<double>();
    if (!std::isfinite(value))
        return fallback;
    return static_cast<T>(std::clamp(value, static_cast<double>(low), static_cast<double>(high)));
}
bool boolean(const Json &j, const char *key, bool fallback) {
    return j.contains(key) && j[key].is_boolean() ? j[key].get<bool>() : fallback;
}
std::string string(const Json &j, const char *key) {
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>().substr(0, 256) : "";
}
} // namespace
std::filesystem::path settings_directory() {
    wchar_t buffer[32768]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);
    if (!length || length >= 32768)
        throw std::runtime_error("LOCALAPPDATA unavailable");
    return std::filesystem::path(buffer) / L"CodexBeerWidget";
}
Settings settings_from_json(const Json &j) {
    Settings s;
    if (!j.is_object())
        return s;
    s.height = number(j, "height", 240, 80, 400);
    s.x = number(j, "x", 120, -100000, 100000);
    s.y = number(j, "y", 120, -100000, 100000);
    s.performance = number(j, "performance", 0, 0, 2);
    s.window_index = number(j, "windowIndex", 0, 0, 1);
    s.top = boolean(j, "top", true);
    s.locked = boolean(j, "locked", false);
    s.visible = boolean(j, "visible", true);
    s.click_through = boolean(j, "clickThrough", false);
    s.snap = boolean(j, "snap", true);
    s.autorun = boolean(j, "autorun", false);
    s.hotkey = number<unsigned>(j, "hotkey", 'B', 1, 254);
    s.hotkey_modifiers = number<unsigned>(j, "hotkeyModifiers", MOD_CONTROL | MOD_ALT, 0, 7);
    if (!(s.hotkey_modifiers & (MOD_CONTROL | MOD_ALT)))
        s.hotkey_modifiers = MOD_CONTROL | MOD_ALT;
    s.monitor = string(j, "monitor");
    if (j.contains("monitorOffsetX") && j["monitorOffsetX"].is_number() && j.contains("monitorOffsetY") &&
        j["monitorOffsetY"].is_number()) {
        s.monitor_offset_x = number(j, "monitorOffsetX", 0, -100000, 100000);
        s.monitor_offset_y = number(j, "monitorOffsetY", 0, -100000, 100000);
    }
    s.group = string(j, "group");
    const auto t = j.value("theme", Json::object());
    if (t.is_object()) {
        s.theme.liquid_color = number<uint32_t>(t, "liquidColor", 0xeda429, 0, 0xffffff);
        s.theme.text_color = number<uint32_t>(t, "textColor", 0xffffff, 0, 0xffffff);
        s.theme.glass_alpha = number(t, "glassAlpha", .55f, 0.f, 1.f);
        s.theme.liquid_alpha = number(t, "liquidAlpha", .92f, 0.f, 1.f);
        s.theme.foam = number(t, "foam", .65f, 0.f, 1.f);
        s.theme.waves = number(t, "waves", .35f, 0.f, 1.f);
        s.theme.bubbles = number(t, "bubbles", 12, 0, 24);
        s.theme.transition_seconds = number(t, "transitionSeconds", .8f, .1f, 5.f);
        s.theme.text_size = number(t, "textSize", 18.f, 10.f, 28.f);
        s.theme.show_percent = boolean(t, "showPercent", true);
        s.theme.decoration = boolean(t, "decoration", true);
        s.theme.ring = boolean(t, "ring", false);
        if (t.contains("resources") && t["resources"].is_object()) {
            const auto geometry = string(t["resources"], "geometry");
            if (geometry == "ring")
                s.theme.ring = true;
            else if (geometry == "beer-mug")
                s.theme.ring = false;
        }
        if (t.contains("fillArea") && t["fillArea"].is_object()) {
            const auto &area = t["fillArea"];
            s.theme.fill_left = number(area, "left", 47.f, 47.f, 150.f);
            s.theme.fill_top = number(area, "top", 46.f, 46.f, 200.f);
            s.theme.fill_right = number(area, "right", 167.f, s.theme.fill_left + 8, 167.f);
            s.theme.fill_bottom = number(area, "bottom", 220.f, s.theme.fill_top + 8, 220.f);
        }
    }
    return s;
}
Json settings_to_json(const Settings &s) {
    const auto &t = s.theme;
    return {{"version", 1},
            {"height", s.height},
            {"x", s.x},
            {"y", s.y},
            {"performance", s.performance},
            {"top", s.top},
            {"locked", s.locked},
            {"visible", s.visible},
            {"clickThrough", s.click_through},
            {"snap", s.snap},
            {"autorun", s.autorun},
            {"hotkey", s.hotkey},
            {"hotkeyModifiers", s.hotkey_modifiers},
            {"monitor", s.monitor},
            {"monitorOffsetX", s.monitor_offset_x ? Json(*s.monitor_offset_x) : Json(nullptr)},
            {"monitorOffsetY", s.monitor_offset_y ? Json(*s.monitor_offset_y) : Json(nullptr)},
            {"group", s.group},
            {"windowIndex", s.window_index},
            {"theme",
             {{"liquidColor", t.liquid_color},
              {"textColor", t.text_color},
              {"glassAlpha", t.glass_alpha},
              {"liquidAlpha", t.liquid_alpha},
              {"foam", t.foam},
              {"waves", t.waves},
              {"bubbles", t.bubbles},
              {"transitionSeconds", t.transition_seconds},
              {"textSize", t.text_size},
              {"showPercent", t.show_percent},
              {"decoration", t.decoration},
              {"ring", t.ring},
              {"resources", {{"geometry", t.ring ? "ring" : "beer-mug"}}},
              {"fillArea",
               {{"left", t.fill_left},
                {"top", t.fill_top},
                {"right", t.fill_right},
                {"bottom", t.fill_bottom}}}}}};
}
Settings load_settings(std::string &error) {
    try {
        auto file = settings_directory() / L"settings.json";
        if (!std::filesystem::exists(file))
            return {};
        if (std::filesystem::file_size(file) > 65536)
            throw std::runtime_error("Settings file is too large");
        std::ifstream stream(file);
        Json j;
        stream >> j;
        return settings_from_json(j);
    } catch (const std::exception &) {
        error = "Не удалось прочитать настройки; использованы значения по умолчанию";
        return {};
    }
}
void save_settings(const Settings &s) {
    auto directory = settings_directory();
    std::filesystem::create_directories(directory);
    auto temporary = directory / L"settings.tmp";
    auto destination = directory / L"settings.json";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << settings_to_json(s).dump(2);
        stream.flush();
        if (!stream)
            throw std::runtime_error("Cannot write settings");
    }
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace settings");
}
void set_autorun(bool enabled) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr,
                        0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        throw std::runtime_error("Cannot open startup setting");
    LONG result;
    if (enabled) {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr, path, 32768);
        std::wstring command = L"\"" + std::wstring(path) + L"\"";
        result = RegSetValueExW(key, L"CodexBeerWidget", 0, REG_SZ,
                                reinterpret_cast<const BYTE *>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else
        result = RegDeleteValueW(key, L"CodexBeerWidget");
    RegCloseKey(key);
    if (result != ERROR_SUCCESS && !(result == ERROR_FILE_NOT_FOUND && !enabled))
        throw std::runtime_error("Cannot update startup setting");
}
} // namespace beer
