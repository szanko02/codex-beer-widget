#include "codex_source.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc, char **) {
    try {
        const auto start = std::chrono::steady_clock::now();
        beer::CodexSource source(beer::codex_path());
        std::atomic_bool stop{false};
        auto account = source.request("account/read", {{"refreshToken", false}}, stop);
        const bool signed_in = account.contains("account") && !account["account"].is_null();
        if (!signed_in)
            throw std::runtime_error("Sign into Codex CLI with the intended ChatGPT account first");
        auto limits = source.request("account/rateLimits/read", beer::Json::object(), stop);
        // Never print account identities, tokens or credit details.
        beer::Json result{
            {"authenticated", signed_in},
            {"serverPid", source.pid()},
            {"firstReadMs",
             std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()}};
        auto groups = limits.value("rateLimitsByLimitId", beer::Json{});
        if (!groups.is_object() || groups.empty())
            groups = {{"legacy", limits.value("rateLimits", beer::Json{})}};
        for (auto &[id, group] : groups.items()) {
            for (const auto *slot : {"primary", "secondary"})
                result["groups"][id][slot] = group.value(slot, beer::Json{});
        }
        result["serviceResources"] = source.resources();
        if (argc > 1) {
            auto before = source.resources();
            auto observed = std::chrono::steady_clock::now();
            int notices = 0;
            source.on_notification = [&](const beer::Json &m) {
                if (m.value("method", "") == "account/rateLimits/updated")
                    notices++;
            };
            for (int i = 0; i < 4; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(15));
                source.request("account/rateLimits/read", beer::Json::object(), stop);
            }
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - observed).count();
            auto after = source.resources();
            result["measurement"] = {
                {"before", before},
                {"after", after},
                {"elapsedSeconds", elapsed},
                {"notifications", notices},
                {"averageCpuOneCorePercent",
                 100 * (after["cpuSeconds"].get<double>() - before["cpuSeconds"].get<double>()) / elapsed}};
        }
        std::cout << result.dump(2) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
