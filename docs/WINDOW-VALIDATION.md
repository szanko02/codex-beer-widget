# Transparent window checkpoint

Direct2D renders premultiplied BGRA into a DXGI flip swap chain; DirectComposition presents it.
The normal window has `WS_EX_NOREDIRECTIONBITMAP`, `WS_EX_TOOLWINDOW` and `WS_EX_NOACTIVATE`.
The window region excludes the exterior and the handle hole; `HTTRANSPARENT` alone is not relied on.

Full click-through uses a separate `WS_EX_LAYERED | WS_EX_TRANSPARENT` tool window with the same image.
GPU readback is confined to that mode, cached at the current size and measured separately later.
The regular DirectComposition window remains the owner of application state and recovery commands.

Local Release build succeeded. The native image was visually inspected at 240 DIP:
transparent exterior, translucent glass, masked amber liquid, highlights and readable demo labels.
`--inspect` exposes the same renderer in an activatable taskbar window for UI inspection tools;
it does not validate the normal mode's no-activation behavior.

Initial static sample: working set 51.57 MiB, private commit 56.27 MiB (hardware-dependent).
The 50 MiB working-set target is not yet met. There is no continuous animation timer at this checkpoint.
Multi-monitor DPI transitions, physical click-through and focus preservation still require dedicated checks.

Stage 5 UI checks: dragging by the mug changed its desktop position. The settings slider reached
400 DIP and persisted it; Ctrl+Alt+B hid and restored the widget while keyboard focus stayed in settings.
The lower 80 DIP slider bound was exercised too. Recovery remains accessible through the tray and relaunch.
An initial settings visibility issue under a hidden Windows startup hint was fixed explicitly.
The earlier 54.94-second resource sample included dragging and UI inspection, so its 0.825% CPU
must not be reported as a static-idle benchmark.

Stage 8: after graphics-resource release was added, Ctrl+Alt+B removed the inspected widget from
the visible-window list and a second press restored it; the ring rendered correctly with current data.
Preset loading was exercised by selecting the saved preset from the list and applying it.
Restoring the standard theme switched back to the mug. These checks used `--inspect`.
The final window region separately includes the two caption capsules and secondary bar;
gaps between them are excluded. Saved monitor-relative offsets support changed monitor origins,
with primary-monitor fallback when the saved device is absent; physical topology changes remain untested.
