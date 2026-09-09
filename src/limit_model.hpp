#pragma once
#include "codex_source.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <optional>

namespace beer {
struct QuotaWindow {
    std::string slot;
    std::optional<double> remaining;
    std::optional<int64_t> minutes, resets_at;
};
struct QuotaGroup {
    std::string id, name;
    std::vector<QuotaWindow> windows;
    std::chrono::system_clock::time_point updated{};
    bool stale = false;
    std::string error;
};
inline std::optional<int64_t> integer_field(const Json &j, const char *key) {
    if (!j.contains(key) || !j[key].is_number_integer())
        return {};
    try {
        return j[key].get<int64_t>();
    } catch (...) {
        return {};
    }
}
inline QuotaWindow parse_window(const Json &j, const std::string &slot) {
    QuotaWindow w;
    w.slot = slot;
    if (!j.is_object())
        return w;
    if (j.contains("usedPercent") && j["usedPercent"].is_number()) {
        double used = j["usedPercent"].get<double>();
        if (std::isfinite(used))
            w.remaining = std::clamp(100.0 - used, 0.0, 100.0);
    }
    w.minutes = integer_field(j, "windowDurationMins");
    w.resets_at = integer_field(j, "resetsAt");
    if (w.minutes && *w.minutes <= 0)
        w.minutes.reset();
    if (w.resets_at && *w.resets_at <= 0)
        w.resets_at.reset();
    return w;
}
inline std::map<std::string, QuotaGroup> parse_groups(const Json &response) {
    std::map<std::string, QuotaGroup> result;
    auto groups = response.value("rateLimitsByLimitId", Json{});
    if (!groups.is_object() || groups.empty()) {
        const auto legacy = response.value("rateLimits", Json{});
        if (!legacy.is_object())
            return result;
        const auto id = legacy.contains("limitId") && legacy["limitId"].is_string()
                            ? legacy["limitId"].get<std::string>()
                            : "legacy";
        groups = {{id, legacy}};
    }
    for (const auto &[id, j] : groups.items()) {
        if (!j.is_object())
            continue;
        QuotaGroup group;
        group.id = id;
        group.name = id;
        if (j.contains("limitName") && j["limitName"].is_string())
            group.name = j["limitName"].get<std::string>();
        for (const auto *slot : {"primary", "secondary"}) {
            if (j.contains(slot) && j[slot].is_object())
                group.windows.push_back(parse_window(j[slot], slot));
        }
        std::stable_sort(group.windows.begin(), group.windows.end(), [](const auto &a, const auto &b) {
            if (a.minutes && b.minutes)
                return *a.minutes < *b.minutes;
            return a.minutes.has_value() && !b.minutes.has_value();
        });
        result.emplace(id, std::move(group));
    }
    return result;
}
struct QuotaState {
    std::map<std::string, QuotaGroup> groups;
    std::chrono::system_clock::time_point updated{};
    bool stale = true;
    std::string error;
    void accept(const Json &response, bool partial = false) {
        auto fresh = parse_groups(response);
        const auto now = std::chrono::system_clock::now();
        for (auto &[id, group] : fresh)
            group.updated = now;
        if (partial)
            for (auto &[id, group] : fresh)
                groups[id] = std::move(group);
        else
            groups = std::move(fresh);
        if (!partial)
            updated = now;
        stale = std::any_of(groups.begin(), groups.end(), [](const auto &item) { return item.second.stale; });
        if (!stale)
            error.clear();
    }
    void fail(std::string message) {
        stale = true;
        error = std::move(message);
        for (auto &[id, group] : groups) {
            group.stale = true;
            group.error = error;
        }
    }
    const QuotaGroup *select_group(const std::string &preferred) const {
        if (auto it = groups.find(preferred); it != groups.end())
            return &it->second;
        if (!preferred.empty())
            return nullptr;
        if (auto it = groups.find("codex"); it != groups.end())
            return &it->second;
        return groups.empty() ? nullptr : &groups.begin()->second;
    }
};
inline bool confirmed_reset(const QuotaWindow &previous, const QuotaWindow &current) {
    return previous.slot == current.slot && previous.remaining && current.remaining &&
           *current.remaining > *previous.remaining && previous.resets_at && current.resets_at &&
           *current.resets_at > *previous.resets_at;
}
class LevelTransition {
    double from_ = 0, target_ = 0;
    std::chrono::steady_clock::time_point start_{};
    double seconds_ = .8;

  public:
    double value(std::chrono::steady_clock::time_point now) const {
        const double t = std::clamp(std::chrono::duration<double>(now - start_).count() / seconds_, 0.0, 1.0);
        const double eased = t * t * (3 - 2 * t);
        return from_ + (target_ - from_) * eased;
    }
    void set(double target, double seconds, bool immediate = false) {
        const auto now = std::chrono::steady_clock::now();
        from_ = immediate ? target : value(now);
        target_ = std::clamp(target, 0.0, 100.0);
        start_ = now;
        seconds_ = std::clamp(seconds, .1, 5.0);
    }
    bool active() const {
        return from_ != target_ &&
               std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count() < seconds_;
    }
};
} // namespace beer
