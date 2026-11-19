import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/app_state.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/settings_store.dart';

void main() {
  test('the first-launch language follows the build definition', () {
    // English unless the build passes INFINIT_DEFAULT_LOCALE, and English
    // again if that build names a language the interface does not ship.
    final expected = isSupportedLanguage(kDefaultLanguage)
        ? kDefaultLanguage
        : 'en';

    expect(defaultLocale().languageCode, expected);

    final settings = AppSettings(persistence: MemorySettingsPersistence());
    expect(settings.locale.languageCode, expected);
  });

  test('a Persian build defaults to Persian', () {
    if (kDefaultLanguage != 'fa') return;
    final settings = AppSettings(persistence: MemorySettingsPersistence());
    expect(settings.isPersian, isTrue);
  });
}
