# Development workflow

- `main`: verified release baseline; `develop`: integrated development.
- Each stage starts in `codex/<stage>` from `develop`.
- Commit coherent, tested changes using Conventional Commits; push during development.
- Integrate stage branches through pull requests with checks and a merge commit, preserving history.
- Never reconstruct or force-push published history to simulate progress.
- Keep credentials, account identities, raw account responses and machine-specific logs outside Git.
- Document actual verification separately from planned acceptance checks. Unperformed checks stay pending.
- Build Windows x64 Release and run CTest before integrating native changes.

No model requests, quota resets, account logout or login automation are part of widget operation.
