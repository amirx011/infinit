# Myket upload bundle

Everything the store form asks for, and where it comes from.

## Package

| | |
|---|---|
| Package | `ir.infinit` |
| Version | `MAJOR.MINOR.PATCH` from `app/pubspec.yaml`, moved on by `scripts/bump_version.py` |
| File for Myket | `release/infinit-lab-<version>-android-fa.apk` |
| File for everyone else | `release/infinit-lab-<version>-android.apk` |
| Size | about 72 MB, one universal APK |
| ABIs | `arm64-v8a`, `armeabi-v7a`, `x86_64` |
| Minimum API | 24 |

The Persian file is the same build with
`--dart-define=INFINIT_DEFAULT_LOCALE=fa`, so the first launch is Persian and
the language switcher still lists every language. The user interface and the
engine are identical.

Check the signature before uploading; Myket rejects a debug-signed APK:

```bash
scripts/verify_apk_signature.sh release/infinit-lab-*-android-fa.apk
```

It must print `CN=Infinit Lab` and exit zero. Keep `app/android/infinit-release.jks`
and `app/android/key.properties` backed up: an update is only installable over
the published app when it carries the same key.

## Text

- `docs/store/myket-fa.txt` - full description, Persian, right-to-left.
- `docs/store/myket-fa-short.txt` - short description; the first line is the
  one-line summary, the second is the tagline.

Every line starts with U+200F (right-to-left mark) so the store does not
reorder a line that begins with a Latin word such as `Infinit Lab`. Keep that
character when editing.

## Images

Six PNG files, 1080x1920 (9:16), in `/home/RooTx/Desktop/myket/`:

| File | Shows |
|---|---|
| `myket-00-hero.png` | Title card: what the lab does, one formula |
| `myket-01-home.png` | Home screen and module groups |
| `myket-02-field.png` | A first-order equation solved, type and field |
| `myket-03-steps.png` | The step list of a solution |
| `myket-04-laplace.png` | Laplace workbench with the transform |
| `myket-05-topics.png` | The topic catalogue |

The cards were composed from phone screenshots with a local HTML + headless
Chrome pipeline that is not part of this repository, and the source captures are
no longer on disk. Rebuilding a card therefore starts with a fresh screenshot
from the app: a 16:9 phone capture, cropped to the app window, then fed to the
same layout.

`myket-03-steps.png` was captured before the math-rendering fix and still shows
one note as ASCII text, so it is the one card worth replacing first.

## Before pressing publish

1. `scripts/build_android.sh`, and the local-only Persian build
2. `scripts/verify_apk_signature.sh` on both APKs
3. Install the Persian APK on a phone and open a few workbenches
4. Upload the APK, both text files, and the six images

The **Release builds** workflow builds and signs the public APKs on a runner and
attaches them to the run. The Persian-default file for Myket is built only on
the maintainer's machine: neither its script nor the APK is committed, and the
release workflow refuses to publish one.

For later updates, run `adb install -r` over the published build before
uploading: it proves the new APK carries the same key the store accepted.
