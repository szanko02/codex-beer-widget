#include "codex_source.hpp"
#include <iostream>
#include <stdexcept>
using beer::Json;
int main(int argc, char**) {
    if (argc > 1) {
        bool ready = false;
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto m = Json::parse(line);
            const auto method = m.value("method", "");
            if (method == "initialized") { ready = true; continue; }
            if (!m.contains("id")) continue;
            Json result;
            if (method == "initialize") result = {{"userAgent", "fixture"}};
            else if (method == "account/read" && ready) result = {{"account", {{"type", "chatgpt"}}}};
            else if (method == "account/rateLimits/read" && ready) {
                std::cout << Json{{"method", "account/rateLimits/updated"}, {"params", {{"test", true}}}}.dump() << std::endl;
                result = {{"rateLimits", {{"primary", {{"usedPercent", 25}}}}}};
            } else {
                std::cout << Json{{"id", m["id"]}, {"error", {{"code", -32601}, {"message", "unknown"}}}}.dump() << std::endl;
                continue;
            }
            std::cout << Json{{"id", m["id"]}, {"result", result}}.dump() << std::endl;
        }
        return 0;
    }
    try {
        wchar_t path[32768]{}; GetModuleFileNameW(nullptr, path, 32768);
        beer::CodexSource source(path);
        std::atomic_bool stop{false};
        auto account = source.request("account/read", Json::object(), stop);
        if (account["account"]["type"] != "chatgpt") throw std::runtime_error("handshake failed");
        int notices = 0;
        source.on_notification = [&](const Json&) { notices++; };
        auto limits = source.request("account/rateLimits/read", Json::object(), stop);
        if (limits["rateLimits"]["primary"]["usedPercent"] != 25 || notices != 1) throw std::runtime_error("message routing failed");
        bool rejected = false;
        try { source.request("unknown", Json::object(), stop); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) throw std::runtime_error("RPC error ignored");
        std::cout << "Handshake, request routing, interleaved notifications and RPC errors passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
