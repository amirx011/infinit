import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../app_state.dart';
import '../l10n.dart';
import '../theme.dart';
import '../widgets.dart';

/// One destination in the settings index. The keys point at the section
/// wrappers so the index can scroll the page without a second ScrollView.
class _SettingsDestination {
  const _SettingsDestination({
    required this.key,
    required this.icon,
    required this.label,
  });

  final GlobalKey key;
  final IconData icon;
  final String label;
}

const _startPages = <(int, String, String)>[
  (0, 'Home', 'خانه'),
  (2, 'Solve an equation', 'حل معادله'),
  (3, 'Wronskian', 'رونسکی'),
  (5, 'Derivative', 'مشتق'),
  (6, 'Integral', 'انتگرال'),
  (7, 'Limit', 'حد'),
  (8, 'Continuity', 'پیوستگی'),
  (9, 'Domain & range', 'دامنه و برد'),
  (23, 'Taylor & Maclaurin', 'تیلور و مکلورن'),
  (24, 'Integral applications', 'کاربردهای انتگرال'),
  (25, 'Partial derivatives', 'مشتق‌های جزئی'),
  (26, 'Series & convergence', 'سری و همگرایی'),
];

class SettingsPage extends StatefulWidget {
  const SettingsPage({
    super.key,
    required this.settings,
    required this.l10n,
    required this.version,
  });

  final AppSettings settings;
  final L10n l10n;
  final String version;

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final _appearanceKey = GlobalKey();
  final _typographyKey = GlobalKey();
  final _languageKey = GlobalKey();
  final _solutionsKey = GlobalKey();
  final _workspaceKey = GlobalKey();
  final _aboutKey = GlobalKey();
  int _active = 0;

  List<_SettingsDestination> get _destinations => [
    _SettingsDestination(
      key: _appearanceKey,
      icon: Icons.palette_outlined,
      label: widget.l10n.t('Appearance', 'ظاهر'),
    ),
    _SettingsDestination(
      key: _languageKey,
      icon: Icons.translate_rounded,
      label: widget.l10n.t('Locale', 'زبان رابط'),
    ),
    _SettingsDestination(
      key: _typographyKey,
      icon: Icons.text_fields_rounded,
      label: widget.l10n.t('Text size', 'اندازهٔ متن'),
    ),
    _SettingsDestination(
      key: _solutionsKey,
      icon: Icons.insights_rounded,
      label: widget.l10n.t('Solutions & graphs', 'حل و نمودار'),
    ),
    _SettingsDestination(
      key: _workspaceKey,
      icon: Icons.space_dashboard_outlined,
      label: widget.l10n.t('Start page', 'صفحهٔ شروع'),
    ),
    _SettingsDestination(
      key: _aboutKey,
      icon: Icons.info_outline_rounded,
      label: widget.l10n.t('About', 'درباره'),
    ),
  ];

  void _jumpTo(int index) {
    setState(() => _active = index);
    final context = _destinations[index].key.currentContext;
    if (context == null) return;
    Scrollable.ensureVisible(
      context,
      duration: widget.settings.reduceMotion
          ? Duration.zero
          : const Duration(milliseconds: 260),
      curve: Curves.easeOutCubic,
      alignment: 0.04,
    );
  }

