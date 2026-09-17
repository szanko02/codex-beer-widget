#include "limit_model.hpp"
#include "settings.hpp"
#include "worker.hpp"
#include <iostream>
#include <stdexcept>
using namespace beer;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        require(parse_window({{"usedPercent", 25}}, "primary").remaining == 75, "remaining calculation");
        require(parse_window({{"usedPercent", -5}}, "primary").remaining == 100, "upper clamp");
        require(parse_window({{"usedPercent", 110}}, "primary").remaining == 0, "lower clamp");
        require(!parse_window({{"usedPercent", nullptr}}, "primary").remaining, "null is no data");
        require(!parse_window({{"usedPercent", "20"}}, "primary").remaining, "invalid type is no data");
        require(!parse_window({{"windowDurationMins", 0}}, "primary").minutes, "invalid duration");
        Json fixture = {{"rateLimitsByLimitId",
                         {{"codex",
                           {{"primary", {{"usedPercent", 12}, {"windowDurationMins", 1000}}},
                            {"secondary", {{"usedPercent", 25}, {"windowDurationMins", 15}}}}},
                          {"other", {{"primary", {{"usedPercent", 99}}}}}}},
                        {"rateLimits", {{"primary", {{"usedPercent", 77}}}}}};
        QuotaState s;
        s.accept(fixture);
        require(s.groups.size() == 2, "prefer grouped view");
        require(s.select_group("")->windows[0].slot == "secondary", "derive order from duration");
        require(s.select_group("other")->windows[0].remaining == 1, "do not combine groups");
        require(!s.select_group("removed"), "do not silently switch selected group");
        const auto timestamp = s.updated;
        s.fail("offline");
        require(s.stale && s.updated == timestamp && s.groups.size() == 2, "retain last known on error");
        s.accept({{"rateLimits", {{"limitId", "other"}, {"primary", {{"usedPercent", 20}}}}}}, true);
        require(s.groups.size() == 2 && s.stale && !s.groups.at("other").stale &&
                    s.groups.at("codex").stale && s.groups.at("codex").updated == timestamp,
                "partial notification must not refresh stale unrelated groups");
        auto old = parse_window({{"usedPercent", 60}, {"resetsAt", 100}}, "primary");
        auto fresh = parse_window({{"usedPercent", 0}, {"resetsAt", 200}}, "primary");
        require(confirmed_reset(old, fresh), "confirmed reset");
        fresh.resets_at = 100;
        require(!confirmed_reset(old, fresh), "correction is not confirmed reset");
        s.accept({{"rateLimits", nullptr}});
        require(s.groups.empty() && !s.stale, "valid no-data clears old quotas");
        auto legacy = parse_groups(
            {{"rateLimitsByLimitId", nullptr}, {"rateLimits", {{"primary", {{"usedPercent", 30}}}}}});
        require(legacy.at("legacy").windows[0].remaining == 70, "legacy fallback");
        LevelTransition level;
        level.set(50, .8, true);
        require(level.value(std::chrono::steady_clock::now()) == 50 && !level.active(),
                "initial value without fake reset");
        level.set(20, .8);
        require(level.active() &&
                    level.value(std::chrono::steady_clock::now() + std::chrono::seconds(2)) == 20,
                "transition reaches target");
        Settings settings;
        require(decorative_animation(settings.theme, 2, 70), "mug decorations animate");
        settings.theme.ring = true;
        require(!decorative_animation(settings.theme, 2, 70), "ring has no moving decorations");
        settings.theme.ring = false;
        require(!decorative_animation(settings.theme, 0, 70) && !decorative_animation(settings.theme, 2, 0) &&
                    !decorative_animation(settings.theme, 2, -1),
                "economy, empty and missing values stay static");
        settings.height = 400;
        settings.x = -1800;
        settings.hotkey = VK_F8;
        settings.monitor_offset_x = 40;
        settings.monitor_offset_y = 70;
        settings.click_through = true;
        settings.theme.liquid_color = 0x123456;
        auto restored = settings_from_json(settings_to_json(settings));
        require(restored.height == 400 && restored.x == -1800 && restored.hotkey == VK_F8 &&
                    restored.click_through && restored.theme.liquid_color == 0x123456 &&
                    restored.monitor_offset_x == 40 && restored.monitor_offset_y == 70,
                "settings roundtrip");
        restored = settings_from_json({{"height", 9999}, {"theme", {{"glassAlpha", -8}, {"bubbles", 200}}}});
        require(restored.height == 400 && restored.theme.glass_alpha == 0 && restored.theme.bubbles == 24,
                "settings range validation");
        restored = settings_from_json(
            {{"theme",
              {{"resources", {{"geometry", "ring"}}},
               {"fillArea", {{"left", 999}, {"right", -3}, {"top", 999}, {"bottom", -1}}}}}});
        require(restored.theme.ring && restored.theme.fill_right > restored.theme.fill_left &&
                    restored.theme.fill_bottom > restored.theme.fill_top,
                "theme geometry validation");
        require(retry_seconds(1, true) == 5 && retry_seconds(2, true) == 10 &&
                    retry_seconds(9, true) == 900 && retry_seconds(1, false) == 300,
                "bounded retry schedule");
        const auto start = std::chrono::steady_clock::time_point{};
        for (int seconds : refresh_choices) {
            auto choice = settings_from_json({{"refreshSeconds", seconds}});
            require(settings_from_json(settings_to_json(choice)).refresh_seconds == seconds,
                    "interval roundtrip");
            require(next_poll(start, start + std::chrono::milliseconds(200), true, seconds) ==
                        start + std::chrono::seconds(seconds),
                    "selected polling interval");
        }
        require(settings_from_json({{"refreshSeconds", 7}}).refresh_seconds == 10,
                "invalid interval fallback");
        HoverIntent hover;
        require(hover.motion(0, 0, start), "arm first hover");
        require(!hover.ready(start + std::chrono::milliseconds(799)), "no immediate tooltip");
        require(!hover.motion(2, 1, start + std::chrono::milliseconds(400)), "ignore small pointer jitter");
        require(hover.ready(start + std::chrono::milliseconds(800)), "show after dwell");
        require(!hover.ready(start + std::chrono::seconds(2)), "do not repeatedly activate tooltip");
        require(hover.motion(10, 0, start + std::chrono::seconds(2)) && !hover.visible,
                "movement hides and rearms");
        hover.leave();
        require(!hover.ready(start + std::chrono::seconds(5)), "leave cancels pending tooltip");
        require(next_poll(start, start + std::chrono::seconds(2), true) == start + std::chrono::seconds(10),
                "normal response latency does not extend ten-second cadence");
        require(next_poll(start, start + std::chrono::seconds(25), true) == start + std::chrono::seconds(35),
                "slow responses must not cause catch-up request bursts");
        require(next_poll(start, start + std::chrono::seconds(2), false) == start + std::chrono::seconds(300),
                "hidden widget retains economical polling");
        std::cout << "Quota parsing, states, groups, resets and transition tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
