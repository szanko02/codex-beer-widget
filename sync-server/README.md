# Local quota relay

Requires Node.js 24 and OpenSSL. Stores one snapshot per paired device in SQLite; no usage-history API.
Only hashed credentials are stored. One phone per device in v1; redeeming a new invitation revokes the old phone.
HTTPS is mandatory in the executable entry point. Unencrypted HTTP is used only by in-process tests on loopback.

From `sync-server/`:

```powershell
npm ci
npm test
npm run init-local -- 192.168.1.10 # Replace with this PC's LAN address or DNS name
$env:SYNC_ADMIN_SECRET_FILE = '../.local/relay/admin-secret.txt'
$env:SYNC_TLS_KEY = '../.local/relay/tls-key.pem'
$env:SYNC_TLS_CERT = '../.local/relay/tls-cert.pem'
$env:SYNC_HOST = '0.0.0.0'        # Omit for loopback-only operation
npm start
```

In another terminal, run `npm run pair -- --register` once to register the generated desktop credentials
and produce `.local/relay/pairing.png`. Subsequent `npm run pair` replaces the five-minute invitation.
Use `../tools/configure-sync.ps1 -ConfigurationFile ../.local/relay/publisher.json` with the widget closed.
The publisher keeps its durable revision in DPAPI; never overwrite an existing pairing with revision zero.

The generated certificate is for local development and expires after 30 days. Windows and Android must
explicitly trust it in the relevant client trust store; this tool does not change system trust or firewall rules.
Check its fingerprint before trusting it. Never disable hostname or certificate verification. Keep private
keys, QR, publisher credentials, and database outside version control. Restrict the directory to its owner
on Windows; POSIX mode flags alone do not establish Windows ACLs.

## API

- `POST /v1/devices`: admin bearer; register `deviceId` and `publisherSecret` (32 random bytes, base64url).
- `POST /v1/pair`: publisher bearer, `deviceId`; return one-use invitation.
- `POST /v1/pair/redeem`: `deviceId`, `pairingSecret`; return read-only `readerSecret`.
- `POST /v1/devices/{id}/state`: publisher bearer, protocol v1 snapshot; reject old/conflicting revisions.
- `GET /v1/devices/{id}/state`: reader bearer; 404 until first snapshot.
- `WSS /v1/devices/{id}/stream`: reader bearer in handshake header; initial/current snapshots, server ping.
- `POST /v1/unpair`: publisher bearer and `deviceId` deletes device; reader bearer plus
  `scope: "subscriber"` revokes only phone access.

No tokens in URLs, CORS, cookies, request-body logs, or redirects. Limits: 64 KiB snapshots, 1,000 devices,
128 streams, 120 requests/minute/IP. For public deployment use a maintained TLS endpoint, persistent restricted
storage, and infrastructure-level connection limits. SQLite/WAL may retain physical pages until maintenance;
the application retains no logical history.

Optional FCM: set `GOOGLE_APPLICATION_CREDENTIALS` to a private service-account file.
`POST /v1/devices/{id}/push` accepts a reader-authenticated `{ "token": "..." }` registration.
Only one phone token is retained; pairing replacement and unpair revoke it. Changes produce
normal-priority hints coalesced for 30 seconds. Slow delivery keeps only the latest pending hint.
Without Firebase configuration, HTTPS/WSS and Android's periodic fallback remain available.
Never place service-account files in this repository; use a private external directory or `.local/`.

Without a public/VPN route, the phone must reach this PC on the same network. PC shutdown stops syncing;
clients retain old percentages and show stale state. A local relay alone cannot deliver updates over the Internet.