  Future<void> _copyDiagnostics(L10n l) async {
    final s = widget.settings;
    final text = [
      'Infinit Lab ${widget.version}',
      'OS: ${Platform.operatingSystem} ${Platform.operatingSystemVersion}',
      'Theme: ${s.themeMode.name} · palette: ${s.palette}',
      'High contrast: ${s.highContrast}',
      'Compact layout: ${s.compact}',
      'Locale: ${s.locale.languageCode} · text scale: ${s.textScale}',
    ].join('\n');
    await Clipboard.setData(ClipboardData(text: text));
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          l.t('Diagnostics copied to the clipboard.', 'اطلاعات تشخیصی کپی شد.'),
        ),
      ),
    );
  }

  Future<void> _confirmReset(L10n l) async {
    final c = context.colors;
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(l.t('Restore defaults?', 'بازگردانی پیش‌فرض‌ها؟')),
        content: Text(
          l.t(
            'Theme, accent, language, text size and workspace preferences will return to their initial values.',
            'پوسته، رنگ، زبان، اندازهٔ متن و تنظیمات فضای کار به مقادیر اولیه برمی‌گردند.',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: Text(l.t('Cancel', 'انصراف')),
          ),
          FilledButton(
            style: FilledButton.styleFrom(
              backgroundColor: c.warning,
              foregroundColor: Colors.black,
            ),
            onPressed: () => Navigator.of(context).pop(true),
            child: Text(l.t('Restore', 'بازگردانی')),
          ),
        ],
      ),
    );
    if (ok == true) {
      await widget.settings.resetDefaults();
      if (!mounted) return;
      setState(() => _active = 0);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            l.t(
              'Default preferences restored.',
              'تنظیمات پیش‌فرض بازگردانی شد.',
            ),
          ),
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.settings,
      builder: (context, _) {
        final s = widget.settings;
        final l = L10n(s.locale);
        final c = context.colors;
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            SoftCard(
              padding: const EdgeInsets.fromLTRB(22, 22, 22, 20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  PageTitle(
                    tone: c.accentAlt,
                    icon: Icons.settings_rounded,
                    title: l.t('Settings', 'تنظیمات'),
                    subtitle: l.t(
                      'Tune how Infinit Lab looks, reads and starts. Everything is stored locally.',
                      'ظاهر، خوانایی و شروع Infinit Lab را تنظیم کنید. همهٔ تنظیمات محلی ذخیره می‌شوند.',
                    ),
                  ),
                  const SizedBox(height: 16),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      TypeBadge(
                        l.t('Local preferences', 'تنظیمات محلی'),
                        icon: Icons.lock_outline_rounded,
                      ),
                      if (widget.version.isNotEmpty)
                        TypeBadge(
                          'v${widget.version}',
                          icon: Icons.sell_outlined,
                          tone: c.muted,
                        ),
                    ],
                  ),
                  const SizedBox(height: 16),
                  _QuickPreferences(settings: s, l10n: l),
                ],
              ),
            ),
            const SizedBox(height: 16),
            LayoutBuilder(
              builder: (context, constraints) {
                final wide = constraints.maxWidth >= 880;
                final body = Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    _section(
                      key: _appearanceKey,
                      icon: Icons.palette_outlined,
                      title: l.t('Appearance', 'ظاهر'),
                      subtitle: l.t(
                        'Theme, accent colour, contrast and motion in one place.',
                        'پوسته، رنگ، کنتراست و حرکت در یک جا.',
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          _themeMode(context, s, l),
                          const SizedBox(height: 20),
                          Divider(color: c.line, height: 1),
                          const SizedBox(height: 18),
                          _accent(context, s, l),
                          const SizedBox(height: 14),
                          _SwitchRow(
                            icon: Icons.animation_rounded,
                            title: l.t('Reduce motion', 'کاهش حرکت'),
                            subtitle: l.t(
                              'Turn off page and result transitions.',
                              'انتقال‌های صفحه و نتیجه را خاموش می‌کند.',
                            ),
                            value: s.reduceMotion,
                            onChanged: s.setReduceMotion,
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 16),
                    _section(
                      key: _languageKey,
                      icon: Icons.translate_rounded,
                      title: l.t('Language', 'زبان'),
                      subtitle: l.t(
                        'The interface language, including labels and worked notes.',
                        'زبان رابط، شامل برچسب‌ها و توضیحات حل.',
                      ),
                      child: _language(context, s, l),
                    ),
                    const SizedBox(height: 16),
                    _section(
                      key: _typographyKey,
                      icon: Icons.text_fields_rounded,
                      title: l.t('Typography', 'تایپوگرافی'),
                      subtitle: l.t(
                        'Scale the interface for your screen and reading distance.',
                        'رابط را برای صفحه و فاصلهٔ مطالعهٔ خود بزرگ‌تر یا کوچک‌تر کنید.',
                      ),
                      child: _typography(context, s, l),
                    ),
                    const SizedBox(height: 16),
                    _section(
                      key: _solutionsKey,
                      icon: Icons.insights_rounded,
                      title: l.t('Solutions & graphs', 'حل و نمودار'),
                      subtitle: l.t(
                        'Choose how much of the reasoning is shown and how numbers are rounded.',
                        'انتخاب کنید چه مقدار از استدلال نمایش داده شود و اعداد چگونه گرد شوند.',
                      ),
                      child: _solutions(context, s, l),
                    ),
                    const SizedBox(height: 16),
                    _section(
                      key: _workspaceKey,
                      icon: Icons.space_dashboard_outlined,
                      title: l.t('Workspace', 'فضای کار'),
                      subtitle: l.t(
                        'Pick the page that opens when Infinit Lab starts.',
                        'صفحه‌ای را انتخاب کنید که هنگام شروع Infinit Lab باز می‌شود.',
                      ),
                      child: _workspace(context, s, l),
                    ),
                    const SizedBox(height: 16),
                    _section(
                      key: _aboutKey,
                      icon: Icons.info_outline_rounded,
                      title: l.t('Engine & about', 'موتور و درباره'),
                      subtitle: l.t(
                        'Build information, diagnostics and the way back to defaults.',
                        'اطلاعات نسخه، تشخیص خطا و بازگشت به پیش‌فرض‌ها.',
                      ),
                      child: _about(context, s, l),
                    ),
                  ],
                );
                if (!wide) {
                  return Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      _SettingsIndex(
                        destinations: _destinations,
                        active: _active,
                        onSelected: _jumpTo,
                        horizontal: true,
                      ),
                      const SizedBox(height: 16),
                      body,
                    ],
                  );
                }
                return Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    SizedBox(
                      width: 244,
                      child: _SettingsIndex(
                        destinations: _destinations,
                        active: _active,
                        onSelected: _jumpTo,
                        horizontal: false,
                      ),
                    ),
                    const SizedBox(width: 16),
                    Expanded(child: body),
                  ],
                );
              },
            ),
          ],
        );
      },
    );
  }

  Widget _section({
    required GlobalKey key,
    required IconData icon,
    required String title,
    required String subtitle,
    required Widget child,
  }) {
    return Container(
      key: key,
      child: SoftCard(
        padding: const EdgeInsets.fromLTRB(20, 18, 20, 20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _SectionHeader(icon: icon, title: title, subtitle: subtitle),
            const SizedBox(height: 18),
            child,
          ],
        ),
      ),
    );
  }

  Widget _themeMode(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _OptionGrid(
          children: [
            _OptionTile(
              icon: Icons.brightness_auto_rounded,
              label: l.t('System mode', 'حالت سیستم'),
              subtitle: l.t('Follow the OS', 'هماهنگ با سیستم'),
              selected: s.themeMode == ThemeMode.system,
              onTap: () => s.setThemeMode(ThemeMode.system),
            ),
            _OptionTile(
              icon: Icons.light_mode_rounded,
              label: l.t('Light mode', 'حالت روشن'),
              subtitle: l.t('Bright surfaces', 'سطح روشن'),
              selected: s.themeMode == ThemeMode.light,
              onTap: () => s.setThemeMode(ThemeMode.light),
            ),
            _OptionTile(
              icon: Icons.dark_mode_rounded,
              label: l.t('Dark mode', 'حالت تاریک'),
              subtitle: l.t('Low-light workspace', 'فضای کم‌نور'),
              selected: s.themeMode == ThemeMode.dark,
              onTap: () => s.setThemeMode(ThemeMode.dark),
            ),
          ],
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            Icon(Icons.info_outline_rounded, size: 15, color: c.muted),
            const SizedBox(width: 7),
            Expanded(
              child: Text(
                l.t(
                  'System follows the desktop or device appearance.',
                  'گزینهٔ سیستم از ظاهر سیستم‌عامل یا دستگاه پیروی می‌کند.',
                ),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          ],
        ),
      ],
    );
  }

  Widget _accent(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          l.t('Accent palette', 'پالت رنگی'),
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(fontWeight: FontWeight.w600, color: c.text),
        ),
        const SizedBox(height: 8),
        LayoutBuilder(
          builder: (context, constraints) {
            final columns = constraints.maxWidth >= 620
                ? 3
                : (constraints.maxWidth >= 380 ? 2 : 1);
            const gap = 10.0;
            final width =
                (constraints.maxWidth - gap * (columns - 1)) / columns;
            return Wrap(
              spacing: gap,
              runSpacing: gap,
              children: [
                for (final palette in OdePalette.all)
                  SizedBox(
                    width: width,
                    child: _PaletteTile(
                      palette: palette,
                      selected: s.palette == palette.id,
                      dark: Theme.of(context).brightness == Brightness.dark,
                      label: l.isFa ? palette.labelFa : palette.labelEn,
                      onTap: () => s.setPalette(palette.id),
                    ),
                  ),
              ],
            );
          },
        ),
        const SizedBox(height: 18),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 4),
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: c.line),
          ),
          child: Material(
            type: MaterialType.transparency,
            child: SwitchListTile(
              value: s.highContrast,
              onChanged: s.setHighContrast,
              contentPadding: EdgeInsets.zero,
              activeThumbColor: c.accent,
              title: Text(
                l.t('High contrast', 'کنتراست بالا'),
                style: Theme.of(context).textTheme.bodyMedium,
              ),
              subtitle: Text(
                l.t(
                  'Stronger borders and brighter text for bright rooms or low-vision use.',
                  'خطوط پررنگ‌تر و متن روشن‌تر برای محیط روشن یا دید کم.',
                ),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _typography(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    final labels = [
      l.t('Small', 'کوچک'),
      l.t('Normal', 'معمولی'),
      l.t('Large', 'بزرگ'),
      l.t('Extra large', 'خیلی بزرگ'),
    ];
    final sizes = ['Aa', 'Aa', 'Aa', 'Aa'];
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _OptionGrid(
          children: [
            for (var i = 0; i < kTextScaleOptions.length; i++)
              _OptionTile(
                icon: Icons.format_size_rounded,
                label: labels[i],
                subtitle: '${(kTextScaleOptions[i] * 100).round()}%',
                trailing: Text(
                  sizes[i],
                  style: TextStyle(
                    fontFamily: 'SpaceGrotesk',
                    fontSize: 13 + i * 2.5,
                    fontWeight: FontWeight.w700,
                    color: s.textScale == kTextScaleOptions[i]
                        ? c.accent
                        : c.muted,
                  ),
                ),
                selected: (s.textScale - kTextScaleOptions[i]).abs() < 0.005,
                onTap: () => s.setTextScale(kTextScaleOptions[i]),
              ),
          ],
        ),
        const SizedBox(height: 18),
        Container(
          width: double.infinity,
          padding: const EdgeInsets.fromLTRB(16, 14, 16, 16),
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(15),
            border: Border.all(color: c.line),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Icon(Icons.visibility_outlined, size: 15, color: c.muted),
                  const SizedBox(width: 6),
                  Text(
                    l.t('Live preview', 'پیش‌نمایش زنده'),
                    style: Theme.of(context).textTheme.bodySmall
                        ?.copyWith(fontWeight: FontWeight.w600),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                l.t('Every equation has a form.', 'هر معادله‌ای یک فرم دارد.'),
                style: Theme.of(context).textTheme.titleMedium
                    ?.copyWith(fontWeight: FontWeight.w700),
              ),
              const SizedBox(height: 4),
              Text(
                l.t(
                  'The answer, the solution path and the validation stay readable at every size.',
                  'جواب، مسیر حل و اعتبارسنجی در هر اندازه‌ای خوانا می‌مانند.',
                ),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _language(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    final current = languageFor(s.locale.languageCode);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        DropdownButtonFormField<String>(
          key: ValueKey('locale-${current.code}'),
          initialValue: current.code,
          isExpanded: true,
          decoration: InputDecoration(
            prefixIcon: Icon(Icons.translate_rounded, color: c.muted),
          ),
          items: [
            for (final language in kAppLanguages)
              DropdownMenuItem(
                value: language.code,
                child: Text(
                  '${language.nativeName}  ·  ${language.englishName}',
                  overflow: TextOverflow.ellipsis,
                ),
              ),
          ],
          onChanged: (code) {
            if (code != null) s.setLocale(Locale(code));
          },
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            Icon(
              current.rtl
                  ? Icons.format_textdirection_r_to_l_rounded
                  : Icons.format_textdirection_l_to_r_rounded,
              size: 16,
              color: c.muted,
            ),
            const SizedBox(width: 7),
            Text(
              current.rtl
                  ? l.t('Right to left', 'راست به چپ')
                  : l.t('Left to right', 'چپ به راست'),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
        ),
        const SizedBox(height: 10),
        Text(
          l.t(
            'Changing the language updates labels immediately. Worked solutions stay in English or Persian.',
            'تغییر زبان برچسب‌ها را فوراً به‌روز می‌کند. متن حل‌های نوشته‌شده به انگلیسی یا فارسی می‌ماند.',
          ),
          style: Theme.of(context).textTheme.bodySmall,
        ),
      ],
    );
  }

  Widget _solutions(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    final decimals = [
      (0, l.t('Automatic', 'خودکار')),
      (2, l.t('2 decimals', '۲ رقم اعشار')),
      (4, l.t('4 decimals', '۴ رقم اعشار')),
      (6, l.t('6 decimals', '۶ رقم اعشار')),
    ];
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SwitchRow(
          icon: Icons.route_outlined,
          title: l.t('Step-by-step solution', 'حل مرحله‌به‌مرحله'),
          subtitle: l.t(
            'Show the full reasoning path under every answer. Turn it off for a quick answer-only view.',
            'مسیر کامل استدلال را زیر هر جواب نشان بده. برای دیدن فقط جواب آن را خاموش کن.',
          ),
          value: s.showSteps,
          onChanged: s.setShowSteps,
        ),
        const SizedBox(height: 10),
        _SwitchRow(
          icon: Icons.verified_outlined,
          title: l.t('Validation checks', 'بررسی اعتبار'),
          subtitle: l.t(
            'Show the substitution checks, warnings and assumptions attached to an answer.',
            'بررسی‌های جایگذاری، هشدارها و فرض‌های هر جواب را نشان بده.',
          ),
          value: s.showChecks,
          onChanged: s.setShowChecks,
        ),
        const SizedBox(height: 10),
        _SwitchRow(
          icon: Icons.insights_rounded,
          title: l.t('Graphs', 'نمودارها'),
          subtitle: l.t(
            'Draw direction fields, solution curves, partial-sum plots and contour maps where they help.',
            'میدان جهت، منحنی جواب، مجموع جزئی و نقشهٔ تراز را هرجا مفید است رسم کن.',
          ),
          value: s.showGraphs,
          onChanged: s.setShowGraphs,
        ),
        const SizedBox(height: 18),
        Text(
          l.t('Decimal places', 'رقم‌های اعشار'),
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(fontWeight: FontWeight.w600, color: c.text),
        ),
        const SizedBox(height: 8),
        _OptionGrid(
          children: [
            for (final (value, label) in decimals)
              _OptionTile(
                icon: Icons.straighten_rounded,
                label: label,
                subtitle: value == 0
                    ? l.t('Keeps full precision', 'دقت کامل را نگه می‌دارد')
                    : value.toStringAsFixed(value),
                selected: s.decimalPlaces == value,
                onTap: () => s.setDecimalPlaces(value),
              ),
          ],
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            Icon(Icons.info_outline_rounded, size: 15, color: c.muted),
            const SizedBox(width: 7),
            Expanded(
              child: Text(
                l.t(
                  'Symbolic answers are always exact; this only changes numeric approximations.',
                  'جواب‌های نمادین همیشه دقیق‌اند؛ این تنظیم فقط تقریب‌های عددی را تغییر می‌دهد.',
                ),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          ],
        ),
      ],
    );
  }

  Widget _workspace(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    final valid = _startPages.any((page) => page.$1 == s.startPage);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          l.t('Start page', 'صفحهٔ شروع'),
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(fontWeight: FontWeight.w600, color: c.text),
        ),
        const SizedBox(height: 8),
        DropdownButtonFormField<int>(
          key: ValueKey('start-page-${s.startPage}'),
          initialValue: valid ? s.startPage : 0,
          isExpanded: true,
          decoration: InputDecoration(
            prefixIcon: Icon(Icons.rocket_launch_outlined, color: c.muted),
          ),
          items: [
            for (final page in _startPages)
              DropdownMenuItem(
                value: page.$1,
                child: Text(
                  l.t(page.$2, page.$3),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
          ],
          onChanged: (value) {
            if (value != null) s.setStartPage(value);
          },
        ),
        const SizedBox(height: 16),
        Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: c.line),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                l.t('Keyboard shortcuts', 'کلیدهای میان‌بر'),
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(fontWeight: FontWeight.w600, color: c.text),
              ),
              const SizedBox(height: 8),
              _ShortcutRow(
                keys: const ['Ctrl', 'K'],
                label: l.t('Search modules', 'جستجوی ماژول‌ها'),
              ),
              const SizedBox(height: 6),
              _ShortcutRow(
                keys: const ['Ctrl', ','],
                label: l.t('Open settings', 'باز کردن تنظیمات'),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _about(BuildContext context, AppSettings s, L10n l) {
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: c.line),
          ),
          child: Row(
            children: [
              Icon(Icons.storage_rounded, size: 17, color: c.muted),
              const SizedBox(width: 9),
              Expanded(
                child: Text(
                  l.t(
                    'Preferences are stored locally in your user configuration folder.',
                    'تنظیمات به‌صورت محلی در پوشهٔ پیکربندی کاربر ذخیره می‌شوند.',
                  ),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 14),
        Wrap(
          spacing: 10,
          runSpacing: 10,
          children: [
            OutlinedButton.icon(
              onPressed: () => _copyDiagnostics(l),
              icon: const Icon(Icons.copy_all_rounded, size: 17),
              label: Text(l.t('Copy diagnostics', 'کپی اطلاعات تشخیصی')),
            ),
            OutlinedButton.icon(
              onPressed: () => showLicensePage(
                context: context,
                applicationName: 'Infinit Lab',
                applicationVersion: widget.version,
              ),
              icon: const Icon(Icons.article_outlined, size: 17),
              label: Text(l.t('Licences', 'مجوزها')),
            ),
            OutlinedButton.icon(
              onPressed: () => _confirmReset(l),
              style: OutlinedButton.styleFrom(
                foregroundColor: c.warning,
                side: BorderSide(color: c.warning.withValues(alpha: 0.5)),
              ),
              icon: const Icon(Icons.restart_alt_rounded, size: 17),
              label: Text(l.t('Restore defaults', 'بازگردانی پیش‌فرض‌ها')),
            ),
          ],
        ),
      ],
    );
  }
}

class _QuickPreferences extends StatelessWidget {
  const _QuickPreferences({required this.settings, required this.l10n});

  final AppSettings settings;
  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    return Wrap(
      spacing: 10,
      runSpacing: 8,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        _QuickGroup(
          icon: Icons.brightness_6_outlined,
          children: [
            _QuickChip(
              label: l10n.t('System', 'سیستم'),
              selected: settings.themeMode == ThemeMode.system,
              onTap: () => settings.setThemeMode(ThemeMode.system),
            ),
            _QuickChip(
              label: l10n.t('Light', 'روشن'),
              selected: settings.themeMode == ThemeMode.light,
              onTap: () => settings.setThemeMode(ThemeMode.light),
            ),
            _QuickChip(
              label: l10n.t('Dark', 'تاریک'),
              selected: settings.themeMode == ThemeMode.dark,
              onTap: () => settings.setThemeMode(ThemeMode.dark),
            ),
          ],
        ),
      ],
    );
  }
}

class _QuickGroup extends StatelessWidget {
  const _QuickGroup({required this.icon, required this.children});

  final IconData icon;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.fromLTRB(9, 3, 3, 3),
      decoration: BoxDecoration(
        color: c.cardAlt,
        borderRadius: BorderRadius.circular(100),
        border: Border.all(color: c.line),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 15, color: c.muted),
          const SizedBox(width: 7),
          for (var i = 0; i < children.length; i++) ...[
            if (i > 0) const SizedBox(width: 4),
            children[i],
          ],
        ],
      ),
    );
  }
}

