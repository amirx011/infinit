# Infinit Lab — اپ Flutter

رابط دوزبانه (انگلیسی/فارسی) و راست‌به‌چپ برای هستهٔ بومی `libodecore`. این پوشه، نسخهٔ لینوکس و اندروید برنامه را می‌سازد.

## ساختار

| مسیر | مسئولیت |
|---|---|
| `lib/main.dart` | پوستهٔ برنامه، داشبورد، منوی کناری و شیت تنظیمات |
| `lib/src/pages/` | پنج صفحهٔ `ode`، `wronskian`، `envelope`، `integral` و `derivative` |
| `lib/src/native_engine.dart` | پل FFI به `ode_solve` / `ode_free` / `ode_core_version` |
| `lib/src/models.dart` | مدل JSON پاسخ هسته (`SolveResult`) |
| `lib/src/app_state.dart` | وضعیت تم و زبان |
| `lib/src/settings_store.dart` | ذخیرهٔ تنظیمات: فایل JSON در لینوکس، `SharedPreferences` در اندروید |
| `lib/src/theme.dart` | تم تاریک/روشن |
| `lib/src/widgets.dart` | ویجت‌های مشترک نمایش نتیجه |
| `lib/src/l10n.dart` | متن‌های دوزبانه |

- حالت پیش‌فرض: تم تاریک، زبان انگلیسی
- تنظیمات بین اجراها حفظ می‌شوند
- نمایش ریاضی با جهت LTR مستقل از جهت رابط

## ساخت

از ریشهٔ پروژه:

```bash
./scripts/build_linux.sh
./scripts/build_android.sh
```

یا مستقیماً:

```bash
flutter pub get
flutter build linux --release
flutter build apk --release
```

کتابخانهٔ بومی باید در مسیر bundle (لینوکس) یا `app/android/app/src/main/jniLibs/` (اندروید) موجود باشد؛ اسکریپت‌های بالا این کار را انجام می‌دهند. مسیر دلخواه کتابخانه را می‌توان با `ODELAB_LIB` مشخص کرد.

## آزمون

```bash
flutter analyze
flutter test
```

تست‌های ویجت، تم/زبان/تنظیمات، ناوبری ماژول‌ها، انتگرال و مشتق و حل واقعی ODE از طریق FFI را پوشش می‌دهند. برای اجرای همهٔ لایه‌ها از ریشهٔ پروژه:

```bash
./scripts/run_tests.sh
```

مستندات کامل پروژه: [`../README.md`](../README.md)
