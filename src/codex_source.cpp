#include "codex_source.hpp"
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <vector>

namespace beer {
namespace {
void release(HANDLE& h) { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); h = nullptr; }
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
std::wstring codex_path() {
    wchar_t value[32768]{};
    DWORD n = GetEnvironmentVariableW(L"CODEX_WIDGET_CODEX_PATH", value, 32768);
    if (n && n < 32768 && std::filesystem::is_regular_file(value)) return value;
    n = GetEnvironmentVariableW(L"APPDATA", value, 32768);
    if (n && n < 32768) {
        auto path = std::filesystem::path(value) / L"npm/node_modules/@openai/codex/node_modules/@openai/codex-win32-x64/vendor/x86_64-pc-windows-msvc/bin/codex.exe";
        if (std::filesystem::is_regular_file(path)) return path.wstring();
    }
    n = SearchPathW(nullptr, L"codex.exe", nullptr, 32768, value, nullptr);
    if (n && n < 32768) return value;
    throw std::runtime_error("Codex not found. Install Codex CLI or set CODEX_WIDGET_CODEX_PATH to codex.exe.");
}
CodexSource::CodexSource(const std::wstring& executable) {
    HANDLE child_in{}, child_out{}, child_err{};
    LPPROC_THREAD_ATTRIBUTE_LIST attributes{};
    PROCESS_INFORMATION pi{};
    try {
        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        check(CreatePipe(&child_in, &input_, &sa, 0), "stdin pipe failed");
        check(CreatePipe(&output_, &child_out, &sa, 0), "stdout pipe failed");
        check(CreatePipe(&error_, &child_err, &sa, 0), "stderr pipe failed");
        check(SetHandleInformation(input_, HANDLE_FLAG_INHERIT, 0), "pipe inheritance failed");
        check(SetHandleInformation(output_, HANDLE_FLAG_INHERIT, 0), "pipe inheritance failed");
        check(SetHandleInformation(error_, HANDLE_FLAG_INHERIT, 0), "pipe inheritance failed");
        SIZE_T size{};
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        attributes = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, size));
        check(attributes && InitializeProcThreadAttributeList(attributes, 1, 0, &size), "attribute allocation failed");
        HANDLE inherited[] = { child_in, child_out, child_err };
        check(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr), "handle whitelist failed");
        STARTUPINFOEXW si{}; si.StartupInfo.cb = sizeof(si);
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        si.StartupInfo.hStdInput = child_in; si.StartupInfo.hStdOutput = child_out; si.StartupInfo.hStdError = child_err;
        si.lpAttributeList = attributes;
        job_ = CreateJobObjectW(nullptr, nullptr);
        check(job_ != nullptr, "job creation failed");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        check(SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limit, sizeof(limit)), "job limit failed");
        std::wstring command = L"\"" + executable + L"\" app-server --stdio";
        check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi), "Could not start Codex");
        process_ = pi.hProcess;
        check(AssignProcessToJobObject(job_, process_), "Cannot supervise Codex process");
        check(ResumeThread(pi.hThread) != DWORD(-1), "Cannot resume Codex process");
        release(pi.hThread);
        DeleteProcThreadAttributeList(attributes); HeapFree(GetProcessHeap(), 0, attributes); attributes = nullptr;
        release(child_in); release(child_out); release(child_err);
        std::atomic_bool stop{false};
        request("initialize", {{"clientInfo", {{"name", "codex_beer_widget"}, {"title", "Codex Beer Widget"}, {"version", "0.1.0"}}}}, stop);
        send({{"method", "initialized"}});
    } catch (...) {
        if (pi.hThread) { if (process_) TerminateProcess(process_, 1); release(pi.hThread); }
        if (attributes) { DeleteProcThreadAttributeList(attributes); HeapFree(GetProcessHeap(), 0, attributes); }
        release(child_in); release(child_out); release(child_err); close(); throw;
    }
}
CodexSource::~CodexSource() { close(); }
void CodexSource::close() {
    release(input_);
    release(job_); // Terminates only this client's supervised process tree.
    if (process_) WaitForSingleObject(process_, 1000);
    release(process_); release(output_); release(error_);
}
void CodexSource::send(const Json& message) {
    auto line = message.dump() + '\n';
    DWORD written{};
    check(WriteFile(input_, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) && written == line.size(), "Codex input closed");
}
std::vector<Json> CodexSource::receive() {
    std::vector<Json> messages;
    for (HANDLE stream : {error_, output_}) {
        DWORD available{};
        check(PeekNamedPipe(stream, nullptr, 0, nullptr, &available, nullptr), "Codex disconnected");
        while (available) {
            char bytes[8192]; DWORD received{};
            check(ReadFile(stream, bytes, (std::min)(available, DWORD(sizeof(bytes))), &received, nullptr) && received, "Codex pipe closed");
            if (stream == output_) {
                buffered_.append(bytes, received);
                check(buffered_.size() <= 4 * 1024 * 1024, "Codex response exceeds size limit");
            }
            check(PeekNamedPipe(stream, nullptr, 0, nullptr, &available, nullptr), "Codex disconnected");
        }
    }
    size_t end;
    while ((end = buffered_.find('\n')) != std::string::npos) {
        auto message = Json::parse(buffered_.substr(0, end), nullptr, false);
        buffered_.erase(0, end + 1);
        check(!message.is_discarded(), "Invalid JSON from Codex");
        // This client never services auth-token, approval or model-tool requests.
        if (message.contains("method") && message.contains("id"))
            send({{"id", message["id"]}, {"error", {{"code", -32601}, {"message", "Method not supported by quota widget"}}}});
        else messages.push_back(std::move(message));
    }
    return messages;
}
Json CodexSource::request(const std::string& method, const Json& params, const std::atomic_bool& stop) {
    int id = ++next_id_;
    send({{"id", id}, {"method", method}, {"params", params}});
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
    while (!stop && std::chrono::steady_clock::now() < deadline) {
        for (const auto& m : receive()) {
            if (m.contains("id") && m["id"] == id) {
                if (m.contains("error")) throw std::runtime_error("Codex rejected request; check sign-in and connection");
                check(m.contains("result"), "Missing RPC result");
                return m["result"];
            }
            if (on_notification && m.contains("method")) on_notification(m);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    throw std::runtime_error(stop ? "Stopped" : "Codex request timed out");
}
void CodexSource::pump(const std::function<void(const Json&)>& notification) {
    for (const auto& m : receive()) if (m.contains("method")) notification(m);
}
}
