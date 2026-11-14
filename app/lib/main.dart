import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_math_fork/flutter_math.dart';

import 'src/app_state.dart';
import 'src/curriculum_articles.dart';
import 'src/i18n/catalog.dart';
import 'src/l10n.dart';
import 'src/native_engine.dart';
import 'src/pages/behaviour_page.dart';
import 'src/pages/coming_soon_page.dart';
import 'src/pages/continuity_page.dart';
import 'src/pages/curriculum_article_page.dart';
import 'src/pages/educational_page.dart';
import 'src/pages/optimize_page.dart';
import 'src/pages/related_rates_page.dart';
import 'src/pages/tangent_page.dart';
import 'src/pages/theorems_page.dart';
import 'src/pages/topic_workbench_page.dart';
import 'src/topic_catalog.dart';
import 'src/pages/derivative_page.dart';
import 'src/pages/domain_range_page.dart';
import 'src/pages/envelope_page.dart';
import 'src/pages/integral_page.dart';
import 'src/pages/limit_page.dart';
import 'src/pages/linear_algebra_page.dart';
import 'src/pages/ode_page.dart';
import 'src/pages/settings_page.dart';
import 'src/pages/wronskian_page.dart';
import 'src/pages/taylor_page.dart';
import 'src/pages/integral_apps_page.dart';
import 'src/pages/partial_page.dart';
import 'src/pages/series_page.dart';
import 'src/theme.dart';
import 'src/widgets.dart';

/// Highest index in the navigation tree. Indices 27–60 are the
/// differential-equations curriculum slots, 61–64 are the four study
/// articles, 65 is the Heun workbench and 66–115 are the curriculum
/// placeholders added by the textbook traceability pass.
const int _lastPageIndex = 115;

/// Curriculum slots that still render the shared coming-soon page.
///
/// The set is explicit instead of a numeric range so that removing a menu
/// entry never changes routing for a persisted `start_page` value.
const Set<int> kComingSoonIndices = <int>{};

/// Curriculum slots that are backed by the shared topic workbench.
const Map<int, String> _topicPages = <int, String>{
  27: 'linear_system',
  28: 'matrix_eigen',
  29: 'phase_portrait',
  30: 'critical_points',
  31: 'predator_prey',
  37: 'laplace',
  38: 'inverse_laplace',
  39: 'convolution',
  72: 'step_functions',
  73: 'discontinuous_forcing',
  74: 'impulse',
  114: 'transform_derivatives',
  115: 'laplace_ivp',
  40: 'fourier_series',
  41: 'fourier_transform',
  42: 'euler',
  43: 'rk4',
  44: 'multistep',
  45: 'error_convergence',
  65: 'heun',
  46: 'growth_decay',
  47: 'cooling',
  48: 'logistic',
  49: 'mixing',
  50: 'spring_mass',
  51: 'damped_oscillation',
  52: 'circuit',
  53: 'falling_body',
  54: 'pde_first_order',
  55: 'pde_classify',
  56: 'pde_characteristics',
  57: 'pde_separation',
  58: 'heat',
  59: 'wave',
  60: 'laplace_pde',
  32: 'series_power',
  33: 'series_singular_points',
  34: 'series_frobenius',
  35: 'special_legendre',
  36: 'special_bessel',
  75: 'system_theory',
  76: 'system_complex_eigen',
  77: 'system_fundamental',
  78: 'system_repeated',
  79: 'system_nonhomogeneous',
  80: 'system_rk4',
  81: 'competing_species',
  82: 'liapunov',
  83: 'limit_cycle',
  84: 'chaos_attractor',
  85: 'bvp_two_point',
  86: 'fourier_convergence',
  87: 'fourier_even_odd',
  88: 'heat_derivation',
  89: 'wave_derivation',
  90: 'bvp_occurrence',
  91: 'sturm_liouville',
  92: 'bvp_nonhomogeneous',
  93: 'sturm_liouville_singular',
  94: 'bessel_series',
  95: 'orthogonal_series',
  96: 'transcendental',
  97: 'integration_techniques',
  111: 'euler_equations',
  112: 'local_linearization',
  113: 'heat_other_bc',
};

