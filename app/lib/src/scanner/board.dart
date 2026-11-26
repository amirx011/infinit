// In-app handwriting board. Strokes stay on the device, are rasterized to a
// black-on-white bitmap, and go through the same classical recognizer as a
// photo. Recognition runs on a short debounce for a live preview; the user
// confirms before anything is solved.
import 'dart:async';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:file_selector/file_selector.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/semantics.dart';
import 'package:flutter_math_fork/flutter_math.dart';

import '../l10n.dart';
import '../scanner_engine.dart';
import '../theme.dart';
import 'board_store.dart';
import 'calibration_page.dart';
import 'prototype_store.dart';

class HandwritingBoardPage extends StatefulWidget {
  const HandwritingBoardPage({super.key, required this.l10n});

  final L10n l10n;

  @override
  State<HandwritingBoardPage> createState() => _HandwritingBoardPageState();
}

class _HandwritingBoardPageState extends State<HandwritingBoardPage> {
  static const _boardWidth = 1600.0;
  static const _boardHeight = 900.0;

  final _strokes = <List<Offset>>[];
  final _undo = <List<Offset>>[];
  final _redo = <List<Offset>>[];
  final _typed = <({String text, Offset at})>[];
  final _typedUndo = <({String text, Offset at})>[];
  final _typedRedo = <({String text, Offset at})>[];
  final _paintKey = GlobalKey();
  final _store = BoardStore();
  Timer? _debounce;
  Timer? _saveTimer;
  ScannerEngine? _scanner;
  String? _previewLatex;
  /// The last reading, kept so a symbol can be fixed without re-running the
  /// vision stages and so the strokes of the fixed symbol can become a
  /// prototype (§12A.5).
  ScanResult? _result;
  final Map<int, String> _fixes = {};
  bool _recognizing = false;
  bool _eraser = false;
  /// Stylus-only mode: a finger pans instead of drawing, which is how the plan
  /// asks for palm rejection without a settings dance.
  bool _stylusOnly = false;
  bool _guideLines = true;
  final _transform = TransformationController();
  int _pointers = 0;
  bool _fitted = false;
  Size _viewport = Size.zero;
  /// True while the current drag belongs to the pen, not to a pan.
  bool _drawing = false;
  int _revision = 0;
  int? _recognizedRevision;
  int _request = 0;

  L10n get l => widget.l10n;

  @override
  void initState() {
    super.initState();
    _loadScanner();
    _restore();
  }

  /// Restores the last board. Drawing is local only, so this is a plain file
  /// read; a corrupt or missing file just means a blank board.
  Future<void> _restore() async {
    final data = await _store.load();
    if (!mounted || data == null) return;
    final strokes = data['strokes'];
    final typed = data['typed'];
    setState(() {
      _strokes.clear();
      if (strokes is List) {
        for (final stroke in strokes) {
          if (stroke is! List) continue;
          final points = <Offset>[];
          for (final point in stroke) {
            if (point is List && point.length >= 2) {
              points.add(
                Offset(
                  (point[0] as num).toDouble(),
                  (point[1] as num).toDouble(),
                ),
              );
            }
          }
          if (points.isNotEmpty) _strokes.add(points);
        }
      }
      _typed.clear();
      if (typed is List) {
        for (final entry in typed) {
          if (entry is Map && entry['text'] is String) {
            _typed.add((
              text: entry['text'] as String,
              at: Offset(
                (entry['x'] as num?)?.toDouble() ?? 0,
                (entry['y'] as num?)?.toDouble() ?? 0,
              ),
            ));
          }
        }
      }
      ++_revision;
    });
    if (_strokes.isNotEmpty || _typed.isNotEmpty) _scheduleRecognition();
  }

  /// Writes the board a moment after the last edit. Rounding the coordinates
  /// keeps the file small without a visible difference when it comes back.
  void _scheduleSave() {
    _saveTimer?.cancel();
    _saveTimer = Timer(const Duration(milliseconds: 700), () {
      _store.save({
        'strokes': [
          for (final stroke in _strokes)
            [
              for (final point in stroke)
                [point.dx.round(), point.dy.round()],
            ],
        ],
        'typed': [
          for (final entry in _typed)
            {'text': entry.text, 'x': entry.at.dx, 'y': entry.at.dy},
        ],
      });
    });
  }

