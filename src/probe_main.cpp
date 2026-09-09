#include "codex_source.hpp"
#include <iostream>
#include <chrono>
int main() {
    try {
        const auto start = std::chrono::steady_clock::now();
        beer::CodexSource source(beer::codex_path());
        std::atomic_bool stop{false};
        auto account = source.request("account/read", {{"refreshToken", false}}, stop);
        const bool signed_in = account.contains("account") && !account["account"].is_null();
        if (!signed_in) throw std::runtime_error("Sign into Codex CLI with the intended ChatGPT account first");
        auto limits = source.request("account/rateLimits/read", beer::Json::object(), stop);
        // Never print account identities, tokens or credit details.
        beer::Json result{{"authenticated", signed_in}, {"serverPid", source.pid()},
            {"firstReadMs", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()}};
        auto groups = limits.value("rateLimitsByLimitId", beer::Json{});
        if (!groups.is_object() || groups.empty()) groups = {{"legacy", limits.value("rateLimits", beer::Json{})}};
        for (auto& [id, group] : groups.items()) {
            for (const auto* slot : {"primary", "secondary"})
                result["groups"][id][slot] = group.value(slot, beer::Json{});
        }
        std::cout << result.dump(2) << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
