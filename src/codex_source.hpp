#pragma once
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <windows.h>

namespace beer {
using Json = nlohmann::json;
std::wstring codex_path();
class CodexSource {
  public:
    explicit CodexSource(const std::wstring &executable, const std::atomic_bool *stop = nullptr);
    ~CodexSource();
    CodexSource(const CodexSource &) = delete;
    CodexSource &operator=(const CodexSource &) = delete;
    Json request(const std::string &method, const Json &params, const std::atomic_bool &stop);
    void pump(const std::function<void(const Json &)> &notification);
    DWORD pid() const { return process_ ? GetProcessId(process_) : 0; }
    Json resources() const;

  private:
    HANDLE process_{}, job_{}, input_{}, output_{}, error_{};
    std::string buffered_;
    int next_id_{};
    void close();
    void send(const Json &message);
    std::vector<Json> receive();

  public:
    std::function<void(const Json &)> on_notification;
};
} // namespace beer
