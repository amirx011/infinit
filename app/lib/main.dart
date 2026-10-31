import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_math_fork/flutter_math.dart';

import 'src/app_state.dart';
import 'src/l10n.dart';
import 'src/native_engine.dart';
import 'src/pages/continuity_page.dart';
import 'src/pages/derivative_page.dart';
import 'src/pages/envelope_page.dart';
import 'src/pages/integral_page.dart';
import 'src/pages/limit_page.dart';
import 'src/pages/linear_algebra_page.dart';
import 'src/pages/ode_page.dart';
import 'src/pages/wronskian_page.dart';
import 'src/theme.dart';
import 'src/widgets.dart';

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
  String? _odeSeed;
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

  void _open(int index, {String? seed}) {
    setState(() {
      _index = index;
      if (seed != null) _odeSeed = seed;
    });
  }

  Widget _page(L10n l10n) {
    switch (_index) {
      case 1:
        return const LinearAlgebraPage(key: ValueKey('linear-algebra'));
      case 2:
        return OdePage(
          key: ValueKey('ode-${_odeSeed ?? ''}'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
          initialEquation: _odeSeed,
        );
      case 3:
        return WronskianPage(
          key: const ValueKey('wronskian'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 4:
        return EnvelopePage(
          key: const ValueKey('envelope'),
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
      case 6:
        return IntegralPage(
          key: const ValueKey('integral'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 7:
        return LimitPage(
          key: const ValueKey('limit'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 8:
        return ContinuityPage(
          key: const ValueKey('continuity'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      default:
        return _HomePage(
          l10n: l10n,
          engineReady: _engine != null,
          engineError: _engineError,
          onOpen: _open,
        );
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = L10n(widget.settings.isPersian);
    final c = context.colors;
    return Directionality(
      textDirection: l10n.isFa ? TextDirection.rtl : TextDirection.ltr,
      child: PopScope(
        canPop: _index == 0,
        onPopInvokedWithResult: (didPop, _) {
          if (!didPop) setState(() => _index = 0);
        },
        child: Scaffold(
          key: _scaffoldKey,
          drawer: _AppDrawer(
            l10n: l10n,
            selected: _index,
            onSelected: (i) {
              _open(i);
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
                        onHome: _index == 0 ? null : () => setState(() => _index = 0),
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

// ---------------------------------------------------------------- navigation

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
    final equations = [
      (
        index: 2,
        icon: Icons.functions_rounded,
        label: l10n.t('Solve an equation', 'حل معادله'),
        hint: l10n.t('Forms', 'فرم‌ها'),
      ),
      (
        index: 3,
        icon: Icons.grid_on_rounded,
        label: l10n.t('Wronskian', 'رونسکی'),
        hint: null,
      ),
      (
        index: 4,
        icon: Icons.join_inner_rounded,
        label: l10n.t('Envelope & orthogonal', 'پوش و مسیرهای متعامد'),
        hint: null,
      ),
    ];
    final calculus = [
      (
        index: 5,
        icon: Icons.drive_file_rename_outline_rounded,
        label: l10n.t('Derivative', 'مشتق'),
        hint: l10n.t('1st–3rd order', 'مرتبهٔ ۱ تا ۳'),
      ),
      (
        index: 6,
        icon: Icons.integration_instructions_rounded,
        label: l10n.t('Integral', 'انتگرال'),
        hint: l10n.t('1D / 2D / 3D', 'یگانه / دوگانه / سه‌گانه'),
      ),
      (
        index: 7,
        icon: Icons.linear_scale_rounded,
        label: l10n.t('Limit', 'حد'),
        hint: l10n.t('Both sides', 'دوطرفه'),
      ),
      (
        index: 8,
        icon: Icons.timeline_rounded,
        label: l10n.t('Continuity', 'پیوستگی'),
        hint: null,
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
                  const InfinitMark(size: 44),
                  const SizedBox(width: 11),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Infinit Lab',
                            maxLines: 1,
                            softWrap: false,
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
                  _DrawerTile(
                    icon: Icons.home_rounded,
                    label: l10n.t('Home', 'خانه'),
                    selected: selected == 0,
                    onTap: () => onSelected(0),
                  ),
                  _DrawerTile(
                    icon: Icons.view_in_ar_rounded,
                    label: l10n.t('Linear algebra', 'جبر خطی'),
                    hint: l10n.t('Soon', 'به‌زودی'),
                    selected: selected == 1,
                    onTap: () => onSelected(1),
                  ),
                  const SizedBox(height: 8),
                  _DrawerGroup(
                    l10n: l10n,
                    icon: Icons.functions_rounded,
                    title: l10n.t('Differential equations', 'معادلات دیفرانسیل'),
                    items: equations,
                    selected: selected,
                    onSelected: onSelected,
                  ),
                  _DrawerGroup(
                    l10n: l10n,
                    icon: Icons.calculate_rounded,
                    title: l10n.t('Differential & integral', 'حساب دیفرانسیل و انتگرال'),
                    items: calculus,
                    selected: selected,
                    onSelected: onSelected,
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

/// Collapsible drawer section: tapping the header row (or its chevron) reveals
/// the modules inside the same hamburger menu.
class _DrawerGroup extends StatefulWidget {
  const _DrawerGroup({
    required this.l10n,
    required this.icon,
    required this.title,
    required this.items,
    required this.selected,
    required this.onSelected,
  });

  final L10n l10n;
  final IconData icon;
  final String title;
  final List<({int index, IconData icon, String label, String? hint})> items;
  final int selected;
  final ValueChanged<int> onSelected;

  @override
  State<_DrawerGroup> createState() => _DrawerGroupState();
}

class _DrawerGroupState extends State<_DrawerGroup> {
  late bool _open = widget.items.any((i) => i.index == widget.selected);

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final active = widget.items.any((i) => i.index == widget.selected);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Material(
          color: active && !_open ? c.accentSoft : Colors.transparent,
          borderRadius: BorderRadius.circular(12),
          child: InkWell(
            borderRadius: BorderRadius.circular(12),
            onTap: () => setState(() => _open = !_open),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 12),
              child: Row(
                children: [
                  Icon(widget.icon, size: 19, color: active ? c.accent : c.muted),
                  const SizedBox(width: 11),
                  Expanded(
                    child: Text(
                      widget.title,
                      style: TextStyle(
                        fontSize: 13,
                        fontWeight: active ? FontWeight.w700 : FontWeight.w600,
                        color: active ? c.accent : c.text,
                      ),
                    ),
                  ),
                  const SizedBox(width: 6),
                  AnimatedRotation(
                    turns: _open ? 0.5 : 0.0,
                    duration: const Duration(milliseconds: 180),
                    child: Icon(Icons.keyboard_arrow_down_rounded,
                        size: 21, color: active ? c.accent : c.muted),
                  ),
                ],
              ),
            ),
          ),
        ),
        AnimatedCrossFade(
          duration: const Duration(milliseconds: 190),
          crossFadeState:
              _open ? CrossFadeState.showSecond : CrossFadeState.showFirst,
          sizeCurve: Curves.easeOutCubic,
          firstChild: const SizedBox(width: double.infinity, height: 0),
          secondChild: Padding(
            padding: const EdgeInsetsDirectional.only(start: 16, top: 2),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (final item in widget.items)
                  _DrawerTile(
                    icon: item.icon,
                    label: item.label,
                    hint: item.hint,
                    selected: widget.selected == item.index,
                    onTap: () => widget.onSelected(item.index),
                  ),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _DrawerTile extends StatelessWidget {
  const _DrawerTile({
    required this.icon,
    required this.label,
    required this.selected,
    required this.onTap,
    this.hint,
  });

  final IconData icon;
  final String label;
  final String? hint;
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
                  )
                else if (hint != null && hint!.isNotEmpty)
                  Flexible(
                    child: Padding(
                      padding: const EdgeInsetsDirectional.only(start: 6),
                      child: Text(
                        hint!,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        textAlign: TextAlign.end,
                        style: TextStyle(fontSize: 9.5, color: c.muted),
                      ),
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

/// The Infinit Lab mark: a gradient tile with the infinity sign in front of
/// faint calculus and linear-algebra glyphs (d/dx, ∫, matrix brackets, 0/1).
class InfinitMark extends StatelessWidget {
  const InfinitMark({super.key, this.size = 52});

  final double size;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final s = size;
    final radius = BorderRadius.circular(s * 0.30);
    return Container(
      width: s,
      height: s,
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [c.accent, Color.lerp(c.accent, c.glow, 0.55)!],
        ),
        borderRadius: radius,
        boxShadow: [
          BoxShadow(
            color: c.accent.withValues(alpha: 0.26),
            blurRadius: s * 0.42,
            offset: Offset(0, s * 0.17),
          ),
        ],
      ),
      child: ClipRRect(
        borderRadius: radius,
        child: Stack(
          alignment: Alignment.center,
          children: [
            Positioned(
              top: s * 0.05,
              left: s * 0.08,
              child: _glyph('d/dx', s * 0.155, 0.52),
            ),
            Positioned(
              top: s * 0.07,
              right: s * 0.09,
              child: _glyph('Aᵀ', s * 0.14, 0.45),
            ),
            Positioned(
              bottom: s * 0.04,
              left: s * 0.10,
              child: _glyph('∫', s * 0.24, 0.42),
            ),
            Positioned(
              bottom: s * 0.07,
              right: s * 0.08,
              child: _glyph('0 1', s * 0.13, 0.5),
            ),
            Container(
              width: s * 0.78,
              height: s * 0.46,
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.10),
                borderRadius: BorderRadius.circular(s * 0.23),
              ),
            ),
            Icon(
              Icons.all_inclusive_rounded,
              size: s * 0.56,
              color: Colors.white.withValues(alpha: 0.96),
            ),
          ],
        ),
      ),
    );
  }

  Widget _glyph(String text, double fontSize, double alpha) {
    return Text(
      text,
      textDirection: TextDirection.ltr,
      style: TextStyle(
        fontFamily: 'SpaceGrotesk',
        fontSize: fontSize,
        fontWeight: FontWeight.w700,
        color: Colors.white.withValues(alpha: alpha),
        height: 1,
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
    this.onHome,
  });

  final L10n l10n;
  final AppSettings settings;
  final VoidCallback onMenu;
  final VoidCallback onSettings;
  final VoidCallback? onHome;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        _RoundIconButton(
          tooltip: l10n.t('Open menu', 'باز کردن منو'),
          icon: Icons.menu_rounded,
          onPressed: onMenu,
        ),
        const SizedBox(width: 8),
        GestureDetector(
          onTap: onHome,
          child: const InfinitMark(size: 48),
        ),
        const SizedBox(width: 13),
        Expanded(
          child: Align(
            alignment: AlignmentDirectional.centerStart,
            child: FittedBox(
              fit: BoxFit.scaleDown,
              alignment: Alignment.centerLeft,
              child: Text(
                'Infinit Lab',
                maxLines: 1,
                softWrap: false,
                textDirection: TextDirection.ltr,
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                      fontFamily: 'SpaceGrotesk',
                      fontSize: 27,
                      fontWeight: FontWeight.w700,
                      letterSpacing: -0.6,
                    ),
              ),
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

// --------------------------------------------------------------------- home

class _OdeForm {
  const _OdeForm({
    required this.labelEn,
    required this.labelFa,
    required this.standard,
    required this.example,
    required this.order,
  });

  final String labelEn;
  final String labelFa;
  final String standard; // LaTeX
  final String example;
  final int order;
}

const _forms = <_OdeForm>[
  _OdeForm(
    labelEn: 'Exact',
    labelFa: 'کامل',
    standard: r'M\,dx + N\,dy = 0',
    example: '(2x+y) + (x+2y)y\' = 0',
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Homogeneous',
    labelFa: 'همگن',
    standard: r"y' = f\left(\frac{y}{x}\right)",
    example: "y' = (x^2+y^2)/(x*y)",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Linear',
    labelFa: 'خطی',
    standard: r"y' + P(x)\,y = Q(x)",
    example: "y' + y/x = x^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Bernoulli',
    labelFa: 'برنولی',
    standard: r"y' + P(x)\,y = Q(x)\,y^n",
    example: "y' = x*y + y^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Clairaut',
    labelFa: 'کلرو',
    standard: r"y = x\,y' + f(y')",
    example: "y = x*y' + (y')^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Lagrange',
    labelFa: 'لاگرانژ',
    standard: r"y = x\,f(y') + g(y')",
    example: "y = 2*x*y' + (y')^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Riccati',
    labelFa: 'ریکاتی',
    standard: r"y' = P + Qy + Ry^2",
    example: "y' = y^2 - 2/x^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Separable',
    labelFa: 'جداشدنی',
    standard: r"y' = f(x)\,g(y)",
    example: "y' = y^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Linear substitution',
    labelFa: 'جایگذاری خطی',
    standard: r"y' = f(ax+by+c)",
    example: "y' = (x+y)^2",
    order: 1,
  ),
  _OdeForm(
    labelEn: 'Constant coefficients',
    labelFa: 'ضرایب ثابت',
    standard: r"y'' + p\,y' + q\,y = f(x)",
    example: "y'' - 3y' + 2y = 0",
    order: 2,
  ),
  _OdeForm(
    labelEn: 'Direct integration',
    labelFa: 'انتگرال‌گیری مستقیم',
    standard: r"y'' = f(x)",
    example: "y'' = 6x",
    order: 2,
  ),
  _OdeForm(
    labelEn: 'Oscillator',
    labelFa: 'نوسانگر',
    standard: r"y'' + \omega^2 y = 0",
    example: "y'' + y = 0",
    order: 2,
  ),
];

class _HomePage extends StatefulWidget {
  const _HomePage({
    required this.l10n,
    required this.engineReady,
    required this.onOpen,
    this.engineError,
  });

  final L10n l10n;
  final bool engineReady;
  final String? engineError;
  final void Function(int index, {String? seed}) onOpen;

  @override
  State<_HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<_HomePage> {
  int _order = 1;

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    final forms = _forms.where((f) => f.order == _order).toList();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (!widget.engineReady) ...[
          _engineBanner(context, l),
          const SizedBox(height: 14),
        ],
        _hero(context, l),
        const SizedBox(height: 26),
        LayoutBuilder(
          builder: (context, constraints) {
            final label = SectionLabel(l.t('Forms we recognise', 'فرم‌هایی که می‌شناسیم'));
            final switcher = SegmentedButton<int>(
              showSelectedIcon: false,
              style: const ButtonStyle(
                visualDensity: VisualDensity.compact,
                tapTargetSize: MaterialTapTargetSize.shrinkWrap,
              ),
              segments: [
                ButtonSegment(
                  value: 1,
                  label: Text(l.t('First order', 'مرتبهٔ اول')),
                ),
                ButtonSegment(
                  value: 2,
                  label: Text(l.t('Second order', 'مرتبهٔ دوم')),
                ),
              ],
              selected: {_order},
              onSelectionChanged: (v) => setState(() => _order = v.first),
            );
            if (constraints.maxWidth < 560) {
              return Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  label,
                  const SizedBox(height: 10),
                  FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: AlignmentDirectional.centerStart,
                    child: switcher,
                  ),
                ],
              );
            }
            return Row(
              children: [
                Expanded(child: label),
                switcher,
              ],
            );
          },
        ),
        const SizedBox(height: 12),
        LayoutBuilder(
          builder: (context, constraints) {
            final width = constraints.maxWidth;
            final columns = width >= 940 ? 3 : (width >= 620 ? 2 : 1);
            const gap = 10.0;
            final itemWidth = (width - gap * (columns - 1)) / columns;
            return Wrap(
              spacing: gap,
              runSpacing: gap,
              children: [
                for (final form in forms)
                  SizedBox(
                    width: itemWidth,
                    child: _FormCard(
                      label: l.isFa ? form.labelFa : form.labelEn,
                      latex: form.standard,
                      onTap: () => widget.onOpen(2, seed: form.example),
                    ),
                  ),
              ],
            );
          },
        ),
        const SizedBox(height: 28),
        SectionLabel(l.t('Beyond solving', 'فراتر از حل')),
        const SizedBox(height: 12),
        LayoutBuilder(
          builder: (context, constraints) {
            final wide = constraints.maxWidth >= 620;
            final cards = [
              _WideCard(
                icon: Icons.grid_on_rounded,
                title: l.t('Wronskian', 'رونسکی'),
                subtitle: l.t('Linear independence of a set of functions.',
                    'استقلال خطی یک مجموعه تابع.'),
                onTap: () => widget.onOpen(3),
              ),
              _WideCard(
                icon: Icons.join_inner_rounded,
                title: l.t('Envelope & orthogonal', 'پوش و مسیرهای متعامد'),
                subtitle: l.t('Envelopes of a family and orthogonal trajectories.',
                    'پوش یک خانوادهٔ منحنی و مسیرهای متعامد.'),
                onTap: () => widget.onOpen(4),
              ),
            ];
            if (!wide) {
              return Column(
                children: [
                  for (var i = 0; i < cards.length; i++) ...[
                    if (i > 0) const SizedBox(height: 10),
                    cards[i],
                  ],
                ],
              );
            }
            return IntrinsicHeight(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(child: cards[0]),
                  const SizedBox(width: 10),
                  Expanded(child: cards[1]),
                ],
              ),
            );
          },
        ),
        const SizedBox(height: 26),
        _calculusRow(context, l),
        const SizedBox(height: 30),
        Center(
          child: Text(
            l.t('See the steps, not just the answer.', 'هر قدم را ببین، نه فقط جواب را.'),
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontSize: 11.5,
                  letterSpacing: 0.2,
                  color: c.muted.withValues(alpha: 0.85),
                ),
          ),
        ),
      ],
    );
  }

  Widget _engineBanner(BuildContext context, L10n l) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: c.warningSoft,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: c.warning.withValues(alpha: 0.35)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.warning_amber_rounded, size: 19, color: c.warning),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              l.t(
                'The native engine is not loaded, so computations will fail. Check libodecore.so next to the executable.',
                'موتور بومی بارگذاری نشده است؛ محاسبات انجام نمی‌شوند. فایل libodecore.so را کنار فایل اجرایی بررسی کنید.',
              ),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
        ],
      ),
    );
  }

  Widget _hero(BuildContext context, L10n l) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 26, 24, 24),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: AlignmentDirectional.topStart,
          end: AlignmentDirectional.bottomEnd,
          colors: [
            c.accent.withValues(alpha: 0.16),
            c.card,
            c.card,
          ],
        ),
        borderRadius: BorderRadius.circular(26),
        border: Border.all(color: c.accent.withValues(alpha: 0.22)),
      ),
      child: Stack(
        children: [
          PositionedDirectional(
            end: -18,
            bottom: -34,
            child: Text(
              '∞',
              textDirection: TextDirection.ltr,
              style: TextStyle(
                fontSize: 130,
                height: 1,
                fontFamily: 'SpaceGrotesk',
                color: c.accent.withValues(alpha: 0.10),
              ),
            ),
          ),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                l.t('Every equation has a form.', 'هر معادله‌ای یک فرم دارد.'),
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                      fontSize: 26,
                      fontWeight: FontWeight.w700,
                      height: 1.25,
                      letterSpacing: -0.4,
                    ),
              ),
              const SizedBox(height: 8),
              ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 560),
                child: Text(
                  l.t(
                    'Enter an ordinary differential equation; Infinit Lab recognises its form, shows the standard shape and solves it step by step.',
                    'یک معادلهٔ دیفرانسیل معمولی وارد کنید؛ Infinit Lab فرم آن را تشخیص می‌دهد، شکل استاندارد را نشان می‌دهد و قدم‌به‌قدم حل می‌کند.',
                  ),
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.6),
                ),
              ),
              const SizedBox(height: 18),
              _HeroMath(),
              const SizedBox(height: 20),
              FilledButton.icon(
                onPressed: () => widget.onOpen(2, seed: "y' + y/x = x^2"),
                icon: const Icon(Icons.play_arrow_rounded, size: 20),
                label: Text(l.t('Solve an equation', 'حل یک معادله')),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _calculusRow(BuildContext context, L10n l) {
    final c = context.colors;
    final items = [
      (icon: Icons.drive_file_rename_outline_rounded, label: l.t('Derivative', 'مشتق'), index: 5),
      (icon: Icons.integration_instructions_rounded, label: l.t('Integral', 'انتگرال'), index: 6),
      (icon: Icons.linear_scale_rounded, label: l.t('Limit', 'حد'), index: 7),
      (icon: Icons.timeline_rounded, label: l.t('Continuity', 'پیوستگی'), index: 8),
    ];
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        Text(
          l.t('Calculus tools', 'ابزارهای حساب دیفرانسیل'),
          style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 11),
        ),
        for (final item in items)
          TextButton.icon(
            onPressed: () => widget.onOpen(item.index),
            style: TextButton.styleFrom(
              foregroundColor: c.text,
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              minimumSize: const Size(0, 40),
              textStyle: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
            ),
            icon: Icon(item.icon, size: 15, color: c.muted),
            label: Text(item.label),
          ),
      ],
    );
  }
}

class _HeroMath extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: c.cardAlt.withValues(alpha: 0.75),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: c.line),
      ),
      child: Directionality(
        textDirection: TextDirection.ltr,
        child: SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: Math.tex(
            r"y' + P(x)\,y = Q(x) \quad\Longrightarrow\quad y = e^{-\int P\,dx}\left(\int Q\,e^{\int P\,dx}dx + C\right)",
            mathStyle: MathStyle.display,
            textStyle: TextStyle(fontSize: 16, color: c.text.withValues(alpha: 0.92)),
            onErrorFallback: (error) => Text(
              "y' + P(x)y = Q(x)",
              style: TextStyle(fontFamily: 'DejaVu Sans Mono', fontSize: 15, color: c.text),
            ),
          ),
        ),
      ),
    );
  }
}

