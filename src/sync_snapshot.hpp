#pragma once
#include "limit_model.hpp"
#include <stdexcept>

namespace beer {
inline constexpr int64_t SyncMaxInteger = 9007199254740991;
inline Json sync_integer(std::optional<int64_t> value) {
    return value && *value > 0 && *value <= SyncMaxInteger ? Json(*value) : Json(nullptr);
}
inline Json sync_timestamp(std::chrono::system_clock::time_point value) {
    return sync_integer(std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch()).count());
}
inline Json sync_snapshot(const QuotaState &state, int64_t revision) {
    if (revision < 1 || revision > SyncMaxInteger)
        throw std::invalid_argument("Invalid sync revision");
    Json groups = Json::object();
    if (state.groups.size() > 64)
        throw std::invalid_argument("Too many quota groups");
    for (const auto &[id, group] : state.groups) {
        if (id.empty() || id.size() > 128 || group.name.size() > 128)
            throw std::invalid_argument("Invalid quota group label");
        Json windows = Json::array();
        bool primary = false, secondary = false;
        for (const auto &window : group.windows) {
            if (window.slot != "primary" && window.slot != "secondary")
                throw std::invalid_argument("Invalid quota window slot");
            auto &seen = window.slot == "primary" ? primary : secondary;
            if (seen)
                throw std::invalid_argument("Duplicate quota window slot");
            seen = true;
            const Json remaining = window.remaining && std::isfinite(*window.remaining)
                                       ? Json(std::clamp(*window.remaining, 0.0, 100.0))
                                       : Json(nullptr);
            windows.push_back({{"slot", window.slot},
                               {"remaining", remaining},
                               {"windowMinutes", sync_integer(window.minutes)},
                               {"resetsAt", sync_integer(window.resets_at)}});
        }
        groups[id] = {{"name", group.name},
                      {"sourceUpdatedAt", sync_timestamp(group.updated)},
                      {"stale", group.stale},
                      {"windows", std::move(windows)}};
    }
    Json result = {{"version", 1},
                   {"revision", revision},
                   {"sourceUpdatedAt", sync_timestamp(state.updated)},
                   {"stale", state.stale},
                   {"groups", std::move(groups)}};
    if (result.dump().size() > 65536)
        throw std::invalid_argument("Sync snapshot too large");
    return result;
}
} // namespace beer
