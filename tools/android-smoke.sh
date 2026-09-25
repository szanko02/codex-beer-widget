#!/usr/bin/env bash
set -euo pipefail
mkdir -p .local/android-smoke
adb() { timeout 15 adb "$@"; }
trap 'adb logcat -d > .local/android-smoke/logcat.txt 2>/dev/null || true; adb emu kill >/dev/null 2>&1 || true' EXIT
timeout 180 adb wait-for-device
booted=false
deadline=$((SECONDS + 180))
while (( SECONDS < deadline )); do
  if [[ $(adb shell getprop sys.boot_completed | tr -d '\r') == 1 ]]; then booted=true; break; fi
  sleep 2
done
[[ $booted == true ]]
echo 'Emulator boot completed.'
adb shell input keyevent 82
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell pm grant dev.codexbeer.android android.permission.POST_NOTIFICATIONS
adb logcat -c
adb shell am start -W -n dev.codexbeer.android/.MainActivity
sleep 8
adb shell pidof dev.codexbeer.android
adb exec-out screencap -p > .local/android-smoke/dashboard.png
adb shell appops set dev.codexbeer.android SYSTEM_ALERT_WINDOW allow
adb shell am start -W -a dev.codexbeer.SHOW_OVERLAY -n dev.codexbeer.android/.MainActivity
sleep 5
adb shell dumpsys activity services dev.codexbeer.android > .local/android-smoke/services.txt
grep -q 'isForeground=true' .local/android-smoke/services.txt
adb shell input keyevent 3
sleep 2
adb exec-out screencap -p > .local/android-smoke/overlay.png
adb logcat -d -b crash > .local/android-smoke/crashes.txt
if grep -q 'FATAL EXCEPTION' .local/android-smoke/crashes.txt; then cat .local/android-smoke/crashes.txt; exit 1; fi
adb shell am start -W -a dev.codexbeer.TOGGLE_OVERLAY -n dev.codexbeer.android/.MainActivity
sleep 2
adb shell dumpsys activity services dev.codexbeer.android > .local/android-smoke/services-stopped.txt
if grep -q 'isForeground=true' .local/android-smoke/services-stopped.txt; then exit 1; fi
echo 'Dashboard and foreground overlay smoke passed (API 35).'
