#pragma once
#include "limit_model.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace beer {
constexpr UINT DataMessage = WM_APP + 2;
constexpr int poll_seconds(bool visible) {
    return visible ? 10 : 300;
}
inline std::chrono::steady_clock::time_point next_poll(std::chrono::steady_clock::time_point started,
                                                       std::chrono::steady_clock::time_point finished,
                                                       bool visible) {
    const auto interval = std::chrono::seconds(poll_seconds(visible));
    const auto scheduled = started + interval;
    return scheduled > finished ? scheduled : finished + interval;
}
inline int retry_seconds(int failures, bool visible) {
    return std::min(900, std::max(visible ? 5 : 300, 5 * (1 << std::clamp(failures - 1, 0, 8))));
}
class QuotaWorker {
  public:
    QuotaWorker(HWND window, bool demonstration, bool visible);
    ~QuotaWorker();
    void set_visible(bool visible);
    void refresh();
    std::optional<QuotaState> take();
    Json resources();

  private:
    HWND window_{};
    bool demo_{};
    std::atomic_bool stop_{false}, visible_{true}, refresh_{true};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<QuotaState> latest_;
    Json resources_;
    void publish(const QuotaState &state);
    void run();
};
} // namespace beer