  Future<void> _loadScanner() async {
    try {
      final scanner = await ScannerEngine.load();
      if (mounted) setState(() => _scanner = scanner);
    } catch (_) {
      // The board still works; recognition reports the failure when used.
    }
  }

  @override
  void dispose() {
    _debounce?.cancel();
    _saveTimer?.cancel();
    super.dispose();
  }

  void _scheduleRecognition() {
    _debounce?.cancel();
    _debounce = Timer(const Duration(milliseconds: 700), _recognize);
  }

  Future<Uint8List> _rasterize({double pixelRatio = 1.0}) async {
    final recorder = ui.PictureRecorder();
    final canvas = Canvas(recorder);
    canvas.scale(pixelRatio);
    canvas.drawRect(
      Rect.fromLTWH(0, 0, _boardWidth, _boardHeight),
      Paint()..color = Colors.white,
    );
    final painter = _InkPainter(
      strokes: _strokes,
      typed: _typed,
      preview: false,
      guides: false,
    );
    painter.paint(canvas, const Size(_boardWidth, _boardHeight));
    final picture = recorder.endRecording();
    final image = await picture.toImage(
      (_boardWidth * pixelRatio).round(),
      (_boardHeight * pixelRatio).round(),
    );
    final data = await image.toByteData(format: ui.ImageByteFormat.png);
    image.dispose();
    picture.dispose();
    return data!.buffer.asUint8List();
  }

  Future<void> _recognize() async {
    if (_strokes.isEmpty && _typed.isEmpty) {
      setState(() => _previewLatex = null);
      return;
    }
    if (_recognizedRevision == _revision && _previewLatex != null) return;
    final scanner = _scanner;
    if (scanner == null) return;
    final revision = _revision;
    final request = ++_request;
    scanner.cancel();
    setState(() => _recognizing = true);
    try {
      // Typed quick-keys are text, not strokes, so a board with typed symbols
      // still goes through the bitmap path. Pure ink uses the stroke front end,
      // which knows which strokes belong to the same symbol.
      final ScanResult result;
      if (_typed.isEmpty) {
        result = await scanner.readInk(
          _strokes,
          width: _boardWidth.round(),
          height: _boardHeight.round(),
        );
      } else {
        final png = await _rasterize(pixelRatio: 1.0);
        result = await scanner.read(png);
      }
      if (!mounted || request != _request) return;
      setState(() {
        _previewLatex = result.latex.isEmpty ? result.expression : result.latex;
        _result = result;
        if (revision == _revision) _recognizedRevision = revision;
        _recognizing = false;
      });
      final spoken = result.expression.isEmpty ? result.latex : result.expression;
      if (spoken.isNotEmpty) {
        SemanticsService.sendAnnouncement(
          View.of(context),
          l.t('scan.semantics.recognized', {'expression': spoken}),
          Directionality.of(context),
        );
      }
    } catch (_) {
      if (!mounted || request != _request) return;
      setState(() => _recognizing = false);
    }
  }