class _QuickChip extends StatelessWidget {
  const _QuickChip({
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Semantics(
      button: true,
      selected: selected,
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          onTap: onTap,
          borderRadius: BorderRadius.circular(100),
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 160),
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
            decoration: BoxDecoration(
              color: selected ? c.accentSoft : Colors.transparent,
              borderRadius: BorderRadius.circular(100),
              border: Border.all(
                color: selected ? c.accent : c.line,
                width: selected ? 1.3 : 1,
              ),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  label,
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                    color: selected ? c.accent : c.text,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  const _SectionHeader({
    required this.icon,
    required this.title,
    required this.subtitle,
  });

  final IconData icon;
  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 34,
          height: 34,
          decoration: BoxDecoration(
            color: c.accentSoft,
            borderRadius: BorderRadius.circular(10),
            border: Border.all(color: c.accent.withValues(alpha: 0.22)),
          ),
          child: Icon(icon, size: 18, color: c.accent),
        ),
        const SizedBox(width: 11),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                title,
                style: Theme.of(context).textTheme.titleMedium
                    ?.copyWith(fontSize: 15, fontWeight: FontWeight.w700),
              ),
              const SizedBox(height: 1),
              Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
        ),
      ],
    );
  }
}

class _SettingsIndex extends StatelessWidget {
  const _SettingsIndex({
    required this.destinations,
    required this.active,
    required this.onSelected,
    required this.horizontal,
  });

