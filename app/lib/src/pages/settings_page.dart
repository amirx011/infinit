import 'package:flutter/material.dart';

import '../app_state.dart';
import '../l10n.dart';
import '../theme.dart';
import '../widgets.dart';

/// Dedicated settings page.  Language, theme and display preferences used to
/// live in the header and the drawer footer; keeping them here makes the
/// workspace quieter and gives the preferences room to breathe.
class SettingsPage extends StatelessWidget {
  const SettingsPage({
    super.key,
    required this.settings,
    required this.l10n,
    required this.version,
    required this.engineReady,
  });

  final AppSettings settings;
  final L10n l10n;
  final String version;
  final bool engineReady;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: settings,
      builder: (context, _) {
        final l = L10n(settings.isPersian);
        final c = context.colors;
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            SoftCard(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  PageTitle(
                    tone: c.accentAlt,
                    icon: Icons.settings_rounded,
                    title: l.t('Settings', 'تنظیمات'),
                    subtitle: l.t(
                      'Language, appearance and display preferences for the whole app.',
                      'زبان، ظاهر و تنظیمات نمایش برای کل برنامه.',
                    ),
                  ),
                  const SizedBox(height: 22),
                  SectionLabel(
                    l.t('Theme', 'پوسته'),
                    icon: Icons.palette_outlined,
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.brightness_auto_rounded,
                          label: l.t('System', 'سیستم'),
                          selected: settings.themeMode == ThemeMode.system,
                          onTap: () => settings.setThemeMode(ThemeMode.system),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.light_mode_rounded,
                          label: l.t('Light', 'روشن'),
                          selected: settings.themeMode == ThemeMode.light,
                          onTap: () => settings.setThemeMode(ThemeMode.light),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.dark_mode_rounded,
                          label: l.t('Dark', 'تاریک'),
                          selected: settings.themeMode == ThemeMode.dark,
                          onTap: () => settings.setThemeMode(ThemeMode.dark),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    l.t(
                      'System follows the desktop or device appearance.',
                      'گزینهٔ سیستم از ظاهر سیستم‌عامل یا دستگاه پیروی می‌کند.',
                    ),
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 22),
                  SectionLabel(
                    l.t('Language', 'زبان'),
                    icon: Icons.translate_rounded,
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.language_rounded,
                          label: 'English',
                          selected: !settings.isPersian,
                          onTap: () => settings.setLocale(const Locale('en')),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.language_rounded,
                          label: 'فارسی',
                          selected: settings.isPersian,
                          onTap: () => settings.setLocale(const Locale('fa')),
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
            SoftCard(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  SectionLabel(
                    l.t('Display', 'نمایش'),
                    icon: Icons.text_fields_rounded,
                  ),
                  const SizedBox(height: 12),
                  Text(
                    l.t('Text size', 'اندازهٔ متن'),
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.text_fields_rounded,
                          label: l.t('Normal', 'معمولی'),
                          selected: settings.textScale < 1.1,
                          onTap: () => settings.setTextScale(1.0),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: _ChoiceTile(
                          icon: Icons.format_size_rounded,
                          label: l.t('Large', 'بزرگ'),
                          selected: settings.textScale >= 1.1,
                          onTap: () => settings.setTextScale(1.15),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  SwitchListTile(
                    value: settings.reduceMotion,
                    onChanged: settings.setReduceMotion,
                    contentPadding: EdgeInsets.zero,
                    activeThumbColor: c.accent,
                    title: Text(
                      l.t('Reduce motion', 'کاهش حرکت'),
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                    subtitle: Text(
                      l.t(
                        'Turn off page and result transitions.',
                        'انتقال‌های صفحه و نتیجه را خاموش می‌کند.',
                      ),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
            SoftCard(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  SectionLabel(
                    l.t('Engine', 'موتور محاسبات'),
                    icon: Icons.memory_rounded,
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Icon(
                        engineReady
                            ? Icons.check_circle_rounded
                            : Icons.error_outline_rounded,
                        size: 18,
                        color: engineReady ? c.success : c.warning,
                      ),
                      const SizedBox(width: 9),
                      Expanded(
                        child: Text(
                          engineReady
                              ? l.t(
                                  'Native engine ${version.isEmpty ? '' : 'v$version'}, exact rational arithmetic',
                                  'موتور بومی ${version.isEmpty ? '' : 'نسخهٔ $version'}، حساب گویای دقیق',
                                )
                              : l.t(
                                  'Native engine unavailable',
                                  'موتور بومی در دسترس نیست',
                                ),
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Align(
                    alignment: AlignmentDirectional.centerStart,
                    child: OutlinedButton.icon(
                      onPressed: settings.resetDefaults,
                      icon: const Icon(Icons.restart_alt_rounded, size: 18),
                      label: Text(
                        l.t('Restore defaults', 'بازگردانی پیش‌فرض‌ها'),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ],
        );
      },
    );
  }
}

class _ChoiceTile extends StatelessWidget {
  const _ChoiceTile({
    required this.icon,
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final IconData icon;
  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(14),
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 160),
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 14),
          decoration: BoxDecoration(
            color: selected ? c.accentSoft : c.cardAlt,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(
              color: selected ? c.accent : c.line,
              width: selected ? 1.4 : 1,
            ),
          ),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, size: 18, color: selected ? c.accent : c.muted),
              const SizedBox(width: 8),
              Flexible(
                child: Text(
                  label,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                    color: selected ? c.accent : c.text,
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