/// Reading pages for curriculum slots that are explained but not interactive.
final Map<int, CurriculumArticle> _curriculumArticles = {
  for (final article in kCurriculumArticles) article.index: article,
};

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  I18n.instance = await I18n.load();
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
        final palette = OdePalette.byId(settings.palette);
        return MaterialApp(
          title: 'Infinit Lab',
          debugShowCheckedModeBanner: false,
          theme: buildOdeTheme(
            dark: false,
            palette: palette,
            highContrast: settings.highContrast,
            compact: settings.compact,
          ),
          darkTheme: buildOdeTheme(
            dark: true,
            palette: palette,
            highContrast: settings.highContrast,
            compact: settings.compact,
          ),
          themeMode: settings.themeMode,
          locale: settings.locale,
          supportedLocales: [
            for (final language in appLanguages) Locale(language.code),
          ],
          localizationsDelegates: const [
            GlobalMaterialLocalizations.delegate,
            GlobalWidgetsLocalizations.delegate,
            GlobalCupertinoLocalizations.delegate,
          ],
          builder: (context, child) {
            final media = MediaQuery.of(context);
            return SettingsScope(
              settings: settings,
              child: MediaQuery(
                data: media.copyWith(
                  textScaler: TextScaler.linear(settings.textScale),
                  disableAnimations: settings.reduceMotion,
                ),
                child: child ?? const SizedBox.shrink(),
              ),
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
    _index = widget.settings.startPage.clamp(0, _lastPageIndex);
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

  Future<void> _openSearch() async {
    final l10n = L10n(widget.settings.locale);
    final choice = await showDialog<_DrawerEntry>(
      context: context,
      builder: (context) =>
          _CommandPalette(l10n: l10n, entries: _flattenNav(_navSections(l10n))),
    );
    if (choice?.index != null && mounted) {
      _open(choice!.index!);
    }
  }

  Widget _page(L10n l10n) {
    final topicId = _topicPages[_index];
    if (topicId != null) {
      return TopicWorkbenchPage(
        key: ValueKey('topic-$topicId-$_index'),
        spec: kTopicCatalog[topicId]!,
        engine: _engine,
        l10n: l10n,
        engineError: _engineError,
      );
    }
    final article = _curriculumArticles[_index];
    if (article != null) {
      return CurriculumArticlePage(
        key: ValueKey('curriculum-${article.index}'),
        article: article,
        l10n: l10n,
        engine: _engine,
        onSolve: (input) => _open(2, seed: input),
        onOpen: (page, seed) => _open(page, seed: seed),
      );
    }
    if (kComingSoonIndices.contains(_index)) {
      return ComingSoonPage(key: ValueKey('coming-soon-$_index'), l10n: l10n);
    }
    switch (_index) {
      case 1:
        return LinearAlgebraPage(
          key: const ValueKey('linear-algebra'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
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
          onOpenCauchy: () => _open(17),
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
      case 61:
        return EducationalPage(
          key: const ValueKey('educational-derivative'),
          l10n: l10n,
          articleId: 'derivative',
          onOpen: (page, seed) => _open(page, seed: seed),
        );
      case 62:
        return EducationalPage(
          key: const ValueKey('educational-integral'),
          l10n: l10n,
          articleId: 'integral',
          onOpen: (page, seed) => _open(page, seed: seed),
        );
      case 63:
        return EducationalPage(
          key: const ValueKey('educational-ode-types'),
          l10n: l10n,
          articleId: 'ode_types',
          onOpen: (page, seed) => _open(page, seed: seed),
        );
      case 64:
        return EducationalPage(
          key: const ValueKey('educational-ode-applications'),
          l10n: l10n,
          articleId: 'ode_applications',
          onOpen: (page, seed) => _open(page, seed: seed),
        );
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
        );
      case 23:
        return TaylorPage(
          key: const ValueKey('taylor'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 24:
        return IntegralAppsPage(
          key: const ValueKey('integral-apps'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 25:
        return PartialPage(
          key: const ValueKey('partial'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      case 26:
        return SeriesPage(
          key: const ValueKey('series'),
          engine: _engine,
          engineError: _engineError,
          l10n: l10n,
        );
      default:
        return _HomePage(l10n: l10n, onOpen: _open);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = L10n(widget.settings.locale);
    final c = context.colors;
    return Directionality(
      textDirection: l10n.isRtl ? TextDirection.rtl : TextDirection.ltr,
      child: CallbackShortcuts(
        bindings: {
          const SingleActivator(LogicalKeyboardKey.keyK, control: true):
              _openSearch,
          const SingleActivator(LogicalKeyboardKey.keyK, meta: true):
              _openSearch,
          const SingleActivator(LogicalKeyboardKey.comma, control: true): () =>
              _open(22),
          const SingleActivator(LogicalKeyboardKey.comma, meta: true): () =>
              _open(22),
        },
        child: Focus(
          autofocus: true,
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
                  child: LayoutBuilder(
                    builder: (context, constraints) {
                      final wide = constraints.maxWidth >= 1320;
                      final content = Center(
                        child: ConstrainedBox(
                          constraints: const BoxConstraints(maxWidth: 1120),
                          child: ListView(
                            padding: const EdgeInsets.fromLTRB(22, 20, 22, 44),
                            children: [
                              _Header(
                                l10n: l10n,
                                showMenu: !wide,
                                onMenu: () =>
                                    _scaffoldKey.currentState?.openDrawer(),
                                onSearch: _openSearch,
                                onSettings: () => _open(22),
                              ),
                              const SizedBox(height: 26),
                              AnimatedSwitcher(
                                duration: widget.settings.reduceMotion
                                    ? Duration.zero
                                    : const Duration(milliseconds: 240),
                                switchInCurve: Curves.easeOutCubic,
                                switchOutCurve: Curves.easeInCubic,
                                transitionBuilder: (child, animation) =>
                                    FadeTransition(
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
                      );
                      if (!wide) return content;
                      return Row(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          _SideNav(
                            l10n: l10n,
                            selected: _index,
                            onSelected: _open,
                            onSearch: _openSearch,
                            version: _version,
                          ),
                          Expanded(child: content),
                        ],
                      );
                    },
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
    this.hint,
  }) : index = null;

  /// Destination page, null for a group.
  final int? index;
  final IconData icon;
  final String label;
  final String? hint;
  final List<_DrawerEntry> children;

  bool get isGroup => index == null;
}

class _AppDrawer extends StatefulWidget {
  const _AppDrawer({
    required this.l10n,
    required this.selected,
    required this.onSelected,
  });

  final L10n l10n;
  final int selected;
  final ValueChanged<int> onSelected;

  @override
  State<_AppDrawer> createState() => _AppDrawerState();
}

class _AppDrawerState extends State<_AppDrawer> {
  final _search = TextEditingController();
  String _query = '';

  @override
  void dispose() {
    _search.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final sections = _navSections(widget.l10n);
    final query = _query.trim().toLowerCase();
    return Drawer(
      width: 336,
      backgroundColor: c.card,
      child: SafeArea(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(20, 18, 10, 14),
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
                          widget.l10n.t('ui.modules'),
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    tooltip: widget.l10n.t('ui.close_menu'),
                    onPressed: () => Navigator.of(context).pop(),
                    icon: Icon(Icons.close_rounded, size: 20, color: c.muted),
                  ),
                ],
              ),
            ),
            Divider(color: c.line, height: 1),
            if (query.isEmpty)
              _DrawerLocation(sections: sections, selected: widget.selected),
            Padding(
              padding: const EdgeInsets.fromLTRB(14, 10, 14, 8),
              child: _DrawerSearchField(
                l10n: widget.l10n,
                controller: _search,
                onChanged: (value) => setState(() => _query = value),
              ),
            ),
            Expanded(
              child: query.isEmpty
                  ? ListView(
                      padding: const EdgeInsets.fromLTRB(10, 4, 10, 20),
                      children: [
                        for (var i = 0; i < sections.length; i++) ...[
                          if (i > 0 && sections[i].isGroup)
                            const SizedBox(height: 4),
                          _DrawerNode(
                            entry: sections[i],
                            l10n: widget.l10n,
                            selected: widget.selected,
                            onSelected: widget.onSelected,
                            depth: 0,
                          ),
                        ],
                      ],
                    )
                  : _DrawerSearchResults(
                      sections: sections,
                      l10n: widget.l10n,
                      selected: widget.selected,
                      onSelected: widget.onSelected,
                      query: query,
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

/// Shows where the current page sits in the module tree. This is the drawer's
/// wayfinding cue: the accordion keeps the list short, while the trail keeps
/// the user oriented when they open it from a deep page.
class _DrawerLocation extends StatelessWidget {
  const _DrawerLocation({required this.sections, required this.selected});

  final List<_DrawerEntry> sections;
  final int selected;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final path = _findPath(sections, selected);
    if (path == null || path.isEmpty) return const SizedBox.shrink();
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 10, 16, 2),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(top: 1),
            child: Icon(Icons.my_location_rounded, size: 14, color: c.muted),
          ),
          const SizedBox(width: 7),
          Expanded(
            child: Text.rich(
              TextSpan(
                children: [
                  for (var i = 0; i < path.length; i++) ...[
                    if (i > 0)
                      TextSpan(
                        text: '  /  ',
                        style: TextStyle(
                          fontFamily: 'Vazirmatn',
                          fontSize: 11.5,
                          color: c.muted.withValues(alpha: 0.65),
                        ),
                      ),
                    TextSpan(
                      text: path[i].label,
                      style: TextStyle(
                        fontFamily: 'Vazirmatn',
                        fontSize: 11.5,
                        fontWeight: i == path.length - 1
                            ? FontWeight.w700
                            : FontWeight.w500,
                        color: i == path.length - 1 ? c.accent : c.muted,
                      ),
                    ),
                  ],
                ],
              ),
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }
}

class _DrawerSearchField extends StatelessWidget {
  const _DrawerSearchField({
    required this.l10n,
    required this.controller,
    required this.onChanged,
  });

  final L10n l10n;
  final TextEditingController controller;
  final ValueChanged<String> onChanged;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return TextField(
      controller: controller,
      onChanged: onChanged,
      textInputAction: TextInputAction.search,
      style: const TextStyle(fontFamily: 'Vazirmatn', fontSize: 13),
      decoration: InputDecoration(
        isDense: true,
        filled: true,
        fillColor: c.cardAlt,
        hintText: l10n.t('ui.search_modules_2'),
        hintStyle: TextStyle(
          fontFamily: 'Vazirmatn',
          fontSize: 12.5,
          color: c.muted,
        ),
        prefixIcon: Icon(Icons.search_rounded, size: 19, color: c.muted),
        suffixIcon: controller.text.isEmpty
            ? null
            : IconButton(
                tooltip: l10n.t('ui.clear'),
                onPressed: () {
                  controller.clear();
                  onChanged('');
                },
                icon: Icon(Icons.close_rounded, size: 17, color: c.muted),
              ),
        contentPadding: const EdgeInsets.symmetric(
          horizontal: 12,
          vertical: 12,
        ),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: c.line),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: c.line),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: c.accent.withValues(alpha: 0.65)),
        ),
      ),
    );
  }
}

class _SearchResult {
  const _SearchResult(this.entry, this.path);

  final _DrawerEntry entry;
  final String path;
}

class _DrawerSearchResults extends StatelessWidget {
  const _DrawerSearchResults({
    required this.sections,
    required this.l10n,
    required this.selected,
    required this.onSelected,
    required this.query,
  });

  final List<_DrawerEntry> sections;
  final L10n l10n;
  final int selected;
  final ValueChanged<int> onSelected;
  final String query;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final results = _searchNav(sections, query);
    if (results.isEmpty) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Text(
            l10n.t('ui.no_module_matches_that_search'),
            textAlign: TextAlign.center,
            style: TextStyle(
              fontFamily: 'Vazirmatn',
              fontSize: 12.5,
              height: 1.7,
              color: c.muted,
            ),
          ),
        ),
      );
    }
    return ListView(
      padding: const EdgeInsets.fromLTRB(10, 4, 10, 20),
      children: [
        for (final result in results)
          _DrawerTile(
            icon: result.entry.icon,
            label: result.entry.label,
            hint: result.entry.hint,
            subtitle: result.path,
            selected: selected == result.entry.index,
            onTap: () => onSelected(result.entry.index!),
          ),
      ],
    );
  }
}

List<_DrawerEntry>? _findPath(List<_DrawerEntry> entries, int index) {
  for (final entry in entries) {
    if (entry.index == index) return [entry];
    if (entry.children.isNotEmpty) {
      final tail = _findPath(entry.children, index);
      if (tail != null) return [entry, ...tail];
    }
  }
  return null;
}

bool _containsIndex(_DrawerEntry entry, int index) {
  if (entry.index == index) return true;
  for (final child in entry.children) {
    if (_containsIndex(child, index)) return true;
  }
  return false;
}

int _leafCount(_DrawerEntry entry) {
  if (!entry.isGroup) return 1;
  var total = 0;
  for (final child in entry.children) {
    total += _leafCount(child);
  }
  return total;
}

List<_SearchResult> _searchNav(List<_DrawerEntry> sections, String query) {
  final results = <_SearchResult>[];
  void walk(List<_DrawerEntry> entries, List<String> path) {
    for (final entry in entries) {
      if (entry.isGroup) {
        walk(entry.children, [...path, entry.label]);
      } else {
        final haystack = '${entry.label} ${path.join(' ')}'.toLowerCase();
        if (haystack.contains(query)) {
          results.add(_SearchResult(entry, path.join(' / ')));
        }
      }
    }
  }

  walk(sections, const []);
  return results;
}

/// Persistent desktop navigation. It mirrors the drawer but keeps the module
/// tree visible, which makes moving between calculus tools a single click.
class _SideNav extends StatelessWidget {
  const _SideNav({
    required this.l10n,
    required this.selected,
    required this.onSelected,
    required this.onSearch,
    required this.version,
  });

  final L10n l10n;
  final int selected;
  final ValueChanged<int> onSelected;
  final VoidCallback onSearch;
  final String version;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final sections = _navSections(l10n);
    return Container(
      width: 278,
      decoration: BoxDecoration(
        color: c.card.withValues(alpha: 0.52),
        border: BorderDirectional(end: BorderSide(color: c.line)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(20, 22, 20, 14),
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
                        l10n.t('ui.symbolic_workspace'),
                        style: Theme.of(context).textTheme.bodySmall
                            ?.copyWith(fontSize: 11.5),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 14),
            child: _SearchLauncher(l10n: l10n, onTap: onSearch),
          ),
          const SizedBox(height: 10),
          Divider(color: c.line, height: 1),
          Expanded(
            child: ListView(
              padding: const EdgeInsets.fromLTRB(10, 12, 10, 16),
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
          if (version.isNotEmpty)
            Padding(
              padding: const EdgeInsets.fromLTRB(20, 8, 20, 16),
              child: Text(
                'v$version',
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(fontSize: 11, letterSpacing: 0.4),
              ),
            ),
        ],
      ),
    );
  }
}

class _SearchLauncher extends StatelessWidget {
  const _SearchLauncher({required this.l10n, required this.onTap});

  final L10n l10n;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: c.line),
          ),
          child: Row(
            children: [
              Icon(Icons.search_rounded, size: 18, color: c.muted),
              const SizedBox(width: 9),
              Expanded(
                child: Text(
                  l10n.t('ui.search_modules'),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 12.5,
                    color: c.muted,
                  ),
                ),
              ),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                decoration: BoxDecoration(
                  color: c.card,
                  borderRadius: BorderRadius.circular(6),
                  border: Border.all(color: c.line),
                ),
                child: Text(
                  'Ctrl K',
                  textDirection: TextDirection.ltr,
                  style: TextStyle(
                    fontFamily: 'SpaceGrotesk',
                    fontSize: 10,
                    fontWeight: FontWeight.w600,
                    color: c.muted,
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

List<_DrawerEntry> _flattenNav(List<_DrawerEntry> entries) {
  final result = <_DrawerEntry>[];
  for (final entry in entries) {
    if (entry.isGroup) {
      result.addAll(_flattenNav(entry.children));
    } else {
      result.add(entry);
    }
  }
  return result;
}

/// Page indices reachable from the drawer and the command palette.
@visibleForTesting
List<int> navReachableIndices(L10n l10n) => [
  for (final entry in _flattenNav(_navSections(l10n)))
    if (entry.index != null) entry.index!,
];

/// Drawer labels after catalogue lookup, in menu order. An unresolved key
/// renders verbatim as `nav.66.label`, so the curriculum guard can reject it.
@visibleForTesting
List<String> navResolvedLabels(L10n l10n) => [
  for (final entry in _flattenNav(_navSections(l10n))) entry.label,
];

/// Labels of groups that still wrap exactly one child. The drawer normalizer
/// must keep this list empty at every depth.
@visibleForTesting
List<String> navSingleChildGroupLabels(L10n l10n) {
  final violations = <String>[];
  void walk(List<_DrawerEntry> entries) {
    for (final entry in entries) {
      if (!entry.isGroup) continue;
      if (entry.children.length == 1) violations.add(entry.label);
      walk(entry.children);
    }
  }

  walk(_navSections(l10n));
  return violations;
}

class _CommandPalette extends StatefulWidget {
  const _CommandPalette({required this.l10n, required this.entries});

  final L10n l10n;
  final List<_DrawerEntry> entries;

  @override
  State<_CommandPalette> createState() => _CommandPaletteState();
}

class _CommandPaletteState extends State<_CommandPalette> {
  final _controller = TextEditingController();
  String _query = '';

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  List<_DrawerEntry> get _matches {
    final query = _query.trim().toLowerCase();
    if (query.isEmpty) return widget.entries.take(10).toList();
    return widget.entries
        .where(
          (e) =>
              e.label.toLowerCase().contains(query) ||
              (e.hint ?? '').toLowerCase().contains(query),
        )
        .take(10)
        .toList();
  }

  void _select(_DrawerEntry entry) => Navigator.of(context).pop(entry);

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final l = widget.l10n;
    final matches = _matches;
    return Dialog(
      backgroundColor: c.card,
      insetPadding: const EdgeInsets.all(24),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22)),
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 560),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(16, 16, 16, 12),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              TextField(
                controller: _controller,
                autofocus: true,
                onChanged: (value) => setState(() => _query = value),
                onSubmitted: (_) {
                  if (matches.isNotEmpty) _select(matches.first);
                },
                decoration: InputDecoration(
                  hintText: l.t('ui.search_modules_3'),
                  prefixIcon: Icon(Icons.search_rounded, color: c.muted),
                  isDense: true,
                ),
              ),
              const SizedBox(height: 10),
              if (matches.isEmpty)
                Padding(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 12,
                    vertical: 24,
                  ),
                  child: Text(
                    l.t('ui.no_module_matches_that_search_2'),
                    textAlign: TextAlign.center,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                )
              else
                ConstrainedBox(
                  constraints: const BoxConstraints(maxHeight: 360),
                  child: ListView(
                    shrinkWrap: true,
                    children: [
                      for (final entry in matches)
                        Material(
                          color: Colors.transparent,
                          child: InkWell(
                            borderRadius: BorderRadius.circular(12),
                            onTap: () => _select(entry),
                            child: Padding(
                              padding: const EdgeInsets.symmetric(
                                horizontal: 12,
                                vertical: 11,
                              ),
                              child: Row(
                                children: [
                                  Icon(entry.icon, size: 19, color: c.accent),
                                  const SizedBox(width: 12),
                                  Expanded(
                                    child: Text(
                                      entry.label,
                                      style: TextStyle(
                                        fontFamily: 'Vazirmatn',
                                        fontSize: 13.5,
                                        fontWeight: FontWeight.w600,
                                        color: c.text,
                                      ),
                                    ),
                                  ),
                                  if (entry.hint != null)
                                    Text(
                                      entry.hint!,
                                      style: TextStyle(
                                        fontFamily: 'Vazirmatn',
                                        fontSize: 11.5,
                                        color: c.muted,
                                      ),
                                    ),
                                  const SizedBox(width: 8),
                                  Icon(
                                    l.isFa
                                        ? Icons.arrow_back_rounded
                                        : Icons.arrow_forward_rounded,
                                    size: 15,
                                    color: c.muted,
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ),
                    ],
                  ),
                ),
              const SizedBox(height: 8),
              Row(
                children: [
                  Icon(Icons.keyboard_rounded, size: 14, color: c.muted),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      l.t('ui.enter_to_open_esc_to_close'),
                      style: Theme.of(context).textTheme.bodySmall
                          ?.copyWith(fontSize: 11),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

/// Collapses groups that only hold a single entry.
///
/// A one-child group makes the user open a folder that repeats the destination
/// underneath. Promoting the child keeps the drawer flat without special-casing
/// any particular index, so future single-entry groups collapse automatically.
List<_DrawerEntry> _collapseSingleChildGroups(List<_DrawerEntry> entries) {
  final result = <_DrawerEntry>[];
  for (final entry in entries) {
    if (!entry.isGroup) {
      result.add(entry);
      continue;
    }
    final children = _collapseSingleChildGroups(entry.children);
    if (children.length == 1) {
      result.add(children.single);
    } else {
      result.add(
        _DrawerEntry.group(
          icon: entry.icon,
          label: entry.label,
          hint: entry.hint,
          children: children,
        ),
      );
    }
  }
  return result;
}

List<_DrawerEntry> _navSections(L10n l10n) {
  _DrawerEntry soon(int index, IconData icon, String labelKey) =>
      _DrawerEntry.link(index: index, icon: icon, label: l10n.t(labelKey));

  // An implemented curriculum slot.  It looks like a normal link because the
  // workbench behind it is ready, not a reserved placeholder.
  _DrawerEntry topic(int index, IconData icon, String labelKey) =>
      _DrawerEntry.link(index: index, icon: icon, label: l10n.t(labelKey));

  return _collapseSingleChildGroups(<_DrawerEntry>[
    _DrawerEntry.group(
      icon: Icons.functions_rounded,
      label: l10n.t('ui.differential_equations'),
      hint: l10n.t('ui.solve_apply_transform'),
      children: [
        // The solver is the primary action, so it stays above the curriculum
        // groups even though it is a single link.
        _DrawerEntry.link(
          index: 2,
          icon: Icons.functions_rounded,
          label: l10n.t('ui.solve_an_equation'),
          hint: l10n.t('ui.forms'),
        ),
        _DrawerEntry.group(
          icon: Icons.looks_two_rounded,
          label: l10n.t('nav.group.ch2'),
          children: [
            soon(98, Icons.explore_rounded, 'nav.98.label'),
            soon(66, Icons.compare_arrows_rounded, 'nav.66.label'),
            soon(99, Icons.linear_scale_rounded, 'nav.99.label'),
            soon(100, Icons.call_split_rounded, 'nav.100.label'),
            soon(101, Icons.rule_rounded, 'nav.101.label'),
            soon(102, Icons.balance_rounded, 'nav.102.label'),
            soon(103, Icons.functions_rounded, 'nav.103.label'),
            soon(67, Icons.verified_user_rounded, 'nav.67.label'),
            soon(68, Icons.format_list_numbered_rounded, 'nav.68.label'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.looks_3_rounded,
          label: l10n.t('nav.group.ch3'),
          children: [
            topic(3, Icons.grid_on_rounded, 'ui.wronskian'),
            soon(104, Icons.tune_rounded, 'nav.104.label'),
            soon(105, Icons.blur_on_rounded, 'nav.105.label'),
            soon(106, Icons.repeat_rounded, 'nav.106.label'),
            soon(107, Icons.add_circle_outline_rounded, 'nav.107.label'),
            soon(108, Icons.tune_rounded, 'nav.108.label'),
            topic(50, Icons.architecture_rounded, 'topic.spring_mass.title'),
            topic(
              51,
              Icons.vibration_rounded,
              'topic.damped_oscillation.title',
            ),
            topic(52, Icons.electrical_services_rounded, 'topic.circuit.title'),
            soon(69, Icons.graphic_eq_rounded, 'nav.69.label'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.looks_4_rounded,
          label: l10n.t('nav.group.ch4'),
          children: [
            soon(70, Icons.functions_rounded, 'nav.70.label'),
            soon(109, Icons.tune_rounded, 'nav.109.label'),
            soon(110, Icons.add_circle_outline_rounded, 'nav.110.label'),
            soon(71, Icons.tune_rounded, 'nav.71.label'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.auto_graph_rounded,
          label: l10n.t('ui.series_special_functions'),
          children: [
            topic(32, Icons.auto_graph_rounded, 'topic.series_power.title'),
            topic(
              33,
              Icons.adjust_rounded,
              'topic.series_singular_points.title',
            ),
            topic(
              35,
              Icons.legend_toggle_rounded,
              'topic.special_legendre.title',
            ),
            topic(111, Icons.functions_rounded, 'topic.euler_equations.title'),
            topic(
              34,
              Icons.account_tree_rounded,
              'topic.series_frobenius.title',
            ),
            topic(36, Icons.cyclone_rounded, 'topic.special_bessel.title'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.transform_rounded,
          label: l10n.t('ui.transforms'),
          children: [
            topic(37, Icons.transform_rounded, 'topic.laplace.title'),
            topic(
              114,
              Icons.linear_scale_rounded,
              'topic.transform_derivatives.title',
            ),
            topic(115, Icons.rule_rounded, 'topic.laplace_ivp.title'),
            topic(38, Icons.u_turn_left_rounded, 'topic.inverse_laplace.title'),
            topic(72, Icons.stairs_rounded, 'topic.step_functions.title'),
            topic(73, Icons.bolt_rounded, 'topic.discontinuous_forcing.title'),
            topic(74, Icons.flash_on_rounded, 'topic.impulse.title'),
            topic(39, Icons.join_full_rounded, 'topic.convolution.title'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.account_tree_rounded,
          label: l10n.t('nav.group.ch7'),
          children: [
            topic(27, Icons.hub_rounded, 'topic.linear_system.title'),
            topic(28, Icons.grid_4x4_rounded, 'topic.matrix_eigen.title'),
            topic(75, Icons.account_tree_rounded, 'topic.system_theory.title'),
            topic(
              76,
              Icons.blur_on_rounded,
              'topic.system_complex_eigen.title',
            ),
            topic(77, Icons.grid_4x4_rounded, 'topic.system_fundamental.title'),
            topic(78, Icons.repeat_rounded, 'topic.system_repeated.title'),
            topic(
              79,
              Icons.add_circle_outline_rounded,
              'topic.system_nonhomogeneous.title',
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.scatter_plot_rounded,
          label: l10n.t('nav.group.ch9'),
          children: [
            topic(29, Icons.scatter_plot_rounded, 'topic.phase_portrait.title'),
            topic(30, Icons.place_rounded, 'topic.critical_points.title'),
            topic(31, Icons.pets_rounded, 'topic.predator_prey.title'),
            topic(
              81,
              Icons.compare_arrows_rounded,
              'topic.competing_species.title',
            ),
            topic(82, Icons.trending_down_rounded, 'topic.liapunov.title'),
            topic(83, Icons.all_inclusive_rounded, 'topic.limit_cycle.title'),
            topic(
              84,
              Icons.scatter_plot_rounded,
              'topic.chaos_attractor.title',
            ),
            topic(
              112,
              Icons.linear_scale_rounded,
              'topic.local_linearization.title',
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.calculate_rounded,
          label: l10n.t('ui.numerical_methods'),
          children: [
            topic(42, Icons.straighten_rounded, 'topic.euler.title'),
            topic(65, Icons.linear_scale_rounded, 'topic.heun.title'),
            topic(43, Icons.stacked_line_chart_rounded, 'topic.rk4.title'),
            topic(
              44,
              Icons.format_list_numbered_rounded,
              'topic.multistep.title',
            ),
            topic(
              45,
              Icons.error_outline_rounded,
              'topic.error_convergence.title',
            ),
            topic(
              80,
              Icons.stacked_line_chart_rounded,
              'topic.system_rk4.title',
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.grid_on_rounded,
          label: l10n.t('nav.group.ch10'),
          children: [
            topic(
              40,
              Icons.multiline_chart_rounded,
              'topic.fourier_series.title',
            ),
            topic(86, Icons.rule_rounded, 'topic.fourier_convergence.title'),
            topic(87, Icons.balance_rounded, 'topic.fourier_even_odd.title'),
            topic(57, Icons.call_split_rounded, 'topic.pde_separation.title'),
            topic(58, Icons.local_fire_department_rounded, 'topic.heat.title'),
            topic(
              88,
              Icons.local_fire_department_rounded,
              'topic.heat_derivation.title',
            ),
            topic(
              113,
              Icons.local_fire_department_rounded,
              'topic.heat_other_bc.title',
            ),
            topic(59, Icons.waves_rounded, 'topic.wave.title'),
            topic(89, Icons.waves_rounded, 'topic.wave_derivation.title'),
            topic(60, Icons.blur_circular_rounded, 'topic.laplace_pde.title'),
            topic(85, Icons.straighten_rounded, 'topic.bvp_two_point.title'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.auto_graph_rounded,
          label: l10n.t('nav.group.ch11'),
          children: [
            topic(90, Icons.link_rounded, 'topic.bvp_occurrence.title'),
            topic(91, Icons.auto_graph_rounded, 'topic.sturm_liouville.title'),
            topic(
              92,
              Icons.add_chart_rounded,
              'topic.bvp_nonhomogeneous.title',
            ),
            topic(
              93,
              Icons.adjust_rounded,
              'topic.sturm_liouville_singular.title',
            ),
            topic(94, Icons.cyclone_rounded, 'topic.bessel_series.title'),
            topic(95, Icons.functions_rounded, 'topic.orthogonal_series.title'),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.science_rounded,
          label: l10n.t('ui.applications'),
          children: [
            topic(46, Icons.trending_up_rounded, 'topic.growth_decay.title'),
            topic(47, Icons.thermostat_rounded, 'topic.cooling.title'),
            topic(48, Icons.groups_rounded, 'topic.logistic.title'),
            topic(49, Icons.science_rounded, 'topic.mixing.title'),
            topic(53, Icons.arrow_downward_rounded, 'topic.falling_body.title'),
            _DrawerEntry.link(
              index: 4,
              icon: Icons.join_inner_rounded,
              label: l10n.t('ui.envelope'),
            ),
            _DrawerEntry.link(
              index: 20,
              icon: Icons.swap_calls_rounded,
              label: l10n.t('ui.orthogonal_trajectories'),
              hint: l10n.t('ui.right_angles'),
            ),
          ],
        ),
      ],
    ),
    _DrawerEntry.group(
      icon: Icons.calculate_rounded,
      label: l10n.t('ui.differential_integral'),
      hint: l10n.t('ui.limits_derivatives_integrals'),
      children: [
        _DrawerEntry.group(
          icon: Icons.category_rounded,
          label: l10n.t('nav.group.calculus_foundations'),
          children: [
            _DrawerEntry.link(
              index: 7,
              icon: Icons.linear_scale_rounded,
              label: l10n.t('ui.limit'),
              hint: l10n.t('ui.both_sides'),
            ),
            _DrawerEntry.link(
              index: 8,
              icon: Icons.timeline_rounded,
              label: l10n.t('ui.continuity'),
            ),
            _DrawerEntry.link(
              index: 9,
              icon: Icons.explore_rounded,
              label: l10n.t('ui.domain_range'),
              hint: l10n.t('ui.plot'),
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.drive_file_rename_outline_rounded,
          label: l10n.t('nav.group.calculus_differentiation'),
          children: [
            _DrawerEntry.link(
              index: 5,
              icon: Icons.drive_file_rename_outline_rounded,
              label: l10n.t('ui.derivative'),
              hint: l10n.t('ui.1st_10th_order'),
            ),
            _DrawerEntry.group(
              icon: Icons.insights_rounded,
              label: l10n.t('ui.applications_of_the_derivative'),
              children: [
                _DrawerEntry.link(
                  index: 10,
                  icon: Icons.insights_rounded,
                  label: l10n.t('ui.function_behaviour'),
                ),
                _DrawerEntry.link(
                  index: 11,
                  icon: Icons.show_chart_rounded,
                  label: l10n.t('ui.tangent_normal'),
                ),
                _DrawerEntry.link(
                  index: 12,
                  icon: Icons.vertical_align_bottom_rounded,
                  label: l10n.t('ui.optimisation'),
                ),
                _DrawerEntry.link(
                  index: 13,
                  icon: Icons.speed_rounded,
                  label: l10n.t('ui.related_rates'),
                ),
                _DrawerEntry.group(
                  icon: Icons.rule_rounded,
                  label: l10n.t('ui.important_theorems'),
                  children: [
                    _DrawerEntry.link(
                      index: 14,
                      icon: Icons.compare_arrows_rounded,
                      label: l10n.t('ui.l_h_pital_s_rule'),
                    ),
                    _DrawerEntry.link(
                      index: 15,
                      icon: Icons.rule_rounded,
                      label: l10n.t('ui.mean_value_theorem'),
                    ),
                    _DrawerEntry.link(
                      index: 16,
                      icon: Icons.horizontal_rule_rounded,
                      label: l10n.t('ui.rolle_s_theorem'),
                    ),
                    _DrawerEntry.link(
                      index: 18,
                      icon: Icons.menu_book_rounded,
                      label: l10n.t('ui.derivative_theorems_complete'),
                    ),
                  ],
                ),
              ],
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.integration_instructions_rounded,
          label: l10n.t('nav.group.calculus_integration'),
          children: [
            _DrawerEntry.link(
              index: 6,
              icon: Icons.integration_instructions_rounded,
              label: l10n.t('ui.integral'),
              hint: l10n.t('ui.1d_2d_3d'),
            ),
            _DrawerEntry.link(
              index: 24,
              icon: Icons.architecture_rounded,
              label: l10n.t('ui.integral_applications'),
              hint: l10n.t('ui.area_volume'),
            ),
            topic(
              97,
              Icons.integration_instructions_rounded,
              'topic.integration_techniques.title',
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.multiline_chart_rounded,
          label: l10n.t('nav.group.calculus_series'),
          children: [
            _DrawerEntry.link(
              index: 23,
              icon: Icons.polyline_rounded,
              label: l10n.t('ui.taylor_maclaurin'),
              hint: l10n.t('ui.polynomial'),
            ),
            _DrawerEntry.link(
              index: 26,
              icon: Icons.waves_rounded,
              label: l10n.t('ui.series_convergence'),
              hint: l10n.t('ui.tests'),
            ),
          ],
        ),
        _DrawerEntry.group(
          icon: Icons.auto_awesome_rounded,
          label: l10n.t('nav.group.calculus_further'),
          children: [
            _DrawerEntry.link(
              index: 25,
              icon: Icons.grid_4x4_rounded,
              label: l10n.t('ui.partial_derivatives'),
              hint: l10n.t('ui.gradient'),
            ),
            topic(96, Icons.show_chart_rounded, 'topic.transcendental.title'),
          ],
        ),
      ],
    ),
    _DrawerEntry.link(
      index: 1,
      icon: Icons.view_in_ar_rounded,
      label: l10n.t('ui.linear_algebra'),
      hint: l10n.t('ui.matrices_eigenvalues'),
    ),
    _DrawerEntry.group(
      icon: Icons.auto_stories_rounded,
      label: l10n.t('ui.educational_explanations'),
      hint: l10n.t('ui.study_notes'),
      children: [
        _DrawerEntry.link(
          index: 61,
          icon: Icons.drive_file_rename_outline_rounded,
          label: l10n.t('ui.derivative'),
        ),
        _DrawerEntry.link(
          index: 62,
          icon: Icons.integration_instructions_rounded,
          label: l10n.t('ui.integral'),
        ),
        _DrawerEntry.link(
          index: 63,
          icon: Icons.category_rounded,
          label: l10n.t('ui.types_of_differential_equations'),
        ),
        _DrawerEntry.link(
          index: 64,
          icon: Icons.science_rounded,
          label: l10n.t('ui.applications_of_differential_equations'),
        ),
      ],
    ),
  ]);
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
        depth: depth,
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

  @override
  void didUpdateWidget(covariant _DrawerGroupNode oldWidget) {
    super.didUpdateWidget(oldWidget);
    // A command-palette jump can land inside a group the user had collapsed.
    if (widget.selected != oldWidget.selected && _containsSelected()) {
      _open = true;
    }
  }

  bool _containsSelected() => _containsIndex(widget.entry, widget.selected);

  bool get _active => _containsSelected();

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final empty = widget.entry.children.isEmpty;
    final count = _leafCount(widget.entry);
    final titleSize = widget.depth == 0 ? 13.5 : 12.5;
    final indent = widget.depth * 12.0;
    final reduceMotion =
        MediaQuery.maybeOf(context)?.disableAnimations ?? false;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Semantics(
          button: true,
          expanded: _open,
          child: Material(
            color: _active && !_open ? c.accentSoft : Colors.transparent,
            borderRadius: BorderRadius.circular(12),
            child: InkWell(
              borderRadius: BorderRadius.circular(12),
              onTap: () => setState(() => _open = !_open),
              child: Padding(
                padding: EdgeInsetsDirectional.only(
                  start: 13 + indent,
                  end: 13,
                  top: widget.depth == 0 ? 12 : 10,
                  bottom: widget.depth == 0 ? 12 : 10,
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
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Text(
                            widget.entry.label,
                            style: TextStyle(
                              fontFamily: 'Vazirmatn',
                              fontSize: titleSize,
                              fontWeight: _active
                                  ? FontWeight.w700
                                  : FontWeight.w600,
                              color: _active ? c.accent : c.text,
                            ),
                          ),
                          if (widget.depth == 0 &&
                              widget.entry.hint != null &&
                              widget.entry.hint!.isNotEmpty)
                            Padding(
                              padding: const EdgeInsets.only(top: 2),
                              child: Text(
                                widget.entry.hint!,
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: TextStyle(
                                  fontFamily: 'Vazirmatn',
                                  fontSize: 11,
                                  color: c.muted,
                                ),
                              ),
                            ),
                        ],
                      ),
                    ),
                    if (count > 0) ...[
                      const SizedBox(width: 8),
                      Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 7,
                          vertical: 2,
                        ),
                        decoration: BoxDecoration(
                          color: _active ? c.accentSoft : c.cardAlt,
                          borderRadius: BorderRadius.circular(20),
                          border: Border.all(color: c.line),
                        ),
                        child: Text(
                          '$count',
                          style: TextStyle(
                            fontFamily: 'SpaceGrotesk',
                            fontSize: 10.5,
                            fontWeight: FontWeight.w600,
                            color: _active ? c.accent : c.muted,
                          ),
                        ),
                      ),
                    ],
                    const SizedBox(width: 4),
                    AnimatedRotation(
                      turns: _open ? 0.5 : 0.0,
                      duration: reduceMotion
                          ? Duration.zero
                          : const Duration(milliseconds: 180),
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
        ),
        AnimatedSize(
          duration: reduceMotion
              ? Duration.zero
              : const Duration(milliseconds: 190),
          curve: Curves.easeOutCubic,
          alignment: Alignment.topCenter,
          child: _open
              ? Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    if (empty)
                      Padding(
                        padding: EdgeInsetsDirectional.fromSTEB(
                          13 + indent,
                          6,
                          13,
                          10,
                        ),
                        child: Text(
                          widget.l10n.t(
                            'ui.nothing_has_been_added_to_this_section_yet',
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
                )
              : const SizedBox(width: double.infinity, height: 0),
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
    this.subtitle,
    this.depth = 0,
  });

  final IconData icon;
  final String label;
  final String? hint;
  final String? subtitle;
  final bool selected;
  final VoidCallback onTap;
  final int depth;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Padding(
      padding: EdgeInsetsDirectional.only(
        start: depth * 12.0,
        top: 2,
        bottom: 2,
      ),
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
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        label,
                        style: TextStyle(
                          fontFamily: 'Vazirmatn',
                          fontSize: 13,
                          fontWeight: selected
                              ? FontWeight.w700
                              : FontWeight.w500,
                          color: selected ? c.accent : c.text,
                        ),
                      ),
                      if (subtitle != null && subtitle!.isNotEmpty)
                        Padding(
                          padding: const EdgeInsets.only(top: 2),
                          child: Text(
                            subtitle!,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: TextStyle(
                              fontFamily: 'Vazirmatn',
                              fontSize: 11,
                              color: c.muted,
                            ),
                          ),
                        ),
                    ],
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

/// The Infinit Lab mark: the same artwork as the launcher icon, tinted with the
/// active palette. Using the real drawing instead of a tiny hand-built path
/// keeps the loops and the 0/1 counters smooth at header sizes.
class InfinitMark extends StatelessWidget {
  const InfinitMark({super.key, this.size = 42, this.tilt = 0});

  /// Height of the mark; the artwork is a little wider than it is tall.
  final double size;

  /// Optional lean in degrees. The header leaves it at 0 so the mark sits
  /// square with the title and menu button.
  final double tilt;

  static const _aspect = 878 / 513;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final mark = SizedBox(
      width: size * _aspect,
      height: size,
      child: ShaderMask(
        blendMode: BlendMode.srcIn,
        shaderCallback: (rect) => LinearGradient(
          begin: Alignment.centerLeft,
          end: Alignment.centerRight,
          colors: [
            c.accent,
            Color.lerp(c.accent, c.accentAlt, 0.55)!,
            c.accentAlt,
          ],
        ).createShader(rect),
        child: Image.asset(
          'assets/branding/infinit-mark.png',
          fit: BoxFit.contain,
          filterQuality: FilterQuality.high,
          excludeFromSemantics: true,
        ),
      ),
    );
    if (tilt == 0) return mark;
    return Transform.rotate(angle: tilt * math.pi / 180, child: mark);
  }
}

class _Header extends StatelessWidget {
  const _Header({
    required this.l10n,
    required this.showMenu,
    required this.onMenu,
    required this.onSearch,
    required this.onSettings,
  });

  final L10n l10n;
  final bool showMenu;
  final VoidCallback onMenu;
  final VoidCallback onSearch;
  final VoidCallback onSettings;

  @override
  Widget build(BuildContext context) {
    // The header is shared by desktop and phones; the mark needs to stay
    // proportionate to the title on a narrow screen instead of dominating it.
    final compact = MediaQuery.sizeOf(context).width < 700;
    return Row(
      children: [
        if (showMenu) ...[
          _RoundIconButton(
            tooltip: l10n.t('ui.open_menu'),
            icon: Icons.menu_rounded,
            onPressed: onMenu,
          ),
          const SizedBox(width: 14),
        ],
        InfinitMark(size: compact ? 34 : 42, tilt: 0),
        const SizedBox(width: 14),
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
          tooltip: l10n.t('ui.search_modules_ctrl_k'),
          icon: Icons.search_rounded,
          onPressed: onSearch,
        ),
        const SizedBox(width: 8),
        _RoundIconButton(
          tooltip: l10n.t('ui.settings_ctrl'),
          icon: Icons.settings_rounded,
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
        padding: const EdgeInsets.all(12),
      ),
      icon: Icon(icon, size: 20, color: c.muted),
    );
  }
}

// --------------------------------------------------------------------- home

class _OdeForm {
  const _OdeForm({
    required this.labelKey,
    required this.standard,
    required this.example,
    required this.order,
  });

  final String labelKey;
  final String standard; // LaTeX
  final String example;
  final int order;
}

const _forms = <_OdeForm>[
  _OdeForm(
    labelKey: 'ui.ode_form.exact',

    standard: r'M\,dx + N\,dy = 0',
    example: '(2x+y) + (x+2y)y\' = 0',
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.homogeneous',

    standard: r"y' = f\left(\frac{y}{x}\right)",
    example: "y' = (x^2+y^2)/(x*y)",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.linear',

    standard: r"y' + P(x)\,y = Q(x)",
    example: "y' + y/x = x^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.bernoulli',

    standard: r"y' + P(x)\,y = Q(x)\,y^n",
    example: "y' = x*y + y^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.clairaut',

    standard: r"y = x\,y' + f(y')",
    example: "y = x*y' + (y')^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.lagrange',

    standard: r"y = x\,g(y') + f(y')",
    example: "y = 2*x*y' + (y')^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.riccati',

    standard: r"y' = P + Qy + Ry^2",
    example: "y' = y^2 - 2/x^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.separable',

    standard: r"y' = f(x)\,g(y)",
    example: "y' = y^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.linear_substitution',

    standard: r"y' = f(ax+by+c)",
    example: "y' = (x+y)^2",
    order: 1,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.constant_coefficients',

    standard: r"y'' + p\,y' + q\,y = f(x)",
    example: "y'' - 3y' + 2y = 0",
    order: 2,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.direct_integration',

    standard: r"y'' = f(x)",
    example: "y'' = 6x",
    order: 2,
  ),
  _OdeForm(
    labelKey: 'ui.ode_form.oscillator',

    standard: r"y'' + \omega^2 y = 0",
    example: "y'' + y = 0",
    order: 2,
  ),
];

class _HomePage extends StatefulWidget {
  const _HomePage({required this.l10n, required this.onOpen});

  final L10n l10n;
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
        _hero(context, l),
        const SizedBox(height: 14),
        LearningCard(
          l10n: l,
          title: l.t('ui.how_to_read_a_solution'),
          idea: l.t('ui.every_answer_comes_in_three_layers_the_closed_fo'),
          expandLabel: l.t('ui.read_the_workflow'),
          points: [
            l.t('ui.the_answer_card_states_what_was_solved_and_any_a'),
            l.t('ui.the_solution_path_names_each_rule_so_you_can_rep'),
            l.t('ui.validation_substitutes_the_answer_back_a_failed'),
          ],
        ),
        const SizedBox(height: 26),
        LayoutBuilder(
          builder: (context, constraints) {
            final label = SectionLabel(l.t('ui.forms_we_recognise'));
            final switcher = SegmentedButton<int>(
              showSelectedIcon: false,
              style: const ButtonStyle(
                visualDensity: VisualDensity.compact,
                tapTargetSize: MaterialTapTargetSize.shrinkWrap,
              ),
              segments: [
                ButtonSegment(value: 1, label: Text(l.t('ui.first_order'))),
                ButtonSegment(value: 2, label: Text(l.t('ui.second_order'))),
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
                      label: l.t(form.labelKey),
                      latex: form.standard,
                      onTap: () => widget.onOpen(2, seed: form.example),
                    ),
                  ),
              ],
            );
          },
        ),
        const SizedBox(height: 28),
        SectionLabel(l.t('ui.beyond_solving')),
        const SizedBox(height: 12),
        LayoutBuilder(
          builder: (context, constraints) {
            final wide = constraints.maxWidth >= 620;
            final cards = [
              _WideCard(
                icon: Icons.grid_on_rounded,
                title: l.t('ui.wronskian'),
                subtitle: l.t('ui.linear_independence_of_a_set_of_functions'),
                onTap: () => widget.onOpen(3),
              ),
              _WideCard(
                icon: Icons.join_inner_rounded,
                title: l.t('ui.envelope_trajectories'),
                subtitle: l.t(
                  'ui.envelopes_of_a_family_orthogonal_trajectories_an',
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
            l.t('ui.see_the_steps_not_just_the_answer'),
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(fontSize: 12, letterSpacing: 0.2, color: c.muted),
          ),
        ),
      ],
    );
  }

  Widget _hero(BuildContext context, L10n l) {
    final c = context.colors;
    // The accent wash marks the hero as the single focal point of the page.
    // Everything below it uses flat card surfaces.
    final copy = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          l.t('ui.every_equation_has_a_form'),
          style: Theme.of(context).textTheme.headlineSmall?.copyWith(
            fontSize: 27,
            fontWeight: FontWeight.w700,
            height: 1.25,
            letterSpacing: -0.4,
          ),
        ),
        const SizedBox(height: 10),
        Text(
          l.t('ui.enter_an_ordinary_differential_equation_infinit'),
          style: Theme.of(context).textTheme.bodySmall?.copyWith(height: 1.65),
        ),
        const SizedBox(height: 20),
        FilledButton.icon(
          onPressed: () => widget.onOpen(2, seed: "y' + y/x = x^2"),
          icon: const Icon(Icons.play_arrow_rounded, size: 20),
          label: Text(l.t('ui.solve_an_equation_2')),
        ),
        const SizedBox(height: 18),
        Wrap(
          spacing: 16,
          runSpacing: 8,
          children: [
            _HeroFeature(
              icon: Icons.check_circle_outline_rounded,
              label: l.t('ui.exact_rational_arithmetic'),
            ),
            _HeroFeature(
              icon: Icons.route_outlined,
              label: l.t('ui.every_step_explained'),
            ),
            _HeroFeature(
              icon: Icons.cloud_off_rounded,
              label: l.t('ui.works_fully_offline'),
            ),
          ],
        ),
      ],
    );

    return Container(
      padding: const EdgeInsets.fromLTRB(24, 26, 24, 24),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: AlignmentDirectional.topStart,
          end: AlignmentDirectional.bottomEnd,
          colors: [c.accent.withValues(alpha: 0.10), c.card],
        ),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: c.accent.withValues(alpha: 0.20)),
      ),
      child: LayoutBuilder(
        builder: (context, constraints) {
          if (constraints.maxWidth < 780) {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                copy,
                const SizedBox(height: 22),
                _HeroPreview(l10n: l),
              ],
            );
          }
          return Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              Expanded(flex: 6, child: copy),
              const SizedBox(width: 28),
              Expanded(flex: 5, child: _HeroPreview(l10n: l)),
            ],
          );
        },
      ),
    );
  }

  Widget _calculusRow(BuildContext context, L10n l) {
    final c = context.colors;
    final items = [
      (
        icon: Icons.drive_file_rename_outline_rounded,
        label: l.t('ui.derivative'),
        index: 5,
      ),
      (
        icon: Icons.integration_instructions_rounded,
        label: l.t('ui.integral'),
        index: 6,
      ),
      (icon: Icons.linear_scale_rounded, label: l.t('ui.limit'), index: 7),
      (icon: Icons.timeline_rounded, label: l.t('ui.continuity'), index: 8),
      (icon: Icons.explore_rounded, label: l.t('ui.domain_range'), index: 9),
    ];
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        Text(
          l.t('ui.calculus_tools'),
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

class _HeroFeature extends StatelessWidget {
  const _HeroFeature({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 15, color: c.accent),
        const SizedBox(width: 6),
        Text(
          label,
          style: TextStyle(
            fontFamily: 'Vazirmatn',
            fontSize: 11.5,
            fontWeight: FontWeight.w500,
            color: c.muted,
          ),
        ),
      ],
    );
  }
}

class _HeroPreview extends StatelessWidget {
  const _HeroPreview({required this.l10n});

  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.fromLTRB(16, 14, 16, 16),
      decoration: BoxDecoration(
        color: c.background.withValues(alpha: 0.6),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: c.line),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(
            children: [
              Icon(Icons.functions_rounded, size: 15, color: c.accent),
              const SizedBox(width: 7),
              Expanded(
                child: Text(
                  l10n.t('ui.worked_example'),
                  style: TextStyle(
                    fontFamily: 'Vazirmatn',
                    fontSize: 11.5,
                    fontWeight: FontWeight.w700,
                    letterSpacing: 0.2,
                    color: c.muted,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          _MathLine(
            latex: r"y' + \frac{y}{x} = x^2",
            color: c.text,
            fontSize: 17,
          ),
          const SizedBox(height: 12),
          Divider(height: 1, color: c.line),
          const SizedBox(height: 12),
          _SolutionStep(
            index: '1',
            label: l10n.t('ui.linear_first_order'),
            latex: r"y' + P(x)\,y = Q(x)",
          ),
          const SizedBox(height: 10),
          _SolutionStep(
            index: '2',
            label: l10n.t('ui.integrating_factor'),
            latex: r"\mu(x) = x",
          ),
          const SizedBox(height: 10),
          _SolutionStep(
            index: '3',
            label: l10n.t('ui.substitute_back'),
            latex: r"y = \frac{x^3}{4} + \frac{C}{x}",
          ),
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
            decoration: BoxDecoration(
              color: c.successSoft,
              borderRadius: BorderRadius.circular(10),
            ),
            child: Row(
              children: [
                Icon(Icons.check_circle_rounded, size: 14, color: c.success),
                const SizedBox(width: 6),
                Expanded(
                  child: Text(
                    l10n.t('ui.validate_the_answer'),
                    style: TextStyle(
                      fontFamily: 'Vazirmatn',
                      fontSize: 11.5,
                      fontWeight: FontWeight.w600,
                      color: c.success,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _SolutionStep extends StatelessWidget {
  const _SolutionStep({
    required this.index,
    required this.label,
    required this.latex,
  });

  final String index;
  final String label;
  final String latex;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 20,
          height: 20,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            color: c.accent.withValues(alpha: 0.14),
            borderRadius: BorderRadius.circular(6),
          ),
          child: Text(
            index,
            textDirection: TextDirection.ltr,
            style: TextStyle(
              fontFamily: 'SpaceGrotesk',
              fontSize: 11,
              fontWeight: FontWeight.w700,
              color: c.accent,
            ),
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(
                label,
                style: TextStyle(
                  fontFamily: 'Vazirmatn',
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                  color: c.text,
                ),
              ),
              const SizedBox(height: 3),
              _MathLine(latex: latex, color: c.muted, fontSize: 13),
            ],
          ),
        ),
      ],
    );
  }
}

class _MathLine extends StatelessWidget {
  const _MathLine({
    required this.latex,
    required this.color,
    this.fontSize = 14,
  });

  final String latex;
  final Color color;
  final double fontSize;

  @override
  Widget build(BuildContext context) {
    return Directionality(
      textDirection: TextDirection.ltr,
      child: Align(
        alignment: AlignmentDirectional.centerStart,
        child: SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: Math.tex(
            latex,
            textStyle: TextStyle(
              fontFamily: 'Vazirmatn',
              fontSize: fontSize,
              color: color,
            ),
            onErrorFallback: (error) => Text(
              latex,
              style: TextStyle(
                fontFamily: 'DejaVu Sans Mono',
                fontSize: fontSize - 1,
                color: color,
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
    return _HoverSurface(
      onTap: onTap,
      radius: 16,
      childBuilder: (context, hovered) => Padding(
        padding: const EdgeInsets.fromLTRB(14, 13, 14, 13),
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
                fontSize: 12.5,
                fontWeight: FontWeight.w700,
                color: hovered ? c.accent : c.text,
              ),
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
    return _HoverSurface(
      onTap: onTap,
      radius: 18,
      childBuilder: (context, hovered) => Padding(
        padding: const EdgeInsets.fromLTRB(16, 15, 14, 15),
        child: Row(
          children: [
            Container(
              width: 38,
              height: 38,
              decoration: BoxDecoration(
                color: hovered ? c.accentSoft : c.cardAlt,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(
                  color: hovered
                      ? c.accent.withValues(alpha: 0.4)
                      : c.accent.withValues(alpha: 0.18),
                ),
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
                      color: hovered ? c.accent : c.text,
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
            Icon(
              l10nDirection(context) == TextDirection.rtl
                  ? Icons.arrow_back_rounded
                  : Icons.arrow_forward_rounded,
              size: 16,
              color: hovered ? c.accent : c.muted,
            ),
          ],
        ),
      ),
    );
  }
}

/// A card that lifts and glows slightly on pointer hover. Pointer hover is a
/// desktop affordance, so touch devices simply get the resting state.
class _HoverSurface extends StatefulWidget {
  const _HoverSurface({
    required this.onTap,
    required this.radius,
    required this.childBuilder,
  });

  final VoidCallback onTap;
  final double radius;
  final Widget Function(BuildContext context, bool hovered) childBuilder;

  @override
  State<_HoverSurface> createState() => _HoverSurfaceState();
}

class _HoverSurfaceState extends State<_HoverSurface> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final reduceMotion =
        MediaQuery.maybeOf(context)?.disableAnimations ?? false;
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: AnimatedContainer(
        duration: reduceMotion
            ? Duration.zero
            : const Duration(milliseconds: 150),
        curve: Curves.easeOutCubic,
        transform: Matrix4.translationValues(
          0,
          _hovered && !reduceMotion ? -1 : 0,
          0,
        ),
        decoration: BoxDecoration(
          color: _hovered ? c.cardAlt : c.card,
          borderRadius: BorderRadius.circular(widget.radius),
          border: Border.all(
            color: _hovered ? c.accent.withValues(alpha: 0.42) : c.line,
          ),
        ),
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            onTap: widget.onTap,
            borderRadius: BorderRadius.circular(widget.radius),
            child: widget.childBuilder(context, _hovered),
          ),
        ),
      ),
    );
  }
}

TextDirection l10nDirection(BuildContext context) => Directionality.of(context);
