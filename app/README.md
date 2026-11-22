# Infinit Lab: Flutter app

The interface for the native `libodecore` engine. It ships in twelve languages, handles right-to-left layouts, and runs on Linux, Android and Windows.

- `lib/main.dart`: app shell, dashboard, drawer, routing and keyboard shortcuts
- `lib/src/pages/`: workbenches, study articles and settings
- `lib/src/native_engine.dart`: FFI bridge to `ode_solve`, `ode_free` and `ode_core_version`
- `lib/src/settings_store.dart`: JSON settings on Linux and Windows, platform channel storage on Android
- `lib/src/study_articles.dart`: the nineteen article catalogue and the library page map
- `lib/src/theme.dart`, `widgets.dart`, `models.dart`, `l10n.dart`: shared UI, data models and strings

The default theme is dark and the default language is English. Mathematical content always renders left-to-right, independent of the interface direction.

## Build and test

From the repository root:

```bash
./scripts/build_linux.sh
./scripts/build_android.sh
./scripts/run_tests.sh
```

On Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
```

Or inside this directory:

```bash
flutter pub get --offline
flutter analyze
flutter test
flutter build linux --release
flutter build apk --release
flutter build windows --release
```

The native library must be in the Linux bundle, in `app/android/app/src/main/jniLibs/` for Android, or next to `infinit.exe` for Windows. Override its path with `INFINIT_LIB`.

Full documentation: [`../README.md`](../README.md)
