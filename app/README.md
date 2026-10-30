# Infinit Lab — Flutter app

Bilingual (English/Persian), RTL-aware Flutter UI for the native `libodecore` engine. This directory builds the Linux and Android versions of the app.

## Layout

| Path | Responsibility |
|---|---|
| `lib/main.dart` | App shell, dashboard, navigation drawer, settings sheet |
| `lib/src/pages/` | Five pages: `ode`, `wronskian`, `envelope`, `integral`, `derivative` |
| `lib/src/native_engine.dart` | FFI bridge to `ode_solve` / `ode_free` / `ode_core_version` |
| `lib/src/models.dart` | JSON response model (`SolveResult`) |
| `lib/src/app_state.dart` | Theme and language state |
| `lib/src/settings_store.dart` | Settings persistence: JSON file on Linux, `SharedPreferences` on Android |
| `lib/src/theme.dart` | Dark/light themes |
| `lib/src/widgets.dart` | Shared result-display widgets |
| `lib/src/l10n.dart` | Bilingual UI strings |

- Defaults: dark theme, English language
- Preferences persist across runs
- Math is rendered LTR independently of the UI direction

## Build

From the repository root:

```bash
./scripts/build_linux.sh
./scripts/build_android.sh
```

Or directly:

```bash
flutter pub get
flutter build linux --release
flutter build apk --release
```

The native library must be present in the bundle (Linux) or in `app/android/app/src/main/jniLibs/` (Android); the scripts above handle both. You can override the native library path with `ODELAB_LIB`.

## Tests

```bash
flutter analyze
flutter test
```

The widget tests cover theme/language/settings, module navigation, the integral and derivative pages, and a real ODE solve through FFI. To run every layer from the repository root:

```bash
./scripts/run_tests.sh
```

Full project documentation: [`../README.md`](../README.md)
