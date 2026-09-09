#include "worker.hpp"
#include <memory>
namespace beer {
QuotaWorker::QuotaWorker(HWND window, bool demo, bool visible)
    : window_(window), demo_(demo), visible_(visible) {
    thread_ = std::thread([this] { run(); });
}
QuotaWorker::~QuotaWorker() {
    stop_ = true;
    wake_.notify_all();
    if (thread_.joinable())
        thread_.join();
}
void QuotaWorker::set_visible(bool visible) {
    const bool previous = visible_.exchange(visible);
    if (previous != visible) {
        if (visible)
            refresh_ = true;
        wake_.notify_all();
    }
}
void QuotaWorker::refresh() {
    refresh_ = true;
    wake_.notify_all();
}
std::optional<QuotaState> QuotaWorker::take() {
    std::lock_guard lock(mutex_);
    auto value = std::move(latest_);
    latest_.reset();
    return value;
}
Json QuotaWorker::resources() {
    std::lock_guard lock(mutex_);
    return resources_;
}
void QuotaWorker::publish(const QuotaState &state) {
    {
        std::lock_guard lock(mutex_);
        latest_ = state;
    }
    PostMessageW(window_, DataMessage, 0, 0);
}
void QuotaWorker::run() {
    using Clock = std::chrono::steady_clock;
    auto due = Clock::now();
    bool last_visible = visible_;
    int failures = 0, demo_step = 0;
    QuotaState state;
    std::unique_ptr<CodexSource> source;
    std::string account_identity;
    while (!stop_) {
        const bool visible = visible_;
        if (visible != last_visible) {
            due = visible ? Clock::now() : Clock::now() + std::chrono::seconds(300);
            last_visible = visible;
        }
        const bool requested = refresh_.exchange(false);
        if (requested || Clock::now() >= due) {
            try {
                if (demo_) {
                    const int step = demo_step++ % 7;
                    if (step == 4)
                        state.fail("Демонстрация: соединение потеряно");
                    else if (step == 6)
                        state.accept({{"rateLimits", nullptr}});
                    else {
                        const double values[] = {100, 50, 10, 0, 0, 100, 0};
                        const auto now =
                            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                        state.accept({{"rateLimits",
                                       {{"limitId", "codex"},
                                        {"primary",
                                         {{"usedPercent", 100 - values[step]},
                                          {"windowDurationMins", 300},
                                          {"resetsAt", now + 3600}}},
                                        {"secondary",
                                         {{"usedPercent", 33},
                                          {"windowDurationMins", 10080},
                                          {"resetsAt", now + 604800}}}}}});
                    }
                    due = Clock::now() + std::chrono::seconds(visible ? 4 : 300);
                } else {
                    if (!source) {
                        source = std::make_unique<CodexSource>(codex_path(), &stop_);
                        source->on_notification = [&](const Json &message) {
                            if (message.value("method", "") == "account/rateLimits/updated") {
                                state.accept(message.value("params", Json::object()), true);
                                publish(state);
                            }
                            if (message.value("method", "") == "account/updated") {
                                state = QuotaState{};
                                state.fail("Аккаунт изменился; обновляем данные");
                                publish(state);
                                refresh_ = true;
                            }
                        };
                    }
                    auto account = source->request("account/read", {{"refreshToken", false}}, stop_);
                    if (!account.contains("account") || account["account"].is_null())
                        throw std::runtime_error("Войдите в Codex CLI через ChatGPT");
                    const auto identity = account["account"].dump();
                    if (!account_identity.empty() && identity != account_identity)
                        state = QuotaState{};
                    account_identity = identity;
                    state.accept(source->request("account/rateLimits/read", Json::object(), stop_));
                    {
                        std::lock_guard lock(mutex_);
                        resources_ = source->resources();
                    }
                    due = Clock::now() + std::chrono::seconds(visible ? 60 : 300);
                }
                failures = 0;
                publish(state);
            } catch (const std::exception &error) {
                if (stop_)
                    break;
                state.fail(error.what());
                source.reset();
                publish(state);
                failures = std::min(9, failures + 1);
                due = Clock::now() + std::chrono::seconds(retry_seconds(failures, visible));
            }
        }
        if (source) {
            try {
                source->pump(source->on_notification);
            } catch (const std::exception &e) {
                state.fail(e.what());
                source.reset();
                publish(state);
                failures = std::min(9, failures + 1);
                due = Clock::now() + std::chrono::seconds(retry_seconds(failures, visible));
            }
        }
        std::unique_lock lock(mutex_);
        wake_.wait_until(lock, std::min(due, Clock::now() + std::chrono::seconds(1)),
                         [&] { return stop_ || refresh_ || visible_ != last_visible; });
    }
}
} // namespace beer
