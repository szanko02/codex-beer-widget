#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace beer {
inline constexpr std::array<int, 5> refresh_choices{2, 5, 10, 30, 60};
inline int valid_refresh(int seconds) {
    return std::find(refresh_choices.begin(), refresh_choices.end(), seconds) != refresh_choices.end()
               ? seconds
               : 10;
}
class HoverIntent {
  public:
    using Clock = std::chrono::steady_clock;
    bool entered = false, visible = false;
    int delay_ms = 800;
    bool motion(double x, double y, Clock::time_point now) {
        if (!entered || std::hypot(x - x_, y - y_) > 4.0) {
            entered = true;
            visible = false;
            x_ = x;
            y_ = y;
            due_ = now + std::chrono::milliseconds(delay_ms);
            return true;
        }
        return false;
    }
    bool ready(Clock::time_point now) {
        if (entered && !visible && now >= due_) {
            visible = true;
            return true;
        }
        return false;
    }
    void leave() { entered = visible = false; }

  private:
    double x_ = 0, y_ = 0;
    Clock::time_point due_{};
};
} // namespace beer
