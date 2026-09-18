#include "sync_publisher.hpp"
#include <future>
#include <iostream>

using namespace beer;
using namespace std::chrono_literals;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
QuotaState sample(double used) {
    QuotaState state;
    state.accept({{"rateLimits", {{"limitId", "codex"}, {"primary", {{"usedPercent", used}}}}}});
    return state;
}
int main() {
    try {
        std::mutex mutex;
        std::condition_variable changed;
        std::vector<Json> delivered;
        std::promise<void> entered, release;
        auto unblocked = release.get_future().share();
        std::atomic_int count{0};
        {
            SyncPublisher publisher(
                [&](const Json &value) {
                    if (count.fetch_add(1) == 0) {
                        entered.set_value();
                        unblocked.wait();
                    }
                    {
                        std::lock_guard lock(mutex);
                        delivered.push_back(value);
                    }
                    changed.notify_one();
                    return true;
                },
                5s, 20ms);
            publisher.submit(sample(0));
            const bool started = entered.get_future().wait_for(2s) == std::future_status::ready;
            // Always unblock sender before any assertion can unwind publisher destruction.
            if (!started) {
                release.set_value();
                throw std::runtime_error("Sender failed to start");
            }
            for (int i = 1; i <= 100; ++i)
                publisher.submit(sample(i));
            release.set_value();
            std::unique_lock lock(mutex);
            require(changed.wait_for(lock, 2s, [&] { return delivered.size() >= 2; }),
                    "Latest state not sent");
            require(delivered.size() == 2, "Queue retained intermediate snapshots");
            require(delivered.back()["groups"]["codex"]["windows"][0]["remaining"] == 0,
                    "Wrong latest percentage");
            lock.unlock();
            auto same = sample(100);
            same.updated += 1s;
            same.groups.at("codex").updated += 1s;
            publisher.submit(same);
            lock.lock();
            require(!changed.wait_for(lock, 100ms, [&] { return delivered.size() > 2; }),
                    "Observation timestamp defeated deduplication");
            lock.unlock();
            same.fail("private failure");
            publisher.submit(same);
            lock.lock();
            require(changed.wait_for(lock, 2s, [&] { return delivered.size() == 3; }),
                    "Stale transition lost");
        }
        std::atomic_int attempts{0};
        std::promise<void> retried;
        {
            SyncPublisher publisher(
                [&](const Json &) {
                    if (++attempts == 1)
                        return false;
                    retried.set_value();
                    return true;
                },
                5s, 20ms);
            publisher.submit(sample(50));
            require(retried.get_future().wait_for(2s) == std::future_status::ready,
                    "Failed send not retried");
        }
        std::atomic_int heartbeats{0};
        std::promise<void> heartbeat;
        {
            SyncPublisher publisher(
                [&](const Json &) {
                    if (++heartbeats == 2)
                        heartbeat.set_value();
                    return true;
                },
                40ms, 20ms);
            publisher.submit(sample(50));
            require(heartbeat.get_future().wait_for(2s) == std::future_status::ready, "Heartbeat missing");
        }
        std::cout << "Latest-state queue, deduplication, stale, retry and heartbeat passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