  /// Initial fit and the Fit button both land here: board pixels to the middle
  /// of the viewport at the largest scale that still shows everything.
  void _fitIfNeeded(Size viewport) {
    if (_fitted || viewport.width <= 0 || viewport.height <= 0) return;
    _fitted = true;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      _fitTo(viewport);
    });
  }

  void _fitTo(Size viewport) {
    if (viewport.width <= 0 || viewport.height <= 0) return;
    final scale = math.min(
      viewport.width / _boardWidth,
      viewport.height / _boardHeight,
    );
    final dx = (viewport.width - _boardWidth * scale) / 2;
    final dy = (viewport.height - _boardHeight * scale) / 2;
    _transform.value = Matrix4.identity()
      ..translateByDouble(dx, dy, 0, 1)
      ..scaleByDouble(scale, scale, 1, 1);
    setState(() {});
  }

  /// Scratch-out and lasso, the two gestures the plan names for erasing
  /// without switching tools. Both are recognized from the finished stroke:
  /// a tight zigzag erases what it covers, a closed loop deletes what it
  /// encloses.
  void _finishGesture() {
    if (_strokes.isEmpty) return;
    final last = _strokes.last;
    if (last.length < 6) return;
    if (_isScratchOut(last)) {
      final covered = last.fold<Rect?>(
        null,
        (rect, point) => rect == null ? Rect.fromLTWH(point.dx, point.dy, 0, 0)
            : rect.expandToInclude(Rect.fromLTWH(point.dx, point.dy, 0, 0)),
      );
      setState(() {
        _strokes.removeLast();
        if (covered != null) _eraseInside(covered.inflate(12));
      });
      return;
    }
    if (_isLasso(last)) {
      final loop = _strokes.removeLast();
      final area = _pathBounds(loop);
      setState(() => _eraseInside(area));
    }
  }

  bool _isScratchOut(List<Offset> stroke) {
    final bounds = _pathBounds(stroke);
    if (bounds.width < 40) return false;
    // A zigzag reverses horizontal direction several times inside a short
    // vertical band; a normal glyph does not.
    var reversals = 0;
    var direction = 0.0;
    for (var i = 1; i < stroke.length; i++) {
      final dx = stroke[i].dx - stroke[i - 1].dx;
      if (dx.abs() < 0.5) continue;
      final sign = dx.sign;
      if (direction != 0 && sign != direction) reversals++;
      direction = sign;
    }
    return reversals >= 4 && bounds.height < bounds.width * 0.8;
  }

  bool _isLasso(List<Offset> stroke) {
    final first = stroke.first;
    final last = stroke.last;
    if ((first - last).distance > 90) return false;
    final bounds = _pathBounds(stroke);
    if (bounds.width < 60 || bounds.height < 40) return false;
    // A loop covers its own bounding box far more than a glyph does.
    final path = Path()..moveTo(first.dx, first.dy);
    for (final point in stroke.skip(1)) {
      path.lineTo(point.dx, point.dy);
    }
    final perimeter = _pathLength(stroke);
    return perimeter > 2.2 * (bounds.width + bounds.height);
  }

  Rect _pathBounds(List<Offset> stroke) {
    var minX = stroke.first.dx, maxX = stroke.first.dx;
    var minY = stroke.first.dy, maxY = stroke.first.dy;
    for (final point in stroke) {
      minX = math.min(minX, point.dx);
      maxX = math.max(maxX, point.dx);
      minY = math.min(minY, point.dy);
      maxY = math.max(maxY, point.dy);
    }
    return Rect.fromLTRB(minX, minY, maxX, maxY);
  }

  double _pathLength(List<Offset> stroke) {
    var total = 0.0;
    for (var i = 1; i < stroke.length; i++) {
      total += (stroke[i] - stroke[i - 1]).distance;
    }
    return total;
  }

  void _eraseInside(Rect area) {
    final kept = <List<Offset>>[];
    for (final stroke in _strokes) {
      final inside = stroke.where(area.contains).length;
      if (inside * 2 < stroke.length) kept.add(stroke);
    }
    if (kept.length == _strokes.length) return;
    setState(() {
      _strokes
        ..clear()
        ..addAll(kept);
      ++_revision;
    });
    _scheduleSave();
    _scheduleRecognition();
  }

  void _insertSymbol(String text) {
    setState(() {
      final x = 40.0 + _typed.length * 58.0;
      _typed.add((text: text, at: Offset(x, _boardHeight * 0.55)));
      _typedRedo.clear();
      _redo.clear();
      _scheduleSave();
      ++_revision;
    });
    _scheduleRecognition();
  }

  void _redoStroke() {
    if (_redo.isEmpty && _typedRedo.isEmpty) return;
    setState(() {
      if (_typedRedo.isNotEmpty) {
        _typed.add(_typedRedo.removeLast());
      } else {
        _strokes.add(_redo.removeLast());
      }
      ++_revision;
    });
    _scheduleSave();
    _scheduleRecognition();
  }

  void _undoStroke() {
    if (_strokes.isEmpty && _typed.isEmpty) return;
    setState(() {
      if (_typed.isNotEmpty && _undo.isEmpty) {
        _typedUndo.add(_typed.removeLast());
      } else if (_strokes.isNotEmpty) {
        final stroke = _strokes.removeLast();
        _undo.add(stroke);
      }
      ++_revision;
    });
    _scheduleRecognition();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Scaffold(
      appBar: AppBar(
        title: Text(l.t('scan.handwrite')),
        actions: [
          IconButton(
            tooltip: l.t('scan.board_undo'),
            icon: const Icon(Icons.undo_rounded),
            onPressed: _undoStroke,
          ),
          IconButton(
            tooltip: l.t('scan.board_redo'),
            icon: const Icon(Icons.redo_rounded),
            onPressed: _redoStroke,
          ),
          IconButton(
            tooltip: l.t('scan.action.fix_symbols'),
            icon: const Icon(Icons.spellcheck_rounded),
            onPressed: (_result?.glyphs.isEmpty ?? true) ? null : _openFixSheet,
          ),
          IconButton(
            tooltip: l.t('scan.board_clear'),
            icon: const Icon(Icons.delete_outline_rounded),
            onPressed: () => setState(() {
              _strokes.clear();
              _typed.clear();
              _undo.clear();
              _redo.clear();
              _typedUndo.clear();
              _typedRedo.clear();
              _previewLatex = null;
              _result = null;
              _fixes.clear();
              ++_revision;
            }),
          ),
          IconButton(
            tooltip: l.t('scan.board.fit'),
            icon: const Icon(Icons.fit_screen_rounded),
            onPressed: () => _fitTo(_viewport),
          ),
          PopupMenuButton<String>(
            tooltip: l.t('scan.board.settings'),
            icon: const Icon(Icons.tune_rounded),
            onSelected: (value) {
              if (value == 'calibrate') {
                Navigator.of(context).push(
                  MaterialPageRoute<void>(
                    builder: (_) => CalibrationPage(l10n: l),
                  ),
                );
                return;
              }
              setState(() {
                if (value == 'stylus') _stylusOnly = !_stylusOnly;
                if (value == 'guides') _guideLines = !_guideLines;
              });
            },
            itemBuilder: (context) => [
              CheckedPopupMenuItem<String>(
                value: 'stylus',
                checked: _stylusOnly,
                child: Text(l.t('scan.board.stylus_only')),
              ),
              CheckedPopupMenuItem<String>(
                value: 'guides',
                checked: _guideLines,
                child: Text(l.t('scan.board.guides')),
              ),
              PopupMenuItem<String>(
                value: 'calibrate',
                child: Text(l.t('scan.calib.title')),
              ),
            ],
          ),
          PopupMenuButton<String>(
            tooltip: l.t('scan.samples.title'),
            icon: const Icon(Icons.psychology_alt_outlined),
            onSelected: _samplesAction,
            itemBuilder: (context) => [
              PopupMenuItem<String>(
                enabled: false,
                child: Text(
                  l.t('scan.samples.count', {
                    'count': '${_scanner?.prototypeCount ?? 0}'
                        ' / ${_scanner?.inkPrototypeCount ?? 0}',
                  }),
                ),
              ),
              PopupMenuItem<String>(
                value: 'export',
                child: Text(l.t('scan.samples.export')),
              ),
              PopupMenuItem<String>(
                value: 'import',
                child: Text(l.t('scan.samples.import')),
              ),
              PopupMenuItem<String>(
                value: 'reset',
                child: Text(l.t('scan.samples.reset')),
              ),
            ],
          ),
          IconButton(
            tooltip: _eraser ? l.t('scan.board_pen') : l.t('scan.board_eraser'),
            isSelected: _eraser,
            icon: Icon(_eraser ? Icons.auto_fix_normal : Icons.edit_rounded),
            onPressed: () => setState(() => _eraser = !_eraser),
          ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 8),
              child: Text(
                l.t('scan.board_hint'),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
            Expanded(
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 12),
                child: Container(
                  decoration: BoxDecoration(
                    color: Colors.white,
                    borderRadius: BorderRadius.circular(18),
                    border: Border.all(color: c.line),
                  ),
                  clipBehavior: Clip.antiAlias,
                  child: LayoutBuilder(
                    builder: (context, constraints) {
                      _viewport = constraints.biggest;
                      _fitIfNeeded(constraints.biggest);
                      return Listener(
                        onPointerDown: (_) => _pointers++,
                        onPointerUp: (_) {
                          if (_pointers > 0) _pointers--;
                        },
                        onPointerCancel: (_) {
                          if (_pointers > 0) _pointers--;
                        },
                        child: InteractiveViewer(
                        transformationController: _transform,
                        constrained: false,
                        minScale: 0.2,
                        maxScale: 4.0,
                        // One finger draws; two fingers move the board. That
                        // is the plan's rule, and it needs no mode switch.
                        panEnabled: _pointers >= 2,
                        scaleEnabled: _pointers >= 2,
                        boundaryMargin: const EdgeInsets.all(400),
                        child: SizedBox(
                          width: _boardWidth,
                          height: _boardHeight,
                          child: GestureDetector(
                            key: _paintKey,
                            behavior: HitTestBehavior.opaque,
                            onPanStart: (details) {
                              // A finger is a pan gesture in stylus-only mode,
                              // and a second pointer always pans.
                              final finger = details.kind != null &&
                                  details.kind != PointerDeviceKind.stylus;
                              if (_pointers >= 2 ||
                                  (_stylusOnly && finger)) {
                                _drawing = false;
                                return;
                              }
                              _drawing = true;
                              if (_eraser) {
                                _eraseAt(details.localPosition);
                              } else {
                                setState(() {
                                  _strokes.add([details.localPosition]);
                                  _undo.clear();
                                  _redo.clear();
                                  ++_revision;
                                });
                                _scheduleSave();
                              }
                            },
                            onPanUpdate: (details) {
                              if (!_drawing) return;
                              if (_eraser) {
                                _eraseAt(details.localPosition);
                              } else if (_strokes.isNotEmpty) {
                                setState(
                                  () =>
                                      _strokes.last.add(details.localPosition),
                                );
                              }
                            },
                            onPanEnd: (_) {
                              if (!_drawing) return;
                              _drawing = false;
                              _finishGesture();
                              _scheduleSave();
                              _scheduleRecognition();
                            },
                            child: CustomPaint(
                              size: const Size(_boardWidth, _boardHeight),
                              painter: _InkPainter(
                                strokes: _strokes,
                                typed: _typed,
                                preview: false,
                                guides: _guideLines,
                              ),
                            ),
                          ),
                        ),
                        ),
                      );
                    },
                  ),
                ),
              ),
            ),
            if (_recognizing || _previewLatex != null)
              Container(
                width: double.infinity,
                margin: const EdgeInsets.fromLTRB(16, 10, 16, 0),
                padding: const EdgeInsets.symmetric(
                  horizontal: 14,
                  vertical: 10,
                ),
                decoration: BoxDecoration(
                  color: c.cardAlt,
                  borderRadius: BorderRadius.circular(14),
                  border: Border.all(color: c.line),
                ),
                child: Directionality(
                  textDirection: TextDirection.ltr,
                  child: _recognizing
                      ? Row(
                          children: [
                            const SizedBox(
                              width: 14,
                              height: 14,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            ),
                            const SizedBox(width: 10),
                            Text(
                              l.t('scan.processing'),
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                          ],
                        )
                      : Math.tex(
                          _previewLatex!,
                          textStyle: TextStyle(
                            fontFamily: 'Vazirmatn',
                            fontSize: 20,
                            color: c.text,
                          ),
                          onErrorFallback: (_) => Text(
                            _previewLatex!,
                            style: TextStyle(fontSize: 16, color: c.text),
                          ),
                        ),
                ),
              ),
            SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.fromLTRB(16, 10, 16, 6),
              child: Row(
                children: [
                  for (final symbol in const [
                    (latex: '\\int', ink: '∫'),
                    (latex: '\\sum', ink: '∑'),
                    (latex: '\\sqrt', ink: '√'),
                    (latex: '\\pi', ink: 'π'),
                    (latex: '\\theta', ink: 'θ'),
                    (latex: '\\le', ink: '≤'),
                    (latex: '\\ge', ink: '≥'),
                    (latex: '\\ne', ink: '≠'),
                    (latex: '\\pm', ink: '±'),
                    (latex: '(', ink: '('),
                    (latex: ')', ink: ')'),
                    (latex: '[', ink: '['),
                    (latex: ']', ink: ']'),
                    (latex: '|', ink: '|'),
                  ])
                    Padding(
                      padding: const EdgeInsets.only(right: 6),
                      child: OutlinedButton(
                        style: OutlinedButton.styleFrom(
                          minimumSize: const Size(44, 40),
                          padding: const EdgeInsets.symmetric(horizontal: 10),
                        ),
                        onPressed: () => _insertSymbol(symbol.ink),
                        child: Math.tex(symbol.latex),
                      ),
                    ),
                ],
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 12),
              child: Row(
                children: [
                  Expanded(
                    child: OutlinedButton(
                      onPressed: () => Navigator.of(context).pop(),
                      child: Text(l.t('ui.cancel')),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    flex: 2,
                    child: FilledButton.icon(
                      onPressed: _strokes.isEmpty && _typed.isEmpty
                          ? null
                          : () async {
                              final png = await _rasterize(pixelRatio: 1.0);
                              if (!context.mounted) return;
                              Navigator.of(context).pop(png);
                            },
                      icon: const Icon(Icons.arrow_forward_rounded, size: 18),
                      label: Text(l.t('scan.board_use')),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Symbols the user can fix, one row each.
  Future<void> _openFixSheet() async {
    final result = _result;
    final scanner = _scanner;
    if (result == null || scanner == null) return;
    final glyph = await showModalBottomSheet<ScanGlyph>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: ListView(
          shrinkWrap: true,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 8),
              child: Text(
                l.t('scan.fix.hint'),
                style: Theme.of(sheetContext).textTheme.bodySmall,
              ),
            ),
            for (final entry in result.glyphs)
              ListTile(
                dense: true,
                leading: Text(
                  '#${entry.id}',
                  style: const TextStyle(
                    fontFamily: 'DejaVu Sans Mono',
                    fontFamilyFallback: ['monospace'],
                  ),
                ),
                title: Text(entry.label, textDirection: TextDirection.ltr),
                subtitle: Text(entry.score.toStringAsFixed(2)),
                onTap: () => Navigator.of(sheetContext).pop(entry),
              ),
          ],
        ),
      ),
    );
    if (glyph == null || !mounted) return;
    final label = await _pickLabel(glyph);
    if (label == null || label.isEmpty || !mounted) return;
    await _applyFix(glyph, label);
  }

  /// Alternatives first, then a free text field: the same ordering as the photo
  /// correction sheet.
  Future<String?> _pickLabel(ScanGlyph glyph) async {
    return showModalBottomSheet<String>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) {
        final controller = TextEditingController(text: glyph.label);
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(16, 4, 16, 16),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text(
                  l.t('scan.fix.title'),
                  style: Theme.of(sheetContext).textTheme.titleSmall,
                ),
                const SizedBox(height: 10),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    for (final alternative in glyph.alternatives)
                      ActionChip(
                        label: Text(alternative.glyph),
                        onPressed: () =>
                            Navigator.of(sheetContext).pop(alternative.glyph),
                      ),
                  ],
                ),
                const SizedBox(height: 12),
                TextField(
                  controller: controller,
                  textDirection: TextDirection.ltr,
                  decoration: InputDecoration(
                    labelText: l.t('scan.fix.type_symbol'),
                    hintText: l.t('scan.fix.type_hint'),
                  ),
                  onSubmitted: (value) => Navigator.of(sheetContext).pop(value),
                ),
                const SizedBox(height: 10),
                FilledButton(
                  onPressed: () => Navigator.of(sheetContext).pop(
                    controller.text.trim(),
                  ),
                  child: Text(l.t('scan.action.use')),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _applyFix(ScanGlyph glyph, String label) async {
    final scanner = _scanner;
    final result = _result;
    if (scanner == null || result == null) return;
    setState(() => _recognizing = true);
    _fixes[glyph.id] = label;
    try {
      final rebuilt = result.session != 0
          ? await scanner.relabel(result.session, _fixes)
          : await scanner.readInk(
              _strokes,
              width: _boardWidth.round(),
              height: _boardHeight.round(),
              overrides: _fixes,
            );
      if (!mounted) return;
      setState(() {
        _result = rebuilt;
        _previewLatex = rebuilt.latex.isEmpty ? rebuilt.expression : rebuilt.latex;
        _recognizing = false;
        _recognizedRevision = _revision;
      });
      await _learnFromFix(glyph, label);
    } catch (_) {
      if (!mounted) return;
      setState(() => _recognizing = false);
    }
  }

  /// The strokes inside the fixed symbol's box become a stroke prototype, so
  /// the next time the user writes it the ink matcher already knows it.
  Future<void> _learnFromFix(ScanGlyph glyph, String label) async {
    final scanner = _scanner;
    if (scanner == null || glyph.box.length < 4) return;
    final left = glyph.box[0].toDouble();
    final top = glyph.box[1].toDouble();
    final right = left + glyph.box[2];
    final bottom = top + glyph.box[3];
    final inside = <List<Offset>>[];
    for (final stroke in _strokes) {
      final hits = stroke
          .where(
            (point) =>
                point.dx >= left - 6 &&
                point.dx <= right + 6 &&
                point.dy >= top - 6 &&
                point.dy <= bottom + 6,
          )
          .length;
      if (stroke.isEmpty) continue;
      if (hits * 2 >= stroke.length) inside.add(stroke);
    }
    if (inside.isEmpty) return;
    if (!scanner.addInkPrototype(
      label,
      inside,
      width: _boardWidth.round(),
      height: _boardHeight.round(),
    )) {
      return;
    }
    final bytes = scanner.exportInkPrototypes();
    if (bytes != null && bytes.isNotEmpty) {
      await PrototypeStore().saveInk(bytes);
    }
  }

  /// Export, import and reset the personal prototype set. The file is the
  /// native prototype format, so it can be carried between devices.
  Future<void> _samplesAction(String action) async {
    final scanner = _scanner;
    if (scanner == null) return;
    try {
      switch (action) {
        case 'export':
          final raster = scanner.exportPrototypes();
          final ink = scanner.exportInkPrototypes();
          if ((raster == null || raster.isEmpty) &&
              (ink == null || ink.isEmpty)) {
            _toast(l.t('scan.samples.empty'));
            return;
          }
          final location = await getSaveLocation(
            suggestedName: 'infinit-samples.bin',
            acceptedTypeGroups: const [
              XTypeGroup(label: 'prototypes', extensions: ['bin']),
            ],
          );
          if (location == null) return;
          if (raster != null && raster.isNotEmpty) {
            await XFile.fromData(
              raster,
              name: 'infinit-samples.bin',
            ).saveTo(location.path);
          }
          // The stroke bank travels next to the raster bank as a companion
          // file: two formats, two files, no container to parse.
          if (ink != null && ink.isNotEmpty) {
            final companion = _companionPath(location.path);
            await XFile.fromData(
              ink,
              name: 'infinit-ink-samples.bin',
            ).saveTo(companion);
          }
          _toast(l.t('scan.samples.saved'));
        case 'import':
          final file = await openFile(
            acceptedTypeGroups: const [
              XTypeGroup(label: 'prototypes', extensions: ['bin']),
            ],
          );
          if (file == null) return;
          final bytes = await file.readAsBytes();
          final rasterOk = scanner.loadPrototypes(bytes);
          if (rasterOk) await PrototypeStore().save(bytes);
          var inkOk = false;
          try {
            final companion = File(_companionPath(file.path));
            if (await companion.exists()) {
              final ink = await companion.readAsBytes();
              inkOk = scanner.loadInkPrototypes(ink);
              if (inkOk) await PrototypeStore().saveInk(ink);
            }
          } catch (_) {
            // A missing companion simply means "no stroke samples in this set".
          }
          if (!rasterOk && !inkOk) {
            _toast(l.t('scan.samples.bad'));
            return;
          }
          if (mounted) setState(() {});
          _toast(l.t('scan.samples.loaded'));
        case 'reset':
          final store = PrototypeStore();
          await store.clear();
          await store.clearInk();
          scanner.loadPrototypes(Uint8List(0));
          scanner.clearInkPrototypes();
          if (mounted) setState(() {});
      }
    } catch (_) {
      _toast(l.t('scan.samples.bad'));
    }
  }

  /// `…/name.bin` becomes `…/name-ink.bin`.
  String _companionPath(String path) {
    final dot = path.lastIndexOf('.');
    if (dot <= path.lastIndexOf(Platform.pathSeparator)) {
      return '$path-ink.bin';
    }
    return '${path.substring(0, dot)}-ink.bin';
  }

  void _toast(String text) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(text)));
  }

  void _eraseAt(Offset at) {
    final hit = <int>[];
    for (var i = 0; i < _strokes.length; i++) {
      for (final point in _strokes[i]) {
        if ((point - at).distance < 26) {
          hit.add(i);
          break;
        }
      }
    }
    if (hit.isEmpty) return;
    setState(() {
      for (final index in hit.reversed) {
        _strokes.removeAt(index);
      }
      ++_revision;
    });
    _scheduleSave();
    _scheduleRecognition();
  }
}

