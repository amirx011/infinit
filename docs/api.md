# API مرجع Infinit Lab

نسخهٔ فعلی: **1.0.0**

هستهٔ بومی یک C ABI کوچک و یک قرارداد JSON دارد. UI، تست‌ها و ابزارهای بیرونی همگی از همین مسیر استفاده می‌کنند.

---

## ۱. C ABI

```c
#include "odecore.h"

char*       ode_solve(const char* request_json);
void        ode_free(char* text);
const char* ode_core_version(void);
```

| تابع | توضیح |
|---|---|
| `ode_solve` | درخواست JSON را می‌گیرد و یک رشتهٔ JSON تازه (`malloc`) برمی‌گرداند. در خطا هم JSON برمی‌گردد (`ok=false`). |
| `ode_free` | حافظهٔ رشتهٔ برگشتی را آزاد می‌کند. |
| `ode_core_version` | رشتهٔ ثابت نسخه، مثلاً `"1.0.0"`. |

قواعد:

- ورودی و خروجی UTF-8 هستند.
- `ode_solve` هرگز `NULL` برنمی‌گرداند مگر کمبود حافظه.
- فراخوانی‌ها مستقل‌اند؛ وضعیت سراسری تغییر نمی‌کند.

---

## ۲. حالت‌های درخواست

### ۲.۱. حل معادله

```json
{
  "mode": "ode",
  "input": "y' = y",
  "x": "x",
  "y": "y",
  "particular": "",
  "lang": "en"
}
```

| فیلد | نوع | پیش‌فرض | توضیح |
|---|---|---|---|
| `mode` | string | `"ode"` | نوع عملیات |
| `input` | string | — | معادله |
| `x` | string | `"x"` | متغیر مستقل |
| `y` | string | `"y"` | متغیر وابسته |
| `particular` | string | `""` | جواب خصوصی اختیاری برای ریکاتی |
| `lang` | string | `"en"` | زبان متن گام‌ها/خطاها: `en` یا `fa` |

### ۲.۲. رونسکی

```json
{
  "mode": "wronskian",
  "functions": ["sin(x)", "cos(x)"],
  "x": "x"
}
```

`functions` می‌تواند آرایهٔ رشته باشد (حداکثر ۶ تابع). رشتهٔ تکی هم پذیرفته می‌شود.

### ۲.۳. پوش خانواده

```json
{
  "mode": "envelope",
  "input": "y = c*x + c^2",
  "c": "c",
  "orthogonal": false,
  "x": "x",
  "y": "y"
}
```

برای مسیرهای متعامد، `mode` را `"orthogonal"` بگذارید یا `orthogonal: true` بفرستید.

### ۲.۴. انتگرال نامعین

```json
{
  "mode": "integral",
  "input": "2*x*cos(x^2)",
  "x": "x",
  "lang": "en"
}
```

روش انتگرال‌گیری به‌صورت خودکار انتخاب می‌شود و در فیلد `method` برمی‌گردد: `table`، `substitution`، `parts`، `rational`، `trig`، `expand`، `sum` یا `constant`. اگر پادمشتق مقدماتی پیدا نشود، `solution` شامل گرهٔ `Integral(...)` و `closed_form=false` است.

پاسخ شامل `checks` است: مشتق جواب با انتگرال‌ده مقایسه می‌شود.

### ۲.۵. مشتق

```json
{
  "mode": "derivative",
  "input": "x^3*sin(x)",
  "x": "x",
  "order": 1
}
```

`order` پیش‌فرض `1` است؛ رابط کاربری مرتبهٔ اول و دوم را ارائه می‌دهد. `type_id` مقدار `"derivative"` و `type_fa` متن «مشتق مرتبهٔ اول/دوم» دارد.

---

## ۳. پاسخ

نمونه (کوتاه‌شده):

```json
{
  "ok": true,
  "mode": "ode",
  "type_id": "linear",
  "type_fa": "خطی مرتبهٔ اول",
  "type_en": "First-order linear",
  "method": "",
  "variable": "x",
  "order": 1,
  "engine": "native",
  "error": "",
  "closed_form": true,
  "time_ms": 0,
  "normalized": {"plain": "...", "pretty": "...", "latex": "..."},
  "solution": {"plain": "C*exp(x)", "pretty": "C·exp(x)", "latex": "C e^{x}"},
  "is_explicit": true,
  "explicit_y": {"plain": "C*exp(x)", "pretty": "C·exp(x)", "latex": "C e^{x}"},
  "param_x": null,
  "param_y": null,
  "param_name": "p",
  "singular": [],
  "wronskian": null,
  "independent": false,
  "envelope": [],
  "orthogonal": null,
  "orthogonal_explicit": false,
  "functions": [],
  "family": "",
  "steps": [
    {
      "title": "شناسایی نوع",
      "note": "...",
      "math": {"plain": "...", "pretty": "...", "latex": "..."}
    }
  ],
  "checks": [
    {"name": "جایگذاری در معادله", "ok": true, "detail": "..."}
  ],
  "warnings": [],
  "assumptions": []
}
```

