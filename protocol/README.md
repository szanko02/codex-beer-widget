# Sync Protocol v1

`quota-state-v1.json` defines a complete normalized snapshot, never a raw Codex response.
Unknown fields and versions are rejected. Maximum UTF-8 payload: 64 KiB. Transport is HTTPS/WSS;
device identity and credentials belong in the authenticated transport, not snapshot fields.

## State semantics

- Remaining percentage is `clamp(100 - usedPercent, 0, 100)`. Null means unknown, never zero.
- Unix timestamps use seconds UTC; null means never observed. Durations come from the source.
- An empty `groups` object is a successful no-data result and replaces cached quotas.
- Transport failures preserve values and source timestamps, marking them stale.
- Partial Codex notifications are merged on Windows before sending a complete snapshot.
  Each group retains its own timestamp and stale flag; a fresh group cannot refresh another group.
- Window identity is `(group ID, slot)`, not array position or duration. Do not combine groups.
- A confirmed reset requires the same identity, increased remaining percentage, and increased
  non-null `resetsAt`. A clock tick or percentage correction alone is not a reset.

## Delivery and ordering

One Windows publisher owns each paired device. Persist a strictly increasing revision before sending;
retries reuse identical revision and payload. Restart must not reset the counter. If durable identity
or revision is lost, create a new device pairing. Integers fit the JSON safe integer range.
The relay rejects older revisions; equal revisions are idempotent only for identical snapshots.
Receivers discard older revisions, including delayed FCM messages. Newer snapshots replace state atomically.

Use a single pending latest-state slot. Deduplicate quota content and stale flags, ignoring observation
timestamps; send a heartbeat with current timestamps at most every 180 seconds when unchanged.
Failed sends retry with capped exponential backoff and jitter, without blocking UI or polling.
Heartbeats never manufacture a successful source timestamp.

Relay receipt time, phone receipt time, and source observation time are distinct. Clients independently
mark the connection stale after 600 seconds without a newer snapshot; no missing update implies 0%.
Use elapsed time for live deadlines; handle process restarts and clock changes conservatively.
An unchanged replay must not extend freshness. FCM is a refresh hint, not guaranteed realtime delivery.

## Privacy and pairing

Only quota group labels, normalized windows, timestamps, stale flags, and revision cross this boundary.
Never serialize account details, emails, tokens, paths, raw responses, or error/diagnostic strings.
Names describe quota groups only; they must never be populated from account identity.
Store only the latest snapshot at the relay. Do not log payloads or authorization headers.

Generate secrets with a cryptographic RNG (256 bits). Use separate publisher and read-only subscriber
credentials so a paired phone cannot overwrite desktop quotas. QR contains relay origin, device ID,
and a short-lived one-use pairing secret, never publisher credentials. Require authenticated pairing
creation, expiry, rate limits, and revocation. Persist desktop secrets with DPAPI; Android secrets with
Keystore-backed encryption. Unpair revokes credentials and removes stored snapshot and push registration.

Fixtures under `fixtures/` are shared normalized cases for C++, Kotlin, and relay validation.