class _InkPainter extends CustomPainter {
  _InkPainter({
    required this.strokes,
    required this.typed,
    required this.preview,
    this.guides = false,
  });

  final List<List<Offset>> strokes;
  final List<({String text, Offset at})> typed;
  final bool preview;

  /// Faint baselines, the plan's optional ruled guide. They are drawn on the
  /// canvas only and never rasterised into the image the recognizer sees.
  final bool guides;

  @override
  void paint(Canvas canvas, Size size) {
    if (guides) {
      final line = Paint()
        ..color = const Color(0x223C6EDB)
        ..strokeWidth = 1.5;
      for (var y = 120.0; y < size.height; y += 170) {
        canvas.drawLine(Offset(0, y), Offset(size.width, y), line);
      }
    }
    final paint = Paint()
      ..color = Colors.black
      ..strokeWidth = 7
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round
      ..style = PaintingStyle.stroke;
    for (final stroke in strokes) {
      if (stroke.length == 1) {
        canvas.drawCircle(stroke.first, 3.5, Paint()..color = Colors.black);
        continue;
      }
      final path = Path()..moveTo(stroke.first.dx, stroke.first.dy);
      for (var i = 1; i < stroke.length; i++) {
        path.lineTo(stroke[i].dx, stroke[i].dy);
      }
      canvas.drawPath(path, paint);
    }
    for (final item in typed) {
      final text = TextPainter(
        text: TextSpan(
          text: item.text,
          style: const TextStyle(
            fontFamily: 'Vazirmatn',
            fontFamilyFallback: [
              'DejaVu Sans',
              'Noto Sans Math',
              'STIX Two Math',
            ],
            fontSize: 44,
            color: Colors.black,
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      text.paint(canvas, item.at);
    }
  }

  @override
  bool shouldRepaint(covariant _InkPainter oldDelegate) => true;
}
