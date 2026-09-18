#include "sync_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace beer;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        const std::string fixture_path = SYNC_FIXTURES;
        std::ifstream input(std::filesystem::path(std::u8string(fixture_path.begin(), fixture_path.end())));
        const auto fixtures = Json::parse(input);
        for (const auto &fixture : fixtures) {
            QuotaState state;
            for (const auto &event : fixture.at("events")) {
                if (event.contains("response")) {
                    state.accept(event.at("response"), event.value("partial", false));
                    const auto observed = std::chrono::system_clock::time_point(
                        std::chrono::seconds(event.at("at").get<int64_t>()));
                    if (!event.value("partial", false))
                        state.updated = observed;
                    const auto changed = parse_groups(event.at("response"));
                    for (const auto &[id, group] : changed)
                        state.groups.at(id).updated = observed;
                } else {
                    state.fail(event.at("failure").get<std::string>());
                }
            }
            const auto actual = sync_snapshot(state, fixture.at("expected").at("revision").get<int64_t>());
            if (actual != fixture.at("expected"))
                throw std::runtime_error("Fixture mismatch: " + fixture.at("name").get<std::string>());
            const auto text = actual.dump();
            require(text.find("secret@example.com") == std::string::npos, "Private error leaked");
            require(text.find("private-token") == std::string::npos, "Raw account data leaked");
        }
        QuotaState state;
        state.accept({{"rateLimits", {{"primary", {{"usedPercent", 50}}}}}});
        auto &window = state.groups.begin()->second.windows.front();
        window.remaining = std::numeric_limits<double>::infinity();
        require(sync_snapshot(state, 1)["groups"]["legacy"]["windows"][0]["remaining"].is_null(),
                "Nonfinite percentage must be unknown");
        state.groups.begin()->second.windows.push_back(window);
        bool rejected = false;
        try {
            (void)sync_snapshot(state, 1);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected, "Duplicate slots must be rejected");
        for (const int64_t revision : {int64_t(0), SyncMaxInteger + 1}) {
            rejected = false;
            try {
                (void)sync_snapshot(QuotaState{}, revision);
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            require(rejected, "Invalid revision must be rejected");
        }
        std::cout << "Shared sync fixtures and privacy boundaries passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
