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
