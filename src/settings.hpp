#pragma once
#include "codex_source.hpp"
#include "theme.hpp"
#include <filesystem>

namespace beer {
struct Settings {
    Theme theme;
    int height = 240, x = 120, y = 120, performance = 0;
    bool top = true, locked = false, click_through = false, visible = true, snap = true, autorun = false;
    unsigned hotkey_modifiers = MOD_CONTROL | MOD_ALT, hotkey = 'B';
    std::string monitor, group;
    std::optional<int> monitor_offset_x, monitor_offset_y;
    int window_index = 0;
};
std::filesystem::path settings_directory();
Settings settings_from_json(const Json &data);
Json settings_to_json(const Settings &settings);
Settings load_settings(std::string &error);
void save_settings(const Settings &settings);
void set_autorun(bool enabled);
} // namespace beer
