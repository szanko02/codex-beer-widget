#pragma once
#include "sync_snapshot.hpp"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace beer {
// Sender must use bounded I/O and return false for retryable delivery failures.
class SyncPublisher {
  public:
    using Sender = std::function<bool(const Json &)>;
    explicit SyncPublisher(Sender sender, std::chrono::milliseconds heartbeat = std::chrono::seconds(180),
                           std::chrono::milliseconds retry = std::chrono::seconds(5));
    ~SyncPublisher();
    void submit(const QuotaState &state);
    bool failed() const { return failed_; }

  private:
    Sender sender_;
    std::chrono::milliseconds heartbeat_, retry_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<QuotaState> latest_;
    bool dirty_ = false, stop_ = false;
    std::atomic_bool failed_{false};
    std::thread thread_;
    void run();
};
} // namespace beer
