#pragma once
#include <cstdint>
namespace beer {
struct Theme {
    uint32_t liquid_color = 0xeda429, text_color = 0xffffff;
    float glass_alpha = .55f, liquid_alpha = .92f, foam = .65f, waves = .35f;
    int bubbles = 12;
    float transition_seconds = .8f, text_size = 18;
    bool show_percent = true, decoration = true, ring = false;
    float fill_left = 47, fill_top = 46, fill_right = 167, fill_bottom = 220;
};
inline bool decorative_animation(const Theme &theme, int performance, double remaining) {
    return !theme.ring && theme.decoration && performance > 0 && remaining > 0 &&
           (theme.bubbles > 0 || theme.waves > 0);
}
} // namespace beer
