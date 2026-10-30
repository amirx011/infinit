import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import 'src/app_state.dart';
import 'src/l10n.dart';
import 'src/native_engine.dart';
import 'src/pages/derivative_page.dart';
import 'src/pages/envelope_page.dart';
import 'src/pages/integral_page.dart';
import 'src/pages/ode_page.dart';
import 'src/pages/wronskian_page.dart';
import 'src/theme.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final settings = AppSettings();
  await settings.load();
  runApp(InfinitApp(settings: settings));
}

class InfinitApp extends StatelessWidget {
  const InfinitApp({super.key, required this.settings});

  final AppSettings settings;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: settings,
      builder: (context, _) {
        return MaterialApp(
          title: 'Infinit Lab',
          debugShowCheckedModeBanner: false,
          theme: buildOdeTheme(dark: false),
          darkTheme: buildOdeTheme(dark: true),
          themeMode: settings.themeMode,
          locale: settings.locale,
          supportedLocales: const [Locale('en'), Locale('fa')],
          localizationsDelegates: const [
            GlobalMaterialLocalizations.delegate,
            GlobalWidgetsLocalizations.delegate,
            GlobalCupertinoLocalizations.delegate,
          ],
          home: HomeScreen(settings: settings),
        );
      },
    );
  }
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key, required this.settings});

  final AppSettings settings;

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final _scaffoldKey = GlobalKey<ScaffoldState>();
  int _index = 0;
  NativeEngine? _engine;
  String? _engineError;
  String _version = '';

  @override
  void initState() {
    super.initState();
    try {
      _engine = NativeEngine.instance;
      _version = _engine!.version;
    } catch (e) {
      _engineError = e.toString();
    }
  }

  Widget _page(L10n l10n) {
    switch (_index) {
      case 1:
        return OdePage(
          key: const ValueKey('ode'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 2:
        return WronskianPage(
          key: const ValueKey('wronskian'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 3:
        return EnvelopePage(
          key: const ValueKey('envelope'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 4:
        return IntegralPage(
          key: const ValueKey('integral'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 5:
        return DerivativePage(
          key: const ValueKey('derivative'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      default:
        return _HomePage(
          l10n: l10n,
          onOpen: (i) => setState(() => _index = i),
        );
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = L10n(widget.settings.isPersian);
    final c = context.colors;
    return Directionality(
      textDirection: l10n.isFa ? TextDirection.rtl : TextDirection.ltr,
      child: Scaffold(
        key: _scaffoldKey,
        drawer: _AppDrawer(
          l10n: l10n,
          selected: _index,
          onSelected: (i) {
            setState(() => _index = i);
            Navigator.of(context).pop();
          },
        ),
        body: Container(
          decoration: pageBackground(c),
          child: SafeArea(
            child: Center(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 1120),
                child: ListView(
                  padding: const EdgeInsets.fromLTRB(22, 20, 22, 44),
                  children: [
                    _Header(
                      l10n: l10n,
                      settings: widget.settings,
                      onMenu: () => _scaffoldKey.currentState?.openDrawer(),
                      onSettings: () => _openSettings(context),
                    ),
                    const SizedBox(height: 26),
                    AnimatedSwitcher(
                      duration: const Duration(milliseconds: 240),
                      switchInCurve: Curves.easeOutCubic,
                      switchOutCurve: Curves.easeInCubic,
                      transitionBuilder: (child, animation) => FadeTransition(
                        opacity: animation,
                        child: SlideTransition(
                          position: Tween<Offset>(
                            begin: const Offset(0, 0.02),
                            end: Offset.zero,
                          ).animate(animation),
                          child: child,
                        ),
                      ),
                      child: _page(l10n),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

  void _openSettings(BuildContext context) {
    showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (_) => _SettingsSheet(
        settings: widget.settings,
        version: _version,
        engineReady: _engine != null,
      ),
    );
  }
}

class _AppDrawer extends StatelessWidget {
  const _AppDrawer({
    required this.l10n,
    required this.selected,
    required this.onSelected,
  });

  final L10n l10n;
  final int selected;
  final ValueChanged<int> onSelected;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final mainItems = [
      (index: 0, icon: Icons.home_rounded, label: l10n.t('Home', 'خانه')),
      (
        index: 1,
        icon: Icons.functions_rounded,
        label: l10n.t('Differential equations', 'معادلات دیفرانسیل'),
      ),
      (
        index: 2,
        icon: Icons.grid_on_rounded,
        label: l10n.t('Wronskian', 'رونسکی'),
      ),
      (
        index: 3,
        icon: Icons.join_inner_rounded,
        label: l10n.t('Envelope & orthogonal', 'پوش و مسیرهای متعامد'),
      ),
    ];
    final calculusItems = [
      (
        index: 4,
        icon: Icons.integration_instructions_rounded,
        label: l10n.t('Integral', 'انتگرال'),
      ),
      (
        index: 5,
        icon: Icons.drive_file_rename_outline_rounded,
        label: l10n.t('Derivative', 'مشتق'),
      ),
    ];
    return Drawer(
      backgroundColor: c.card,
      child: SafeArea(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(20, 22, 20, 18),
              child: Row(
                children: [
                  Container(
                    width: 44,
                    height: 44,
                    decoration: BoxDecoration(
                      color: c.cardAlt,
                      borderRadius: BorderRadius.circular(14),
                      border: Border.all(color: c.accent.withValues(alpha: 0.35)),
                    ),
                    child: Icon(Icons.all_inclusive_rounded, color: c.accent, size: 26),
                  ),
                  const SizedBox(width: 11),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Infinit Lab',
                            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                                  fontFamily: 'SpaceGrotesk',
                                  fontSize: 17,
                                  fontWeight: FontWeight.w700,
                                  letterSpacing: -0.2,
                                )),
                        Text(l10n.t('Modules', 'ماژول‌ها'),
                            style: Theme.of(context).textTheme.bodySmall),
                      ],
                    ),
                  ),
                ],
              ),
            ),
            Divider(color: c.line, height: 1),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 12),
                children: [
                  for (final item in mainItems)
                    _DrawerTile(
                      icon: item.icon,
                      label: item.label,
                      selected: selected == item.index,
                      onTap: () => onSelected(item.index),
                    ),
                  const SizedBox(height: 10),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
                    child: Text(
                      l10n.t('CALCULUS', 'حساب دیفرانسیل و انتگرال'),
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                            fontSize: 10.5,
                            fontWeight: FontWeight.w700,
                            letterSpacing: 0.7,
                          ),
                    ),
                  ),
                  for (final item in calculusItems)
                    _DrawerTile(
                      icon: item.icon,
                      label: item.label,
                      selected: selected == item.index,
                      onTap: () => onSelected(item.index),
                    ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _DrawerTile extends StatelessWidget {
  const _DrawerTile({
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
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Material(
        color: selected ? c.accentSoft : Colors.transparent,
        borderRadius: BorderRadius.circular(12),
        child: InkWell(
          onTap: onTap,
          borderRadius: BorderRadius.circular(12),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 12),
            child: Row(
              children: [
                Icon(icon, size: 19, color: selected ? c.accent : c.muted),
                const SizedBox(width: 11),
                Expanded(
                  child: Text(
                    label,
                    style: TextStyle(
                      fontSize: 13,
                      fontWeight: selected ? FontWeight.w700 : FontWeight.w500,
                      color: selected ? c.accent : c.text,
                    ),
                  ),
                ),
                if (selected)
                  Container(
                    width: 5,
                    height: 5,
                    decoration: BoxDecoration(color: c.accent, shape: BoxShape.circle),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _Header extends StatelessWidget {
  const _Header({
    required this.l10n,
    required this.settings,
    required this.onMenu,
    required this.onSettings,
  });

  final L10n l10n;
  final AppSettings settings;
  final VoidCallback onMenu;
  final VoidCallback onSettings;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      children: [
        _RoundIconButton(
          tooltip: l10n.t('Open menu', 'باز کردن منو'),
          icon: Icons.menu_rounded,
          onPressed: onMenu,
        ),
        const SizedBox(width: 8),
        Container(
          width: 50,
          height: 50,
          decoration: BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [c.accent, c.accent.withValues(alpha: 0.68)],
            ),
            borderRadius: BorderRadius.circular(16),
            boxShadow: [
              BoxShadow(
                color: c.accent.withValues(alpha: 0.22),
                blurRadius: 20,
                offset: const Offset(0, 9),
              ),
            ],
          ),
          child: Icon(
            Icons.all_inclusive_rounded,
            color: settings.isDark ? const Color(0xFF0A0714) : Colors.white,
            size: 29,
          ),
        ),
        const SizedBox(width: 14),
        Expanded(
          child: Text(
            'Infinit Lab',
            style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                  fontFamily: 'SpaceGrotesk',
                  fontSize: 27,
                  fontWeight: FontWeight.w700,
                  letterSpacing: -0.6,
                ),
          ),
        ),
        const SizedBox(width: 8),
        _RoundIconButton(
          tooltip: settings.isDark
              ? l10n.t('Switch to light mode', 'تغییر به حالت روشن')
              : l10n.t('Switch to dark mode', 'تغییر به حالت تاریک'),
          icon: settings.isDark ? Icons.light_mode_outlined : Icons.dark_mode_outlined,
          onPressed: settings.toggleTheme,
        ),
        const SizedBox(width: 4),
        _RoundIconButton(
          tooltip: l10n.t('Settings', 'تنظیمات'),
          icon: Icons.tune_rounded,
          onPressed: onSettings,
        ),
      ],
    );
  }
}

class _RoundIconButton extends StatelessWidget {
  const _RoundIconButton({
    required this.tooltip,
    required this.icon,
    required this.onPressed,
  });

  final String tooltip;
  final IconData icon;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return IconButton(
      tooltip: tooltip,
      onPressed: onPressed,
      style: IconButton.styleFrom(
        backgroundColor: c.card,
        side: BorderSide(color: c.line),
        padding: const EdgeInsets.all(11),
      ),
      icon: Icon(icon, size: 19, color: c.muted),
    );
  }
}

class _HomePage extends StatelessWidget {
  const _HomePage({required this.l10n, required this.onOpen});

  final L10n l10n;
  final ValueChanged<int> onOpen;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _HomeCard(
          icon: Icons.drive_file_rename_outline_rounded,
          title: l10n.t('Derivative', 'مشتق'),
          subtitle: l10n.t(
            'First and second derivatives, with exact symbolic simplification.',
            'مشتق مرتبهٔ اول و دوم، با ساده‌سازی نمادین دقیق.',
          ),
          onTap: () => onOpen(5),
        ),
        const SizedBox(height: 10),
        _HomeCard(
          icon: Icons.integration_instructions_rounded,
          title: l10n.t('Integral', 'انتگرال'),
          subtitle: l10n.t(
            'Single integrals with automatic substitution and integration by parts.',
            'انتگرال یگانه با تشخیص خودکار تغییر متغیر و جزء‌به‌جزء.',
          ),
          onTap: () => onOpen(4),
        ),
        const SizedBox(height: 10),
        _HomeCard(
          icon: Icons.functions_rounded,
          title: l10n.t('Differential equations', 'معادلات دیفرانسیل'),
          subtitle: l10n.t(
            'Exact, homogeneous, linear, Bernoulli, Clairaut, Lagrange, Riccati…',
            'کامل، همگن، خطی، برنولی، کلرو، لاگرانژ، ریکاتی…',
          ),
          onTap: () => onOpen(1),
        ),
        const SizedBox(height: 18),
        Row(
          children: [
            Icon(Icons.menu_rounded, size: 14, color: context.colors.muted),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                l10n.t(
                  'Wronskian and envelope modules are in the menu.',
                  'ماژول‌های رونسکی و پوش در منوی همبرگری هستند.',
                ),
                style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 11),
              ),
            ),
          ],
        ),
      ],
    );
  }
}

class _HomeCard extends StatelessWidget {
  const _HomeCard({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(20),
        child: Container(
          padding: const EdgeInsets.fromLTRB(18, 18, 16, 18),
          decoration: BoxDecoration(
            color: c.card,
            borderRadius: BorderRadius.circular(20),
            border: Border.all(color: c.line),
          ),
          child: Row(
            children: [
              Container(
                width: 46,
                height: 46,
                decoration: BoxDecoration(
                  color: c.accentSoft,
                  borderRadius: BorderRadius.circular(14),
                  border: Border.all(color: c.accent.withValues(alpha: 0.25)),
                ),
                child: Icon(icon, color: c.accent, size: 23),
              ),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      title,
                      style: Theme.of(context).textTheme.titleMedium?.copyWith(
                            fontSize: 16,
                            fontWeight: FontWeight.w700,
                          ),
                    ),
                    const SizedBox(height: 3),
                    Text(
                      subtitle,
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 8),
              Icon(
                l10nDirection(context) == TextDirection.rtl
                    ? Icons.arrow_back_rounded
                    : Icons.arrow_forward_rounded,
                size: 19,
                color: c.muted,
              ),
            ],
          ),
        ),
      ),
    );
  }
}

TextDirection l10nDirection(BuildContext context) =>
    Localizations.localeOf(context).languageCode == 'fa'
        ? TextDirection.rtl
        : TextDirection.ltr;

class _SettingsSheet extends StatelessWidget {
  const _SettingsSheet({
    required this.settings,
    required this.version,
    required this.engineReady,
  });

  final AppSettings settings;
  final String version;
  final bool engineReady;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: settings,
      builder: (context, _) {
        final l10n = L10n(settings.isPersian);
        final c = context.colors;
        return Directionality(
          textDirection: l10n.isFa ? TextDirection.rtl : TextDirection.ltr,
          child: Padding(
            padding: const EdgeInsets.fromLTRB(24, 4, 24, 28),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(l10n.t('Settings', 'تنظیمات'),
                    style: Theme.of(context).textTheme.titleLarge),
                const SizedBox(height: 18),
                Text(l10n.t('Appearance', 'ظاهر'),
                    style: Theme.of(context).textTheme.bodySmall),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: _OptionTile(
                        icon: Icons.dark_mode_rounded,
                        label: l10n.t('Dark', 'تاریک'),
                        selected: settings.isDark,
                        onTap: () => settings.setThemeMode(ThemeMode.dark),
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: _OptionTile(
                        icon: Icons.light_mode_rounded,
                        label: l10n.t('Light', 'روشن'),
                        selected: !settings.isDark,
                        onTap: () => settings.setThemeMode(ThemeMode.light),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 18),
                Text(l10n.t('Language', 'زبان'),
                    style: Theme.of(context).textTheme.bodySmall),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: _OptionTile(
                        icon: Icons.translate_rounded,
                        label: 'English',
                        selected: !settings.isPersian,
                        onTap: () => settings.setLocale(const Locale('en')),
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: _OptionTile(
                        icon: Icons.translate_rounded,
                        label: 'فارسی',
                        selected: settings.isPersian,
                        onTap: () => settings.setLocale(const Locale('fa')),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 20),
                Divider(color: c.line),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Icon(
                      engineReady ? Icons.check_circle_rounded : Icons.error_outline_rounded,
                      size: 17,
                      color: engineReady ? c.success : c.warning,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        engineReady
                            ? l10n.t('Native engine ${version.isEmpty ? '' : 'v$version'} — exact rational arithmetic',
                                'موتور بومی ${version.isEmpty ? '' : 'نسخهٔ $version'} — حساب گویای دقیق')
                            : l10n.t('Native engine unavailable', 'موتور بومی در دسترس نیست'),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
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
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 14),
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
              Text(
                label,
                style: TextStyle(
                  fontSize: 13,
                  fontWeight: FontWeight.w600,
                  color: selected ? c.accent : c.text,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
