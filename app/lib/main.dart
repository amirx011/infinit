import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_math_fork/flutter_math.dart';

import 'src/app_state.dart';
import 'src/l10n.dart';
import 'src/native_engine.dart';
import 'src/pages/behaviour_page.dart';
import 'src/pages/continuity_page.dart';
import 'src/pages/educational_page.dart';
import 'src/pages/optimize_page.dart';
import 'src/pages/related_rates_page.dart';
import 'src/pages/tangent_page.dart';
import 'src/pages/theorems_page.dart';
import 'src/pages/derivative_page.dart';
import 'src/pages/domain_range_page.dart';
import 'src/pages/envelope_page.dart';
import 'src/pages/integral_page.dart';
import 'src/pages/limit_page.dart';
import 'src/pages/linear_algebra_page.dart';
import 'src/pages/ode_page.dart';
import 'src/pages/settings_page.dart';
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
          builder: (context, child) {
            final media = MediaQuery.of(context);
            return MediaQuery(
              data: media.copyWith(
                textScaler: TextScaler.linear(settings.textScale),
                disableAnimations: settings.reduceMotion,
              ),
              child: child ?? const SizedBox.shrink(),
            );
          },
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
      case 9:
        return DomainRangePage(
          key: const ValueKey('domain-range'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 10:
        return BehaviourPage(
          key: const ValueKey('behaviour'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 11:
        return TangentPage(
          key: const ValueKey('tangent'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 12:
        return OptimizePage(
          key: const ValueKey('optimize'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 13:
        return RelatedRatesPage(
          key: const ValueKey('related-rates'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 14:
        return TheoremsPage(
          key: const ValueKey('lhopital'),
          mode: TheoremMode.lhopital,
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 15:
        return TheoremsPage(
          key: const ValueKey('mvt'),
          mode: TheoremMode.mvt,
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 16:
        return TheoremsPage(
          key: const ValueKey('rolle'),
          mode: TheoremMode.rolle,
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 17:
        return TheoremsPage(
          key: const ValueKey('cauchy'),
          mode: TheoremMode.cauchy,
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 18:
        return TheoremsPage(
          key: const ValueKey('theorem-catalogue'),
          mode: TheoremMode.catalogue,
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 19:
        return EducationalPage(key: const ValueKey('educational'), l10n: l10n);
      case 20:
        return EnvelopePage(
          key: const ValueKey('orthogonal'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
          orthogonalOnly: true,
        );
      case 21:
        return EnvelopePage(
          key: const ValueKey('oblique'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
          obliqueOnly: true,
        );
      case 22:
        return SettingsPage(
          key: const ValueKey('settings'),
          settings: widget.settings,
          l10n: l10n,
          version: _version,
          engineReady: _engine != null,
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
                        onMenu: () => _scaffoldKey.currentState?.openDrawer(),
                        onSettings: () => _open(22),
                      ),
                      const SizedBox(height: 26),
                      AnimatedSwitcher(
                        duration: widget.settings.reduceMotion
                            ? Duration.zero
                            : const Duration(milliseconds: 240),
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

}

// ---------------------------------------------------------------- navigation

/// One row of the drawer: either a link to a page or a collapsible group.
class _DrawerEntry {
  const _DrawerEntry.link({
    required this.index,
    required this.icon,
    required this.label,
    this.hint,
  }) : children = const [];

  const _DrawerEntry.group({
    required this.icon,
    required this.label,
    required this.children,
  }) : index = null,
       hint = null;

  /// Destination page, null for a group.
  final int? index;
  final IconData icon;
  final String label;
  final String? hint;
  final List<_DrawerEntry> children;

  bool get isGroup => index == null;
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
    final sections = <_DrawerEntry>[
      _DrawerEntry.link(
        index: 0,
        icon: Icons.home_rounded,
        label: l10n.t('Home', 'خانه'),
      ),
      _DrawerEntry.link(
        index: 1,
        icon: Icons.view_in_ar_rounded,
        label: l10n.t('Linear algebra', 'جبر خطی'),
        hint: l10n.t('Soon', 'به‌زودی'),
      ),
      
      _DrawerEntry.group(
        icon: Icons.functions_rounded,
        label: l10n.t('Differential equations', 'معادلات دیفرانسیل'),
        children: [
          _DrawerEntry.link(
            index: 2,
            icon: Icons.functions_rounded,
            label: l10n.t('Solve an equation', 'حل معادله'),
            hint: l10n.t('Forms', 'فرم‌ها'),
          ),
          _DrawerEntry.link(
            index: 3,
            icon: Icons.grid_on_rounded,
            label: l10n.t('Wronskian', 'رونسکی'),
          ),
          _DrawerEntry.link(
            index: 4,
            icon: Icons.join_inner_rounded,
            label: l10n.t('Envelope', 'پوش خانواده'),
          ),
          _DrawerEntry.link(
            index: 20,
            icon: Icons.swap_calls_rounded,
            label: l10n.t('Orthogonal trajectories', 'مسیرهای قائم'),
            hint: l10n.t('Right angles', 'زاویهٔ قائمه'),
          ),
          _DrawerEntry.link(
            index: 21,
            icon: Icons.straighten_rounded,
            label: l10n.t('Oblique trajectories', 'مسیرهای مایل'),
            hint: l10n.t('Constant angle', 'زاویهٔ ثابت'),
          ),
        ],
      ),
      _DrawerEntry.group(
        icon: Icons.calculate_rounded,
        label: l10n.t(
          'Differential & integral',
          'حساب دیفرانسیل و انتگرال',
        ),
        children: [
          _DrawerEntry.link(
            index: 5,
            icon: Icons.drive_file_rename_outline_rounded,
            label: l10n.t('Derivative', 'مشتق'),
            hint: l10n.t('1st–10th order', 'مرتبهٔ ۱ تا ۱۰'),
          ),
          _DrawerEntry.link(
            index: 6,
            icon: Icons.integration_instructions_rounded,
            label: l10n.t('Integral', 'انتگرال'),
            hint: l10n.t('1D / 2D / 3D', 'یگانه / دوگانه / سه‌گانه'),
          ),
          _DrawerEntry.link(
            index: 7,
            icon: Icons.linear_scale_rounded,
            label: l10n.t('Limit', 'حد'),
            hint: l10n.t('Both sides', 'دوطرفه'),
          ),
          _DrawerEntry.link(
            index: 8,
            icon: Icons.timeline_rounded,
            label: l10n.t('Continuity', 'پیوستگی'),
          ),
          _DrawerEntry.link(
            index: 9,
            icon: Icons.explore_rounded,
            label: l10n.t('Domain & range', 'دامنه و برد'),
            hint: l10n.t('Plot', 'نمودار'),
          ),
        _DrawerEntry.group(
          icon: Icons.insights_rounded,
          label: l10n.t('Applications of the derivative', 'کاربردهای مشتق'),
          children: [
            _DrawerEntry.link(
              index: 10,
              icon: Icons.insights_rounded,
              label: l10n.t('Function behaviour', 'تحلیل رفتار تابع'),
            ),
            _DrawerEntry.link(
              index: 11,
              icon: Icons.show_chart_rounded,
              label: l10n.t('Tangent & normal', 'مماس و خط عمود'),
            ),
            _DrawerEntry.link(
              index: 12,
              icon: Icons.vertical_align_bottom_rounded,
              label: l10n.t('Optimisation', 'مسایل بهینه‌سازی'),
            ),
            _DrawerEntry.link(
              index: 13,
              icon: Icons.speed_rounded,
              label: l10n.t('Related rates', 'نرخ‌های مرتبط'),
            ),
            _DrawerEntry.group(
              icon: Icons.rule_rounded,
              label: l10n.t('Important theorems', 'قضایای مهم'),
              children: [
                _DrawerEntry.link(
                  index: 14,
                  icon: Icons.compare_arrows_rounded,
                  label: l10n.t("L'Hôpital's rule", 'قاعدهٔ هوپیتال'),
                ),
                _DrawerEntry.link(
                  index: 15,
                  icon: Icons.rule_rounded,
                  label: l10n.t('Mean value theorem', 'قضیهٔ مقدار میانگین'),
                ),
                _DrawerEntry.link(
                  index: 16,
                  icon: Icons.horizontal_rule_rounded,
                  label: l10n.t("Rolle's theorem", 'قضیهٔ رول'),
                ),
                _DrawerEntry.link(
                  index: 17,
                  icon: Icons.swap_horiz_rounded,
                  label: l10n.t("Cauchy's theorem", 'قضیهٔ کوشی'),
                ),
                _DrawerEntry.link(
                  index: 18,
                  icon: Icons.menu_book_rounded,
                  label: l10n.t(
                    'Derivative theorems, complete',
                    'قضایای مشتق، کامل',
                  ),
                ),
              ],
            ),
          ],
        ),
        ],
      ),
      _DrawerEntry.group(
        icon: Icons.auto_stories_rounded,
        label: l10n.t('Educational explanations', 'توضیحات آموزشی'),
        children: const [],
      ),
      _DrawerEntry.link(
        index: 22,
        icon: Icons.settings_rounded,
        label: l10n.t('Settings', 'تنظیمات'),
        hint: l10n.t('Language & theme', 'زبان و پوسته'),
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
                  const InfinitMark(size: 34),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Infinit Lab',
                          maxLines: 1,
                          softWrap: false,
                          style: Theme.of(context).textTheme.titleMedium
                              ?.copyWith(
                                fontFamily: 'SpaceGrotesk',
                                fontSize: 17,
                                fontWeight: FontWeight.w700,
                                letterSpacing: -0.2,
                              ),
                        ),
                        Text(
                          l10n.t('Modules', 'ماژول‌ها'),
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
            Divider(color: c.line, height: 1),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.symmetric(
                  horizontal: 10,
                  vertical: 12,
                ),
                children: [
                  for (var i = 0; i < sections.length; i++) ...[
                    if (i > 0 && sections[i].isGroup) const SizedBox(height: 4),
                    _DrawerNode(
                      entry: sections[i],
                      l10n: l10n,
                      selected: selected,
                      onSelected: onSelected,
                      depth: 0,
                    ),
                  ],
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// Renders either a single drawer row or a collapsible group, at any depth.
class _DrawerNode extends StatelessWidget {
  const _DrawerNode({
    required this.entry,
    required this.l10n,
    required this.selected,
    required this.onSelected,
    required this.depth,
  });

  final _DrawerEntry entry;
  final L10n l10n;
  final int selected;
  final ValueChanged<int> onSelected;
  final int depth;

  @override
  Widget build(BuildContext context) {
    if (!entry.isGroup) {
      return _DrawerTile(
        icon: entry.icon,
        label: entry.label,
        hint: entry.hint,
        selected: selected == entry.index,
        onTap: () => onSelected(entry.index!),
      );
    }
    return _DrawerGroupNode(
      entry: entry,
      l10n: l10n,
      selected: selected,
      onSelected: onSelected,
      depth: depth,
    );
  }
}

/// Collapsible drawer section. A group with no children expands to a short
/// honest note instead of a dead link, which is what the empty educational
/// section needs.
class _DrawerGroupNode extends StatefulWidget {
  const _DrawerGroupNode({
    required this.entry,
    required this.l10n,
    required this.selected,
    required this.onSelected,
    required this.depth,
  });

  final _DrawerEntry entry;
  final L10n l10n;
  final int selected;
  final ValueChanged<int> onSelected;
  final int depth;

  @override
  State<_DrawerGroupNode> createState() => _DrawerGroupNodeState();
}

class _DrawerGroupNodeState extends State<_DrawerGroupNode> {
  late bool _open = _containsSelected();

  bool _containsSelected() {
    bool search(List<_DrawerEntry> items) {
      for (final item in items) {
        if (item.index == widget.selected) return true;
        if (item.children.isNotEmpty && search(item.children)) return true;
      }
      return false;
    }

    return search(widget.entry.children);
  }

  bool get _active => _containsSelected();

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final empty = widget.entry.children.isEmpty;
    final titleSize = widget.depth == 0 ? 13.0 : 12.5;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Material(
          color: _active && !_open ? c.accentSoft : Colors.transparent,
          borderRadius: BorderRadius.circular(12),
          child: InkWell(
            borderRadius: BorderRadius.circular(12),
            onTap: () => setState(() => _open = !_open),
            child: Padding(
              padding: EdgeInsets.symmetric(
                horizontal: 13,
                vertical: widget.depth == 0 ? 12 : 10,
              ),
              child: Row(
                children: [
                  Icon(
                    widget.entry.icon,
                    size: widget.depth == 0 ? 19 : 17,
                    color: _active ? c.accent : c.muted,
                  ),
                  const SizedBox(width: 11),
                  Expanded(
                    child: Text(
                      widget.entry.label,
                      style: TextStyle(
                        fontFamily: 'Vazirmatn',
                        fontSize: titleSize,
                        fontWeight: _active ? FontWeight.w700 : FontWeight.w600,
                        color: _active ? c.accent : c.text,
                      ),
                    ),
                  ),
                  const SizedBox(width: 6),
                  AnimatedRotation(
                    turns: _open ? 0.5 : 0.0,
                    duration: const Duration(milliseconds: 180),
                    child: Icon(
                      Icons.keyboard_arrow_down_rounded,
                      size: 21,
                      color: _active ? c.accent : c.muted,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        AnimatedCrossFade(
          duration: const Duration(milliseconds: 190),
          crossFadeState: _open
              ? CrossFadeState.showSecond
              : CrossFadeState.showFirst,
          sizeCurve: Curves.easeOutCubic,
          firstChild: const SizedBox(width: double.infinity, height: 0),
          secondChild: Padding(
            padding: EdgeInsetsDirectional.only(
              start: widget.depth == 0 ? 16 : 12,
              top: 2,
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                if (empty)
                  Padding(
                    padding: const EdgeInsets.fromLTRB(13, 6, 13, 10),
                    child: Text(
                      widget.l10n.t(
                        'Nothing has been added to this section yet.',
                        'هنوز مطلبی به این بخش اضافه نشده است.',
                      ),
                      style: TextStyle(
                        fontFamily: 'Vazirmatn',
                        fontSize: 12,
                        height: 1.7,
                        color: c.muted,
                      ),
                    ),
                  ),
                for (final child in widget.entry.children)
                  _DrawerNode(
                    entry: child,
                    l10n: widget.l10n,
                    selected: widget.selected,
                    onSelected: widget.onSelected,
                    depth: widget.depth + 1,
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
                      fontFamily: 'Vazirmatn',
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
                    decoration: BoxDecoration(
                      color: c.accent,
                      shape: BoxShape.circle,
                    ),
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
                        style: TextStyle(
                          fontFamily: 'Vazirmatn',
                          fontSize: 12,
                          color: c.muted,
                        ),
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


/// The Infinit Lab logo: a slightly tilted infinity with 0 in the left loop and
/// 1 in the right one, drawn with the same geometry as the packaged app icons.
class InfinitMark extends StatelessWidget {
  const InfinitMark({super.key, this.size = 42});

  /// Height of the mark; the widget is a little wider than it is tall.
  final double size;

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    // The 0/1 counters are only a few pixels tall on the header mark. Drawing
    // them there just adds noise, so keep them for hero-sized marks.
    final showDigits = size >= 56;
    return SizedBox(
      width: size * 1.55,
      height: size,
      child: CustomPaint(
        painter: _InfinitMarkPainter(
          glow: dark && showDigits ? 0.18 : 0,
          showDigits: showDigits,
        ),
      ),
    );
  }
}

class _InfinitMarkPainter extends CustomPainter {
  const _InfinitMarkPainter({required this.glow, required this.showDigits});

  final double glow;
  final bool showDigits;

  static const _gradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [Color(0xFF38BDF8), Color(0xFF3B82F6), Color(0xFF4F46E5)],
  );

  @override
  void paint(Canvas canvas, Size size) {
    final w = size.width;
    final h = size.height;
    final a = w * 0.395;
    final stroke = w * 0.070;
    const yScale = 1.30;
    final tilt = -8 * math.pi / 180;
    final center = Offset(w / 2, h / 2);

    final path = Path();
    const steps = 240;
    for (var i = 0; i <= steps; i++) {
      final t = 2 * math.pi * i / steps;
      final d = 1 + math.pow(math.sin(t), 2).toDouble();
      final x = a * math.cos(t) / d;
      final y = a * math.sin(t) * math.cos(t) / d * yScale;
      final px = center.dx + x * math.cos(tilt) - y * math.sin(tilt);
      final py = center.dy + x * math.sin(tilt) + y * math.cos(tilt);
      if (i == 0) {
        path.moveTo(px, py);
      } else {
        path.lineTo(px, py);
      }
    }

    final shader = _gradient.createShader(Rect.fromLTWH(0, 0, w, h));

    if (glow > 0) {
      canvas.drawPath(
        path,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = stroke * 1.7
          ..strokeCap = StrokeCap.round
          ..strokeJoin = StrokeJoin.round
          ..color = const Color(0xFF3B82F6).withValues(alpha: glow)
          ..maskFilter = MaskFilter.blur(BlurStyle.normal, stroke * 0.8),
      );
    }

    canvas.drawPath(
      path,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = stroke
        ..strokeCap = StrokeCap.round
        ..strokeJoin = StrokeJoin.round
        ..shader = shader,
    );

    if (showDigits) {
      final fontSize = w * 0.140;
      for (final entry in const [('0', -1.0), ('1', 1.0)]) {
        final painter = TextPainter(
          text: TextSpan(
            text: entry.$1,
            style: TextStyle(
              fontFamily: 'SpaceGrotesk',
              fontSize: fontSize,
              fontWeight: FontWeight.w700,
              height: 1,
              foreground: Paint()..shader = shader,
            ),
          ),
          textDirection: TextDirection.ltr,
        )..layout();
        final dx = entry.$2 * 0.43 * a;
        final px = center.dx + dx * math.cos(tilt);
        final py = center.dy + dx * math.sin(tilt);
        painter.paint(
          canvas,
          Offset(px - painter.width / 2, py - painter.height / 2),
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _InfinitMarkPainter oldDelegate) =>
      oldDelegate.glow != glow || oldDelegate.showDigits != showDigits;
}

class _Header extends StatelessWidget {
  const _Header({
    required this.l10n,
    required this.onMenu,
    required this.onSettings,
  });

  final L10n l10n;
  final VoidCallback onMenu;
  final VoidCallback onSettings;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        _RoundIconButton(
          tooltip: l10n.t('Open menu', 'باز کردن منو'),
          icon: Icons.menu_rounded,
          onPressed: onMenu,
        ),
        const SizedBox(width: 12),
        const InfinitMark(size: 40),
        const SizedBox(width: 12),
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
    standard: r"y = x\,g(y') + f(y')",
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
        const SizedBox(height: 14),
        LearningCard(
          l10n: l,
          title: l.t('How to read a solution', 'چگونه یک جواب را بخوانیم'),
          idea: l.t(
            'Every answer comes in three layers: the closed form, the solution path, and the validation. Read them together instead of stopping at the result.',
            'هر جواب سه لایه دارد: شکل بسته، مسیر حل و اعتبارسنجی. این سه را با هم بخوانید، نه فقط نتیجه را.',
          ),
          expandLabel: l.t('Read the workflow', 'روند کار را ببینید'),
          points: [
            l.t(
              'The answer card states what was solved and any assumptions.',
              'کارت جواب می‌گوید چه چیزی حل شده و چه فرض‌هایی در کار بوده است.',
            ),
            l.t(
              'The solution path names each rule, so you can reproduce it by hand.',
              'مسیر حل نام هر قاعده را می‌آورد تا بتوانید آن را دستی بازتولید کنید.',
            ),
            l.t(
              'Validation substitutes the answer back; a failed check means the form needs another look.',
              'اعتبارسنجی جواب را جایگذاری می‌کند؛ اگر بررسی رد شود، فرم معادله را دوباره ببینید.',
            ),
          ],
        ),
        const SizedBox(height: 26),
        LayoutBuilder(
          builder: (context, constraints) {
            final label = SectionLabel(
              l.t('Forms we recognise', 'فرم‌هایی که می‌شناسیم'),
            );
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
                subtitle: l.t(
                  'Linear independence of a set of functions.',
                  'استقلال خطی یک مجموعه تابع.',
                ),
                onTap: () => widget.onOpen(3),
              ),
              _WideCard(
                icon: Icons.join_inner_rounded,
                title: l.t('Envelope & trajectories', 'پوش و مسیرهای قائم'),
                subtitle: l.t(
                  'Envelopes of a family, orthogonal trajectories, and oblique trajectories.',
                  'پوش یک خانوادهٔ منحنی، مسیرهای قائم و مسیرهای مایل.',
                ),
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
            l.t(
              'See the steps, not just the answer.',
              'هر قدم را ببین، نه فقط جواب را.',
            ),
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(fontSize: 12, letterSpacing: 0.2, color: c.muted),
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
                'The native engine could not be loaded, so computations will fail. '
                    'On Android the APK must contain libodecore.so; on desktop check that '
                    'it sits next to the executable.',
                'موتور بومی بارگذاری نشد؛ محاسبات انجام نمی‌شوند. در اندروید فایل '
                    'libodecore.so باید داخل خود APK باشد؛ در دسکتاپ آن را کنار فایل اجرایی بررسی کنید.',
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
          colors: [c.accent.withValues(alpha: 0.16), c.card, c.card],
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
                  style: Theme.of(context).textTheme.bodySmall
                      ?.copyWith(height: 1.6),
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
      (
        icon: Icons.drive_file_rename_outline_rounded,
        label: l.t('Derivative', 'مشتق'),
        index: 5,
      ),
      (
        icon: Icons.integration_instructions_rounded,
        label: l.t('Integral', 'انتگرال'),
        index: 6,
      ),
      (icon: Icons.linear_scale_rounded, label: l.t('Limit', 'حد'), index: 7),
      (
        icon: Icons.timeline_rounded,
        label: l.t('Continuity', 'پیوستگی'),
        index: 8,
      ),
      (
        icon: Icons.explore_rounded,
        label: l.t('Domain & range', 'دامنه و برد'),
        index: 9,
      ),
    ];
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        Text(
          l.t('Calculus tools', 'ابزارهای حساب دیفرانسیل'),
          style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 12),
        ),
        for (final item in items)
          TextButton.icon(
            onPressed: () => widget.onOpen(item.index),
            style: TextButton.styleFrom(
              foregroundColor: c.text,
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              minimumSize: const Size(0, 40),
              textStyle: const TextStyle(
                fontFamily: 'Vazirmatn',
                fontSize: 12,
                fontWeight: FontWeight.w600,
              ),
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
            textStyle: TextStyle(
              fontFamily: 'Vazirmatn',
              fontSize: 16,
              color: c.text.withValues(alpha: 0.92),
            ),
            onErrorFallback: (error) => Text(
              "y' + P(x)y = Q(x)",
              style: TextStyle(
                fontFamily: 'DejaVu Sans Mono',
                fontSize: 15,
                color: c.text,
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _FormCard extends StatelessWidget {
  const _FormCard({
    required this.label,
    required this.latex,
    required this.onTap,
  });

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
                        fontFamily: 'Vazirmatn',
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
                    onErrorFallback: (error) => Text(
                      latex,
                      style: TextStyle(
                        fontFamily: 'DejaVu Sans Mono',
                        fontSize: 12,
                        color: c.muted,
                      ),
                    ),
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
                    Text(
                      title,
                      style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontSize: 14.5,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                    const SizedBox(height: 2),
                    Text(
                      subtitle,
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                      style: Theme.of(context).textTheme.bodySmall
                          ?.copyWith(fontSize: 12),
                    ),
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