class _FormCard extends StatelessWidget {
  const _FormCard({required this.label, required this.latex, required this.onTap});

  final String label;
  final String latex;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(16),
        child: Container(
          padding: const EdgeInsets.fromLTRB(14, 13, 14, 13),
          decoration: BoxDecoration(
            color: c.card,
            borderRadius: BorderRadius.circular(16),
            border: Border.all(color: c.line),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                children: [
                  Expanded(
                    child: Text(
                      label,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: TextStyle(
                        fontSize: 12.5,
                        fontWeight: FontWeight.w700,
                        color: c.text,
                      ),
                    ),
                  ),
                  Icon(
                    l10nDirection(context) == TextDirection.rtl
                        ? Icons.arrow_back_rounded
                        : Icons.arrow_forward_rounded,
                    size: 15,
                    color: c.muted,
                  ),
                ],
              ),
              const SizedBox(height: 6),
              Directionality(
                textDirection: TextDirection.ltr,
                child: SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: Math.tex(
                    latex,
                    textStyle: TextStyle(fontSize: 13.5, color: c.muted),
                    onErrorFallback: (error) => Text(latex,
                        style: TextStyle(
                            fontFamily: 'DejaVu Sans Mono', fontSize: 12, color: c.muted)),
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

class _WideCard extends StatelessWidget {
  const _WideCard({
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
        borderRadius: BorderRadius.circular(18),
        child: Container(
          padding: const EdgeInsets.fromLTRB(16, 15, 14, 15),
          decoration: BoxDecoration(
            color: c.card,
            borderRadius: BorderRadius.circular(18),
            border: Border.all(color: c.line),
          ),
          child: Row(
            children: [
              Container(
                width: 38,
                height: 38,
                decoration: BoxDecoration(
                  color: c.accentSoft,
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: c.accent.withValues(alpha: 0.22)),
                ),
                child: Icon(icon, color: c.accent, size: 20),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(title,
                        style: Theme.of(context)
                            .textTheme
                            .titleMedium
                            ?.copyWith(fontSize: 14.5, fontWeight: FontWeight.w700)),
                    const SizedBox(height: 2),
                    Text(subtitle,
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 11.5)),
                  ],
                ),
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
