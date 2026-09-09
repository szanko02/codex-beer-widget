# Stage 1: independent limit source

## Observed on 2026-09-09

Windows 11 Pro 10.0.26200, Codex CLI 0.153.4, existing ChatGPT-managed sign-in.
`node tools/probe-source.mjs` launched the native CLI executable directly with redirected stdio.
It did not read credential files or use a Codex Desktop tool. Node is development tooling only.

- initialize → initialized → account/read → account/rateLimits/read succeeded.
- Responses described two windows by duration: 300 and 10080 minutes in group `codex`.
- Used percentages changed during Desktop work in the same session. Polling sees Desktop consumption.
- Stopping the server and starting a new process retained the same account and working quota reads.
- No `account/rateLimits/updated` notifications were observed. Cross-process push delivery is **not** established;
  use scheduled reads for correctness and notifications only as an additional signal.
- First read including process startup and account check: 4.45 seconds. Show the widget before this work completes.

| Service measurement | Initial | Final |
|---|---:|---:|
| Working set, MiB | 42.97 | 44.50 |
| Private committed memory, MiB | 18.11 | 19.27 |
| Cumulative CPU, seconds | 0.28125 | 0.37500 |
| Threads | 40 | 34 |

The measured interval was 89.73 seconds, including four remote reads and their latency.
Mean CPU: 0.1045% of one logical core (not divided by the computer's logical processor count).
This is a short observation, not a performance guarantee. Installation activity was concurrent.
The probe launcher and PowerShell measurement processes are development-only and are excluded above.
Raw account-specific results are ignored under `.local/` and must not be uploaded.

## Native client gate

Native `CodexQuotaProbe.exe` validation, process-tree accounting and fake-server CTest results will be recorded here.
Clean-machine sign-in, expired authorization and a Windows reboot remain separate acceptance tests.
No inference is made that restarting App Server is equivalent to rebooting Windows.

Protocol reference: [Codex App Server](https://learn.chatgpt.com/docs/app-server).
