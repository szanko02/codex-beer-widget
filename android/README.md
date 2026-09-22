# Android companion

Requires JDK 17, SDK platform `android-37.2`, and Android 8+ (API 26).
From this directory:

```sh
./gradlew :core-model:test testDebugUnitTest lintDebug :app:assembleDebug
```

The debug APK is `app/build/outputs/apk/debug/app-debug.apk`. GitHub Actions
builds it independently of the Windows CMake jobs.

## Local pairing

Follow `sync-server/README.md` to create an HTTPS relay and a five-minute QR.
Use the PC's LAN hostname/IP in its certificate and origin; `localhost` on the
phone is the phone itself. Open only the configured relay port on your private
network. Install the generated certificate explicitly on the test phone: only
debug builds trust user-installed CAs. Release builds require a trusted chain.
Scan the QR in the app, or paste its JSON. No Codex account credentials reach Android.

The dashboard and active overlay use WSS with reconnect backoff up to 60 seconds.
Closing both stops WSS; turning the screen off stops the overlay subscription.
WorkManager provides a best-effort 15-minute background fallback. Android may
delay it during Doze or after force-stop. Last values persist and become stale;
missing values never become 0%.

## Optional FCM

Create a Firebase Android app for `dev.codexbeer.android` and place its
`google-services.json` in `android/app/` (ignored by Git). The Google Services
plugin activates only when that file exists. On the relay, point
`GOOGLE_APPLICATION_CREDENTIALS` at a private Firebase service-account file.
Without these files, local HTTPS/WSS and WorkManager still work.

FCM carries only a change hint, device ID and revision; the phone fetches the
snapshot over authenticated HTTPS. Hints coalesce for 30 seconds and use normal
priority. The relay must be reachable from the phone (LAN or configured VPN).
FCM cannot make a private LAN endpoint reachable through mobile internet.

Allow notifications and overlay access explicitly in the app. Add the Codex tile
through Android's Quick Settings editor and widgets through the launcher.
Device-specific background restrictions, overlay gestures and real FCM delivery
still require physical-device acceptance; CI is not a substitute.