### فیلدهای اصلی

| فیلد | نوع | معنی |
|---|---|---|
| `ok` | bool | تحلیل با موفقیت انجام شد |
| `mode` | string | `ode` / `wronskian` / `envelope` / `orthogonal` / `integral` / `derivative` |
| `type_id` | string | شناسهٔ انگلیسی نوع |
| `type_fa` | string | نام فارسی نوع |
| `type_en` | string | نام انگلیسی نوع |
| `method` | string | روش تشخیص‌داده‌شده (انتگرال) |
| `variable` | string | متغیر مستقل |
| `order` | int | مرتبهٔ معادله یا مشتق |
| `closed_form` | bool | جواب شامل انتگرال غیرمقدماتی نیست |
| `solution` | ExView? | جواب ضمنی یا صریح |
| `explicit_y` | ExView? | جواب صریح `y(x)` در صورت وجود |
| `is_explicit` | bool | `explicit_y` معتبر است |
| `param_x` / `param_y` | ExView? | جواب پارامتری (لاگرانژ) |
| `singular` | ExView[] | جواب‌های منفرد |
| `wronskian` | ExView? | دترمینان رونسکی |
| `independent` | bool | استقلال خطی |
| `envelope` | ExView[] | منحنی‌های پوش |
| `orthogonal` | ExView? | جواب مسیرهای متعامد |
| `orthogonal_explicit` | bool | جواب متعامد صریح است |
| `steps` | Step[] | مسیر حل |
| `checks` | Check[] | گواهی‌های اعتبار |
| `warnings` | string[] | هشدارها (مثلاً «پوش ندارد») |
| `assumptions` | string[] | فرض‌های دامنه |

### `ExView`

هر عبارت سه نمایش دارد:

| کلید | کاربرد |
|---|---|
| `plain` | ASCII، مناسب پارس مجدد و تست |
| `pretty` | یونیکد با کسرها، توان‌های بالانویس، `∫`، `√` |
| `latex` | کد LaTeX برای کپی/گزارش |

اگر عبارت وجود نداشته باشد، مقدار `null` است.

### `Step`

```json
{"title": "عامل انتگرال‌ساز", "math": {"plain": "...", ...}, "note": "..."}
```

`math` می‌تواند `null` باشد (گام توضیحی).

### `Check`

```json
{"name": "جایگذاری در معادله", "ok": true, "detail": "..."}
```

---

## ۴. خطاها

خطاها هم با HTTP-like payload برمی‌گردند:

```json
{
  "ok": false,
  "mode": "ode",
  "error": "این معادله با روش‌های استاندارد این نسخه حل نشد.",
  ...
}
```

خطاهای رایج:

| پیام | علت |
|---|---|
| `معادله هیچ مشتقی ندارد` | ورودی ODE نیست |
| `معادله نسبت به y′ خطی نیست...` | شکل پشتیبانی‌نشده |
| `ضرایب عددی بیش از حد بزرگ شدند` | سرریز `Rational` |
| `شکل دیفرانسیلی معادله خوانده نشد` | `dx`/`dy` مبهم |
| `درخواست نامعتبر است` | JSON یا فیلد ناقص |

---

## ۵. نمونه‌های `curl`-like (بدون سرور)

هسته سرور ندارد؛ از Python `ctypes`:

```python
import ctypes, json

lib = ctypes.CDLL("core/build/lib/libodecore.so")
lib.ode_solve.argtypes = [ctypes.c_char_p]
lib.ode_solve.restype = ctypes.c_void_p
lib.ode_free.argtypes = [ctypes.c_void_p]

ptr = lib.ode_solve(json.dumps({"mode": "ode", "input": "y'' + y = 0"}).encode())
try:
    print(ctypes.string_at(ptr).decode())
finally:
    lib.ode_free(ptr)
```

از Dart:

```dart
final engine = NativeEngine.instance;
final result = engine.solveOde("y' = y");
print(result.primary?.pretty);
```

---

## ۶. نسخه‌پذیری

- افزودن فیلد جدید به پاسخ، سازگار است (کلاینت‌ها باید فیلد ناشناخته را نادیده بگیرند).
- تغییر معنی فیلد موجود یا حذف آن، نسخهٔ major را افزایش می‌دهد.
- `ode_core_version()` و فیلد `engine` برای گزارش خطا مفیدند.

---

## ۷. نکات امنیتی

- پارسر JSON محدود و بازگشتی است؛ عمق ورودی توسط UI کنترل می‌شود.
- پارسر ریاضی سقف عمق (۲۰۰) دارد.
- هیچ کد ورودی اجرا نمی‌شود؛ فقط AST ساخته می‌شود.
- `ode_solve` رشتهٔ ورودی را تغییر نمی‌دهد.
