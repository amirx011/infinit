# Running Infinit Lab on a local Android emulator

The app was verified end to end on an Android 11 (API 30, x86_64) AVD. This note
records the working setup so it can be reproduced without guessing.

## Image and AVD

```bash
export ANDROID_HOME="$HOME/Android/Sdk"
"$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" \
  "system-images;android-30;default;x86_64"
echo no | "$ANDROID_HOME/cmdline-tools/latest/bin/avdmanager" create avd \
  -n infinit30 -k "system-images;android-30;default;x86_64" --force
```

## Launch

```bash
export ANDROID_HOME="$HOME/Android/Sdk"
DISPLAY=:0 "$ANDROID_HOME/emulator/emulator" -avd infinit30 \
  -no-window -no-audio -no-boot-anim -gpu host \
  -no-snapshot -no-metrics -memory 1536 -cores 2
```

`-gpu host` is the important flag. With `-gpu off`, `-gpu guest` or
`-gpu swiftshader_indirect` the emulator initialises SwiftShader, whose JIT asks
SELinux for `execheap` on a heap mapping. On an enforcing Fedora host that
request is denied and the emulator dies with `SIGSEGV` a few seconds after the
first redraw. Using the host Mesa driver (Intel here) avoids SwiftShader
completely.

If the host has no usable GL driver, the SELinux boolean is the clean fix:

```bash
sudo setsebool -P selinuxuser_execheap 1
```

## Install and run

```bash
ADB="$ANDROID_HOME/platform-tools/adb"
"$ADB" install -r release/infinit-lab-1.0.0-android.apk
"$ADB" shell wm size 640x1280
"$ADB" shell wm density 240
"$ADB" shell am start -n ir.infinit/.MainActivity
"$ADB" exec-out screencap -p > android-home.png
```

The 320x640 default panel is too narrow for the header; the size/density
override above produces a normal phone layout.

## Launcher icon switching

Each accent palette has its own pre-rendered icon behind an `activity-alias`.
Android cannot recolor a launcher icon in place, and changing the enabled state
of the alias that owns a running task closes that task even with
`DONT_KILL_APP`. The aliases therefore point at `LauncherActivity`, a
zero-work trampoline that hands the launch to `MainActivity` and finishes. The
task belongs to `MainActivity`, so the alias can be switched the moment the
palette changes, while the app is still on screen.

`applyLauncherIcon` enables the new alias first and turns the old one off
second, so the package always keeps one launcher entry and the launcher never
renders the app as disabled or greyed out.

Every alias except the boot alias has `enabled="false"` in the manifest, so
resetting it with `COMPONENT_ENABLED_STATE_DEFAULT` hides it without an explicit
disable, which is the request Android treats as a package change. Only the first
switch, away from `LauncherRose`, sends an explicit `DISABLED`, because that
alias has to start enabled for a fresh install to have an icon at all.

`LauncherRoseAlt` is an identical twin of the boot icon used when the user
returns to rose: `LauncherRose` stays disabled after the first switch.

To check the active alias:

```bash
"$ADB" shell cmd package query-activities --brief \
  -a android.intent.action.MAIN -c android.intent.category.LAUNCHER |
  grep ir.infinit
```

There should be exactly one `ir.infinit` entry at any time.

## What was checked

- Home hero, drawer groups, and the ODE / Derivative pages
- Settings: Appearance, Language, Typography, and Solutions & graphs
- Numeric fallback `y'' = y' + y^2` (Approximate, RK4 trajectories, no invented
  closed form)
- Higher-order solution graph and the `f` / `f'` derivative graph
- Turning **Graphs** off removes the plot, turning it back on restores it
- Switching the accent palette keeps the session alive and updates the launcher
  icon immediately, without leaving the app
