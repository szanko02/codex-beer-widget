# Источник → состояние → анимация → окно

`QuotaWorker` owns its Codex process and performs all blocking account and quota requests on a background thread.
It publishes a bounded latest-state slot and posts a window message; no UI callback waits for a quota response.
Showing the window and constructing Direct2D do not depend on the server. Shutdown cancels a pending handshake/read.

Visible polling: every 10 seconds, measured from the start of a successful read. Hidden/locked/display-off: 300 seconds.
If a request outlasts the interval, the next read is delayed by a full interval to avoid catch-up bursts.
Requests run sequentially on the existing server, without creating a new process for each poll.
Notifications are drained at most one second after arrival while idle, without initiating remote work;
polling remains the reliable fallback because Desktop-originated quota notifications have not been observed locally.
This is not a guarantee of real-time backend data: network latency, server updates and errors can delay values.
Manual refresh and resume trigger an immediate read. Errors retain the last state and use bounded retry delays
(5, 10, 20… up to 900 seconds visible; at least 300 seconds hidden). Account changes clear the previous account's values.

`--demo` replaces Codex with a sequence of full, half, low, empty, offline, restored and unknown states.
It starts no Codex process and never consumes model usage. The caption always labels this mode as demonstration.

Animation timers run only while a level transition or enabled visible decoration requires frames.
Session lock, suspend and console display power notifications pause rendering. Actual sleep and monitor tests remain pending.
The source interface, quota model and renderer are independent of settings and theme JSON.

Local verification: Release build and CTest passed. The live widget displayed a 65% remainder for the
source-provided five-hour window on 2026-09-09; the renderer no longer uses fixed demonstration values in normal mode.
