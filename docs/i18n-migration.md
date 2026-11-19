# Language-free core and localisation migration — status

This file records the Phase 0 map and the live state of the migration described
in the owner's brief. It is documentation only; it is not program logic and it
is excluded from the L0 leak scan.


## Phase 0 map (initial state, before the migrations)

Counts are lines containing Persian/Arabic characters (L0 pattern).

| file | lines | category |
| --- | --- | --- |
| `app/lib/main.dart` | 47 | UI |
| `app/lib/src/charts.dart` | 6 | UI |
| `app/lib/src/l10n.dart` | 58 | translation table |
| `app/lib/src/l10n/ar.dart` | 232 | translation table |
| `app/lib/src/l10n/fa.dart` | 179 | translation table |
| `app/lib/src/l10n/de_topics.dart` | 90 | translation table |
| `app/lib/src/models.dart` | 1 | UI |
| `app/lib/src/native_engine.dart` | 10 | engine bridge |
| `app/lib/src/pages/behaviour_page.dart` | 1 | UI |
| `app/lib/src/pages/continuity_page.dart` | 2 | UI |
| `app/lib/src/pages/derivative_page.dart` | 2 | UI |
| `app/lib/src/pages/domain_range_page.dart` | 4 | UI |
| `app/lib/src/pages/envelope_page.dart` | 2 | UI |
| `app/lib/src/pages/integral_apps_page.dart` | 1 | UI |
| `app/lib/src/pages/integral_page.dart` | 1 | UI |
| `app/lib/src/pages/limit_page.dart` | 2 | UI |
| `app/lib/src/pages/linear_algebra_page.dart` | 6 | UI |
| `app/lib/src/pages/ode_page.dart` | 1 | UI |
| `app/lib/src/pages/optimize_page.dart` | 1 | UI |
| `app/lib/src/pages/partial_page.dart` | 1 | UI |
| `app/lib/src/pages/related_rates_page.dart` | 2 | UI |
| `app/lib/src/pages/series_page.dart` | 1 | UI |
| `app/lib/src/pages/settings_page.dart` | 12 | UI |
| `app/lib/src/pages/tangent_page.dart` | 1 | UI |
| `app/lib/src/pages/taylor_page.dart` | 1 | UI |
| `app/lib/src/pages/theorems_page.dart` | 1 | UI |
| `app/lib/src/pages/topic_workbench_page.dart` | 2 | UI |
| `app/lib/src/pages/wronskian_page.dart` | 1 | UI |
| `app/lib/src/study_articles.dart` | 88 | content / UI |
| `app/lib/src/theme.dart` | 6 | UI |
| `app/lib/src/theorems.dart` | 149 | content / UI |
| `app/lib/src/topic_catalog.dart` | 118 | content / UI |
| `app/lib/src/widgets.dart` | 12 | UI |
| `app/test/native_engine_test.dart` | 1 | test |
| `app/test/widget_test.dart` | 21 | test |
| `core/src/calculus_api.cpp` | 113 | engine |
| `core/src/calculus_apps.cpp` | 138 | engine |
| `core/src/calculus_more.cpp` | 25 | engine |
| `core/src/localize.hpp` | 2 | engine |
| `core/src/ode.cpp` | 523 | engine |
| `core/src/topics.cpp` | 57 | engine |
| `core/test/test_regression.cpp` | 1 | test |
| `scripts/install_linux.sh` | 1 | script |
| `testdata/brief_cases.json` | 1 | fixture |

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
| P1 L0–L3 guard rails | done | `python/tests/test_i18n_guard.py` — 5 passed |
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
lines. It now contains **zero entries**: no Persian/Arabic text exists outside
`app/assets/i18n/` (plus documentation and explicitly named i18n fixtures).

Everything else — UI, catalogues, tests, scripts and fixtures — is clean. The
allowlist may only shrink; the guard test fails if a count grows or a stale
entry remains.

## Tests

```
ctest --test-dir core/build --output-on-failure     # 7/7
python3 python/tests/brief_harness.py               # 47 passed, 0 failed
python3 -m pytest python/tests/test_i18n_guard.py   # 5 passed
flutter test --no-pub                               # 123 passed
flutter analyze --no-pub                            # no issues
```

## Remaining work

The native exact-ODE engine was the last piece: every first-order/linear ODE
step, Wronskian, envelope and trajectory sentence now travels as a catalogue
key with plain/math params. The `enTitle`/`enNote` tables, the bilingual
`typeFa` labels and the last escaped Persian strings are gone.

## Notes

- The engine never returns a user-facing sentence: steps/checks carry keys,
  failures carry codes (`error.<code>`), and technical exception text stays in
  `errorDetail` for logs only.
- Chart/plot diagnostics are keys too (`plot.*`); the UI resolves them with
  `L10n.t`, and exceptions are reduced to `plot.failed` instead of leaking raw
  technical text.
