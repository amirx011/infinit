# Infinit Lab — Flutter app

Bilingual English/Persian UI for the native `libodecore` engine.

- `lib/main.dart` — app shell, dashboard, drawer, and settings
- `lib/src/pages/` — derivative, integral, ODE, Wronskian, and envelope pages
- `lib/src/native_engine.dart` — FFI bridge to `ode_solve`, `ode_free`, and `ode_core_version`
- `lib/src/settings_store.dart` — JSON settings on Linux, `SharedPreferences` on Android
- `lib/src/theme.dart`, `widgets.dart`, `models.dart`, `l10n.dart` — shared UI, data models, and strings

Default theme is dark and default language is English. Math is always rendered left-to-right.

## Build and test

From the repository root:

```bash
./scripts/build_linux.sh
./scripts/build_android.sh
./scripts/run_tests.sh
```

Or inside this directory:

```bash
flutter pub get --offline
flutter analyze
flutter test
flutter build linux --release
flutter build apk --release
```

The native library must be in the Linux bundle or in `app/android/app/src/main/jniLibs/`. Override its path with `INFINIT_LIB`.

Full documentation: [`../README.md`](../README.md)
