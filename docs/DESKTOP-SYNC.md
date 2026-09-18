# Desktop sync (development)

Sync is disabled until explicitly provisioned. Real quota state enters a latest-only slot; a separate
thread serializes and sends HTTPS requests. UI delivery does not wait for relay requests. Demo mode
never publishes. Unchanged observations are deduplicated; heartbeat is 180 seconds. Failed delivery
backs off from 5 seconds to 300 seconds with jitter. Hidden desktop polling remains 300 seconds.

The relay provisioning flow supplies a private JSON file with `origin`, `deviceId`, and `publisherSecret`.
Do not put it in Git. Close the widget, then run:

```powershell
./tools/configure-sync.ps1 -ConfigurationFile .local/publisher.json
```

This encrypts configuration with current-user DPAPI into `%LOCALAPPDATA%\CodexBeerWidget\sync.dpapi`.
The publisher durably increments revision before sending, disables HTTP redirects, and uses normal
Windows TLS certificate validation. Never bypass certificate validation for a LAN relay; install a
trusted test CA explicitly or use a trusted HTTPS reverse proxy. No background sync runs without this file.

For disable/unpair, first revoke the device at the relay, close the widget, then remove `sync.dpapi`.
Do not restore an old revision file or reuse a device after losing its configuration: pair a new device.
Configuration errors disable sync without clearing quotas. Delivery errors do not turn source data stale;
the receiver independently tracks connection freshness. Diagnostic booleans appear in worker resources.

The transport has one-second WinHTTP phase timeouts. Shutdown can wait for an in-flight request;
these phase timeouts are not an absolute total request deadline. Real relay/TLS integration and mobile
delivery are validated in subsequent stages; this stage proves queue, deduplication, and retry behavior.
