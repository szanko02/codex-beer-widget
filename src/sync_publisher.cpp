#include "sync_publisher.hpp"
#include <random>

namespace beer {
SyncPublisher::SyncPublisher(Sender sender, std::chrono::milliseconds heartbeat,
                             std::chrono::milliseconds retry)
    : sender_(std::move(sender)), heartbeat_(heartbeat), retry_(retry) {
    if (!sender_ || heartbeat_.count() <= 0 || retry_.count() <= 0)
        throw std::invalid_argument("Invalid sync publisher configuration");
    thread_ = std::thread([this] { run(); });
}
SyncPublisher::~SyncPublisher() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    thread_.join();
}
void SyncPublisher::submit(const QuotaState &state) {
    {
        std::lock_guard lock(mutex_);
        latest_ = state;
        dirty_ = true;
    }
    wake_.notify_one();
}
void SyncPublisher::run() {
    using Clock = std::chrono::steady_clock;
    auto next = Clock::time_point::max();
    Json sent_content;
    bool retrying = false;
    unsigned failures = 0;
    std::minstd_rand random(std::random_device{}());
    std::unique_lock lock(mutex_);
    while (!stop_) {
        wake_.wait_until(lock, next, [&] { return stop_ || (dirty_ && !retrying); });
        if (stop_)
            break;
        if (!latest_)
            continue;
        const auto state = *latest_;
        dirty_ = false;
        lock.unlock();
        try {
            auto snapshot = sync_snapshot(state, 1); // Transport assigns its durable revision.
            auto content = snapshot;
            content.erase("sourceUpdatedAt");
            for (auto &[id, group] : content["groups"].items())
                group.erase("sourceUpdatedAt");
            if (retrying || content != sent_content || Clock::now() >= next) {
                if (!sender_(snapshot))
                    throw std::runtime_error("Sync delivery failed");
                sent_content = std::move(content);
                failed_ = false;
                retrying = false;
                failures = 0;
                next = Clock::now() + heartbeat_;
            }
        } catch (...) {
            failed_ = true;
            retrying = true;
            failures = std::min(6u, failures + 1);
            const auto delay = std::min(std::chrono::milliseconds(300000), retry_ * (1 << (failures - 1)));
            next = Clock::now() + delay + std::chrono::milliseconds(random() % (delay.count() / 4 + 1));
        }
        lock.lock();
    }
}
} // namespace beer
