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

برای مسیرهای قائم (متعامد)، `mode` را `"orthogonal"` بگذارید یا `orthogonal: true` بفرستید.

### ۲.۳.۱. مسیرهای مایل

```json
{
  "mode": "oblique",
  "input": "y = c*x",
  "c": "c",
  "angle": 45,
  "x": "x",
  "y": "y",
  "lang": "fa"
}
```

`angle` زاویهٔ ثابت تقاطع بر حسب درجه است و باید بین ۰ و ۹۰ باشد. رابطهٔ شیب‌ها:

`tan α = (m_t − m)/(1 + m_t m)` و بنابراین `m_t = (m + tan α)/(1 − m tan α)`.

زاویهٔ ۹۰ درجه به حالت قائم (`y′ = −1/y′`) تقلیل می‌یابد. خروجی این حالت در فیلدهای `trajectory`، `trajectory_explicit`، `trajectory_slope`، `trajectory_angle` و `trajectory_mode` برمی‌گردد.

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

### ۲.۶. تحلیل رفتار تابع

```json
{"mode":"behaviour","input":"x^3 - 3*x","x":"x","xmin":-3,"xmax":3}
```

مشتق اول و دوم، نقطه‌های بحرانی، نقطه‌های عطف و بازه‌های یکنوایی را برمی‌گرداند. پاسخ شامل
`derivative`، `second_derivative`، `critical`، `inflections` و `pieces` است.

### ۲.۷. مماس و خط عمود

```json
{"mode":"tangent","input":"x^2","x":"x","at":"1"}
```

`tangent`، `normal`، `tangent_slope` و `normal_slope` را می‌دهد. اگر مماس افقی باشد،
`normal_slope` تهی است و خط عمود قائم است.

### ۲.۸. بهینه‌سازی روی بازهٔ بسته

```json
{"mode":"optimize","input":"x^3 - 3*x","x":"x","xmin":-2,"xmax":2}
```

همهٔ کاندیدها (نقطه‌های سکون و دو سر بازه) در `candidates` می‌آیند و `maximum`، `minimum`،
`maximum_at` و `minimum_at` نتیجهٔ مقایسه‌اند.

### ۲.۹. نرخ‌های مرتبط

```json
{"mode":"related_rates","input":"x^2 + y^2 = 25","var":"x","rate_var":"y","t":"t","q_value":3,"w_value":4,"given_rate":2}
```

رابطه نسبت به زمان مشتق می‌شود و نرخ خواسته‌شده جدا می‌شود. اگر `q_value`، `w_value` و
`given_rate` داده شوند، پاسخ عددی هم در `numeric_value` می‌آید؛ در غیر این صورت نمادین می‌ماند.

### ۲.۱۰. قضیهٔ مقدار میانگین و رول

```json
{"mode":"mvt","input":"x^2","x":"x","a":0,"b":2}
```

فرض‌های پیوستگی و مشتق‌پذیری در `checks` بررسی می‌شوند، شیب قاطع در `slope` و نقطه‌های
تضمین‌شده در `points` با مقدارشان در `point_values` برمی‌گردند. اگر `f(a) = f(b)` باشد،
`rolle_applies` درست است.

### ۲.۱۱. قضیهٔ کوشی

```json
{"mode":"cauchy","input":"x^2","g":"x^3","x":"x","a":0,"b":1}
```

نسبت `f′(c)/g′(c)` با نسبت تغییرات مقایسه می‌شود؛ `g(a) = g(b)` به‌عنوان نقض فرض گزارش می‌شود.

### ۲.۱۲. قاعدهٔ هوپیتال

```json
{"mode":"lhopital","input":"sin(x)","g":"x","x":"x","point":"0"}
```

مبهم بودن کسر بررسی می‌شود و حد در `solution` یا به‌صورت واگرایی گزارش می‌شود.

### ۲.۱۳. میدان جهت و نمودار

```json
{"mode":"field","input":"(x+y)dx + (x+2y)dy = 0","x":"x","y":"y","xmin":-6,"xmax":6,"ymin":-6,"ymax":6,"cols":17,"rows":17,"curves":6}
```

شیب در هر نقطه از `y′ = −M/N` (برای فرم دیفرانسیلی) یا از خود `y′` گرفته می‌شود. برای
معادله‌های درجهٔ دوم در `y′` (کلرو و لاگرانژ) هر دو شاخه عددی نگه داشته می‌شوند و مسیر از
شاخهٔ نزدیک‌تر پیروی می‌کند.

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
  "trajectory": null,
  "trajectory_explicit": false,
  "trajectory_slope": null,
  "trajectory_angle": 90,
  "trajectory_mode": "",
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
| `mode` | string | `ode` / `wronskian` / `envelope` / `orthogonal` / `oblique` / `integral` / `derivative` |
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
| `orthogonal` | ExView? | جواب مسیرهای قائم (حالت ۹۰ درجه) |
| `orthogonal_explicit` | bool | جواب قائم صریح است |
| `trajectory` | ExView? | جواب مسیرهای مایل یا قائم |
| `trajectory_explicit` | bool | جواب مسیرها صریح است |
| `trajectory_slope` | ExView? | سمت راست `y′ = f(x,y)` برای رسم میدان جهت |
| `trajectory_angle` | double | زاویهٔ تقاطع بر حسب درجه (۹۰ برای قائم) |
| `trajectory_mode` | string | `orthogonal` یا `oblique` |
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
