// Calibration (§12A.5): write each symbol once or twice so the personal stroke
// bank knows this writer. Optional and repeatable, like the plan asks: the
// correction sheet already teaches the bank one symbol at a time, and this
// screen is the fast path for a new user.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../scanner_engine.dart';
import '../theme.dart';
import 'prototype_store.dart';

/// The classes the stroke bank can hold, in an order that starts with the
/// characters a student writes most.
const calibrationSymbols = <String>[
  'x', 'y', 'z', 'a', 'b', 'c', 'n', 'm', 't', 'e',
  's', 'u', 'v', 'w', 'd', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'o', 'p', 'q', 'r',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '+', '-', '=', '*', '/', '^', '(', ')', '[', ']',
  '.', ',', '<', '>', '|', '!', "'",
  'sum', 'integral', 'inf', 'pi', 'arrow',
];

class CalibrationPage extends StatefulWidget {
  const CalibrationPage({super.key, required this.l10n});

  final L10n l10n;

  @override
  State<CalibrationPage> createState() => _CalibrationPageState();
}

class _CalibrationPageState extends State<CalibrationPage> {
  static const _boardWidth = 600.0;
  static const _boardHeight = 400.0;

  final List<List<Offset>> _strokes = [];
  int _index = 0;
  int _saved = 0;
  bool _busy = false;

  L10n get l => widget.l10n;

  String get _target => calibrationSymbols[_index];

  void _clear() => setState(_strokes.clear);

  Future<void> _save() async {
    if (_strokes.isEmpty || _busy) return;
    setState(() => _busy = true);
    try {
      final engine = await ScannerEngine.load();
      final ok = engine.addInkPrototype(
        _target,
        _strokes,
        width: _boardWidth.round(),
        height: _boardHeight.round(),
      );
      if (ok) {
        final bytes = engine.exportInkPrototypes();
        if (bytes != null && bytes.isNotEmpty) {
          await PrototypeStore().saveInk(bytes);
        }
        _saved++;
      }
    } finally {
      if (mounted) {
        setState(() {
          _busy = false;
          _strokes.clear();
          _advance();
        });
      }
    }
  }

  void _advance() {
    if (_index + 1 >= calibrationSymbols.length) {
      Navigator.of(context).maybePop();
      return;
    }
    _index++;
  }

  void _skip() => setState(() {
    _strokes.clear();
    _advance();
  });

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final progress = (_index + 1) / calibrationSymbols.length;
    return Scaffold(
      appBar: AppBar(
        title: Text(l.t('scan.calib.title')),
        actions: [
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12),
            child: Center(
              child: Text(
                l.t('scan.calib.progress', {
                  'current': '${_index + 1}',
                  'total': '${calibrationSymbols.length}',
                }),
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 4),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  LinearProgressIndicator(value: progress, minHeight: 3),
                  const SizedBox(height: 10),
                  Text(
                    l.t('scan.calib.hint'),
                    style: Theme.of(
                      context,
                    ).textTheme.bodySmall?.copyWith(color: c.muted),
                  ),
                ],
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
                      // The prompt is drawn in the corner of the board, so the
                      // writer sees the target symbol without a separate header.
                      return FittedBox(
                        fit: BoxFit.contain,
                        child: SizedBox(
                          width: _boardWidth,
                          height: _boardHeight,
                          child: Stack(
                            children: [
                              Positioned(
                                left: 16,
                                top: 8,
                                child: Text(
                                  _calibrationGlyph(_target),
                                  style: TextStyle(
                                    fontFamily: 'Vazirmatn',
                                    fontFamilyFallback: const [
                                      'DejaVu Sans',
                                      'STIX Two Math',
                                      'Noto Sans Math',
                                    ],
                                    fontSize: 64,
                                    color: c.muted.withValues(alpha: 0.35),
                                  ),
                                ),
                              ),
                              GestureDetector(
                                behavior: HitTestBehavior.opaque,
                                onPanStart: (details) => setState(
                                  () => _strokes.add([details.localPosition]),
                                ),
                                onPanUpdate: (details) {
                                  if (_strokes.isEmpty) return;
                                  setState(
                                    () => _strokes.last.add(details.localPosition),
                                  );
                                },
                                child: CustomPaint(
                                  size: const Size(_boardWidth, _boardHeight),
                                  painter: _CalibrationPainter(_strokes),
                                ),
                              ),
                            ],
                          ),
                        ),
                      );
                    },
                  ),
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
              child: Row(
                children: [
                  OutlinedButton.icon(
                    onPressed: _strokes.isEmpty ? null : _clear,
                    icon: const Icon(Icons.backspace_outlined, size: 18),
                    label: Text(l.t('scan.board_clear')),
                  ),
                  const SizedBox(width: 10),
                  OutlinedButton(
                    onPressed: _skip,
                    child: Text(l.t('scan.calib.skip')),
                  ),
                  const Spacer(),
                  FilledButton.icon(
                    onPressed: _strokes.isEmpty || _busy ? null : _save,
                    icon: const Icon(Icons.check_rounded, size: 18),
                    label: Text(
                      _busy ? l.t('scan.calib.saving') : l.t('scan.calib.save'),
                    ),
                  ),
                ],
              ),
            ),
            if (_saved > 0)
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: Text(
                  l.t('scan.calib.saved', {'count': '$_saved'}),
                  style: Theme.of(
                    context,
                  ).textTheme.bodySmall?.copyWith(color: c.muted),
                ),
              ),
          ],
        ),
      ),
    );
  }

  /// How the target is shown to the user: the character itself, or the shape's
  /// name for the structural labels.
  String _calibrationGlyph(String label) {
    switch (label) {
      case 'sum':
        return '∑';
      case 'integral':
        return '∫';
      case 'inf':
        return '∞';
      case 'pi':
        return 'π';
      case 'arrow':
        return '→';
      default:
        return label;
    }
  }
}

class _CalibrationPainter extends CustomPainter {
  _CalibrationPainter(this.strokes);

  final List<List<Offset>> strokes;

  @override
  void paint(Canvas canvas, Size size) {
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
  }

  @override
  bool shouldRepaint(covariant _CalibrationPainter oldDelegate) => true;
}
