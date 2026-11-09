# Language-free core and localisation migration — status

This file records the Phase 0 map and the live state of the migration described
in the owner's brief. It is documentation only; it is not program logic and it
is excluded from the L0 leak scan.

## Architecture (current)

```
engine (C++/Dart)  ->  keys, codes, math and plain params
assets/i18n/*.json ->  one flat catalogue per language (same keys everywhere)
UI (Flutter)       ->  L10n.t(key, params), direction and math widgets
math               ->  ExView / LaTeX, never inside a translated sentence
```

- `app/lib/src/i18n/catalog.dart` loads every `assets/i18n/<code>.json`.
- `app/lib/src/l10n.dart` is the only lookup entry point:
  `t(key, [params])` and `tList(prefix)`.
- A language file carries its own metadata (`__meta.english_name`,
  `__meta.native_name`, `__meta.rtl`, `__meta.order`), so adding a language
  means adding one JSON file; no Dart change is needed.
- Default language is Persian; English is the second language.
- Missing keys render as the key in debug and are caught by the guard tests.

## Task status

| Task | Status | Evidence |
| --- | --- | --- |
| T0 reference harness | done | `python/tests/brief_harness.py` — 47 passed |
| P1 L0–L3 guard rails | done | `python/tests/test_i18n_guard.py` — 4 passed |
| I1 catalogue + dispatch | done | `catalog.dart`, `l10n.dart` |
| I2 result data model | done | `core/src/solver.hpp`, `i18n/result.dart` |
| I3 direction in UI only | done | `i18n/sentence.dart`, `result_view.dart` |
| M1 errors and notices | done | engine emits `error.*` codes |
| M2 validations | done | `check.*` keys |
| M3 Newton / envelope | done | topic/envelope steps keyed |
| M4 Taylor / Laplace | done | `calculus_more.cpp` steps keyed |
| M5 RK4 / Heun | done | `topics.cpp` numerical ODE keyed |
| M6 Fourier / optimisation | done | `topics.cpp` and `calculus_apps.cpp` keyed |
| M7 UI chrome | done | main, pages, widgets, catalogues, theorems, articles |
| X1 English proof | done | `en.json` complete; language switch; 12 catalogues |
| Phase 4 one file per language | done | 12 JSON files; metadata inside them |

## Persian/Arabic leak table (L0)

At the start of this pass the allowlist covered 49 files and more than 3,000
lines. It now contains four engine files:

| file | lines with Persian/Arabic | category |
| --- | --- | --- |
| `core/src/calculus_api.cpp` | 113 | engine |
| `core/src/calculus_more.cpp` | 25 | engine |
| `core/src/localize.hpp` | 2 | engine |
| `core/src/ode.cpp` | 523 | engine |

Everything else — UI, catalogues, tests, scripts and fixtures — is clean. The
allowlist may only shrink; the guard test fails if a count grows or a stale
entry remains.

## Tests

```
ctest --test-dir core/build --output-on-failure     # 7/7
python3 python/tests/brief_harness.py               # 47 passed, 0 failed
python3 -m pytest python/tests/test_i18n_guard.py   # 4 passed
flutter test --no-pub                               # 99 passed
flutter analyze --no-pub                            # no issues
```

## Remaining work

The native exact-ODE engine still builds sentences in C++:

1. `calculus_api.cpp` — integration, derivative, limit and continuity titles,
   notes and errors (113 lines).
2. `calculus_more.cpp` — Taylor/series, integral applications and partial
   derivatives (25 lines).
3. `ode.cpp` — first-order/linear ODE steps, Wronskian, envelope and
   trajectory text plus the `enTitle`/`enNote` translation maps (523 lines).
4. `localize.hpp` — the last two-entry Persian→English message table.

The target remains the same as for the migrated sections: replace each
text-producing call with `titleKey`/`noteKey`/`nameKey`/`detailKey`, put plain
values in `params`, put formulas in the math fields, and move every sentence
into `fa.json` and `en.json` (then sync the other ten catalogues).