  final List<_SettingsDestination> destinations;
  final int active;
  final ValueChanged<int> onSelected;
  final bool horizontal;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final items = [
      for (var i = 0; i < destinations.length; i++)
        _SettingsIndexItem(
          icon: destinations[i].icon,
          label: destinations[i].label,
          selected: active == i,
          onTap: () => onSelected(i),
        ),
    ];
    if (horizontal) {
      return SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: Row(
          children: [
            for (var i = 0; i < items.length; i++) ...[
              if (i > 0) const SizedBox(width: 8),
              items[i],
            ],
          ],
        ),
      );
    }
    return Container(
      padding: const EdgeInsets.all(8),
      decoration: BoxDecoration(
        color: c.card,
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: c.line),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          for (var i = 0; i < items.length; i++) ...[
            if (i > 0) const SizedBox(height: 2),
            items[i],
          ],
        ],
      ),
    );
  }
}

class _SettingsIndexItem extends StatelessWidget {
  const _SettingsIndexItem({
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
        borderRadius: BorderRadius.circular(11),
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 160),
          padding: const EdgeInsets.symmetric(horizontal: 11, vertical: 10),
          decoration: BoxDecoration(
            color: selected ? c.accentSoft : Colors.transparent,
            borderRadius: BorderRadius.circular(11),
            border: Border.all(
              color: selected
                  ? c.accent.withValues(alpha: 0.3)
                  : Colors.transparent,
            ),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(icon, size: 17, color: selected ? c.accent : c.muted),
              const SizedBox(width: 8),
              Flexible(
                child: Text(
                  label,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 12.5,
                    fontWeight: selected ? FontWeight.w700 : FontWeight.w500,
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

class _OptionGrid extends StatelessWidget {
  const _OptionGrid({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final available = constraints.maxWidth;
        final count = available >= 640 ? children.length : 2;
        final safeCount = count < 1 ? 1 : count;
        const gap = 10.0;
        final width = (available - gap * (safeCount - 1)) / safeCount;
        return Wrap(
          spacing: gap,
          runSpacing: gap,
          children: [
            for (final child in children)
              SizedBox(width: width < 150 ? available : width, child: child),
          ],
        );
      },
    );
  }
}

class _OptionTile extends StatelessWidget {
  const _OptionTile({
    required this.icon,
    required this.label,
    required this.selected,
    required this.onTap,
    this.subtitle,
    this.trailing,
  });

  final IconData icon;
  final String label;
  final String? subtitle;
  final Widget? trailing;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Semantics(
      button: true,
      selected: selected,
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          onTap: onTap,
          borderRadius: BorderRadius.circular(14),
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 170),
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
            decoration: BoxDecoration(
              color: selected ? c.accentSoft : c.cardAlt,
              borderRadius: BorderRadius.circular(14),
              border: Border.all(
                color: selected ? c.accent : c.line,
                width: selected ? 1.4 : 1,
              ),
            ),
            child: Row(
              children: [
                Icon(icon, size: 18, color: selected ? c.accent : c.muted),
                const SizedBox(width: 9),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        label,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: TextStyle(
                          fontFamily: 'Vazirmatn',
                          fontSize: 13,
                          fontWeight: FontWeight.w600,
                          color: selected ? c.accent : c.text,
                        ),
                      ),
                      if (subtitle != null)
                        Text(
                          subtitle!,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: TextStyle(
                            fontFamily: 'Vazirmatn',
                            fontSize: 11,
                            color: selected ? c.accent : c.muted,
                          ),
                        ),
                    ],
                  ),
                ),
                if (trailing != null) ...[
                  const SizedBox(width: 6),
                  trailing!,
                ] else if (selected) ...[
                  const SizedBox(width: 6),
                  Icon(Icons.check_circle_rounded, size: 16, color: c.accent),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _PaletteTile extends StatelessWidget {
  const _PaletteTile({
    required this.palette,
    required this.selected,
    required this.dark,
    required this.label,
    required this.onTap,
  });

  final OdePalette palette;
  final bool selected;
  final bool dark;
  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final accent = dark ? palette.darkAccent : palette.lightAccent;
    final alt = dark ? palette.darkAlt : palette.lightAlt;
    return Semantics(
      button: true,
      selected: selected,
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          onTap: onTap,
          borderRadius: BorderRadius.circular(14),
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 170),
            padding: const EdgeInsets.symmetric(horizontal: 11, vertical: 10),
            decoration: BoxDecoration(
              color: selected ? c.accentSoft : c.cardAlt,
              borderRadius: BorderRadius.circular(14),
              border: Border.all(
                color: selected ? c.accent : c.line,
                width: selected ? 1.4 : 1,
              ),
            ),
            child: Row(
              children: [
                Container(
                  width: 26,
                  height: 26,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    gradient: LinearGradient(
                      begin: Alignment.topLeft,
                      end: Alignment.bottomRight,
                      colors: [accent, alt],
                    ),
                  ),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    label,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                      fontFamily: 'Vazirmatn',
                      fontSize: 12.5,
                      fontWeight: FontWeight.w600,
                      color: selected ? c.accent : c.text,
                    ),
                  ),
                ),
                if (selected)
                  Icon(Icons.check_rounded, size: 16, color: c.accent),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _SwitchRow extends StatelessWidget {
  const _SwitchRow({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.value,
    required this.onChanged,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.fromLTRB(14, 6, 8, 6),
      decoration: BoxDecoration(
        color: c.cardAlt,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: c.line),
      ),
      child: Row(
        children: [
          Icon(icon, size: 18, color: value ? c.accent : c.muted),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  title,
                  style: Theme.of(context).textTheme.bodyMedium
                      ?.copyWith(fontWeight: FontWeight.w600),
                ),
                Text(
                  subtitle,
                  style: Theme.of(context).textTheme.bodySmall
                      ?.copyWith(fontSize: 11.5),
                ),
              ],
            ),
          ),
          Switch(
            value: value,
            onChanged: onChanged,
            activeThumbColor: c.accent,
          ),
        ],
      ),
    );
  }
}

class _ShortcutRow extends StatelessWidget {
  const _ShortcutRow({required this.keys, required this.label});

  final List<String> keys;
  final String label;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      children: [
        for (var i = 0; i < keys.length; i++) ...[
          if (i > 0) const SizedBox(width: 4),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 3),
            decoration: BoxDecoration(
              color: c.card,
              borderRadius: BorderRadius.circular(6),
              border: Border.all(color: c.line),
            ),
            child: Text(
              keys[i],
              textDirection: TextDirection.ltr,
              style: TextStyle(
                fontFamily: 'SpaceGrotesk',
                fontSize: 10.5,
                fontWeight: FontWeight.w600,
                color: c.muted,
              ),
            ),
          ),
        ],
        const SizedBox(width: 10),
        Expanded(
          child: Text(
            label,
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(fontSize: 11.5),
          ),
        ),
      ],
    );
  }
}
