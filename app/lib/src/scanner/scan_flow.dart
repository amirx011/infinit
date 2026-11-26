// One scan flow shared by every expression field in the app.
//
// The entry point is a single sheet with the four input paths from the plan:
// camera, gallery, handwriting board and the keyboard. Whatever the source,
// the user gets the same review screen: the picture with per-symbol boxes, a
// rendered LaTeX preview, an editable expression and a "Use" button. Nothing
// is solved here; the caller only receives the confirmed text.
import 'dart:async';
import 'dart:io';
import 'dart:isolate';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:flutter/semantics.dart';
import 'package:flutter/services.dart';
import 'package:flutter_math_fork/flutter_math.dart';
import 'package:image/image.dart' as img;

import '../l10n.dart';
import '../native_engine.dart';
import '../scanner_engine.dart';
import '../theme.dart';
import 'board.dart';
import 'camera.dart';
import 'camera_page.dart';
import 'perspective.dart';
import 'prototype_store.dart';

/// Where the scan came from. Only "Type" never reaches the review screen: it
/// closes the sheet so the caller can focus its own field.
enum ScanSource { camera, gallery, ink, type }

class ScanOutcome {
  const ScanOutcome({
    this.result,
    required this.image,
    required this.text,
    this.retake = false,
    this.manual = false,
  });

  /// Null when the user typed the expression because recognition failed; the
  /// caller only needs `text` in that case.
  final ScanResult? result;

  /// The image the recognizer actually saw, kept for the symbol overlay.
  final Uint8List image;

  /// The confirmed expression: the matrix form for a matrix reading, the
  /// parser syntax otherwise, or whatever the user typed in the editor.
  final String text;

  /// The user asked to retake instead of confirming; the caller reopens the
  /// same source without leaving the scan flow.
  final bool retake;

  /// The text came from the manual editor, not from a recognised reading.
  final bool manual;
}

const _imageTypes = XTypeGroup(
  label: 'images',
  extensions: [
    'png',
    'jpg',
    'jpeg',
    'webp',
    'bmp',
    'gif',
    'tif',
    'tiff',
    // iPhone and some Android cameras hand over HEIC; the platform codec
    // decodes it in normalizeForScanner, so the picker may offer it.
    'heic',
    'heif',
  ],
);

/// Opens the source sheet and, when the user picks a picture or the board,
/// runs the review flow. Returns null on cancel and when the user chooses to
/// type instead.
Future<ScanOutcome?> scanExpression(BuildContext context, {L10n? l10n}) async {
  final text = l10n ?? L10n(Localizations.localeOf(context));
  final source = await showModalBottomSheet<ScanSource>(
    context: context,
    showDragHandle: true,
    isScrollControlled: true,
    builder: (context) => _SourceSheet(l10n: text),
  );
  if (source == null || source == ScanSource.type) return null;

  while (true) {
    if (!context.mounted) return null;
    final bytes = await _acquire(context, source, text);
    if (bytes == null || bytes.isEmpty) return null;
    if (!context.mounted) return null;
    final outcome = await Navigator.of(context).push<ScanOutcome>(
      MaterialPageRoute(
        builder: (_) => ScanFlowPage(
          l10n: text,
          bytes: bytes,
          allowCrop: source != ScanSource.ink,
        ),
      ),
    );
    if (outcome == null || !outcome.retake) return outcome;
    // Retake keeps the user in the flow and returns to the same source.
  }
}

/// One capture for the chosen source: the in-app camera on Android, the
/// system camera elsewhere, a gallery picker, or the handwriting board.
Future<Uint8List?> _acquire(
  BuildContext context,
  ScanSource source,
  L10n text,
) async {
  switch (source) {
    case ScanSource.camera:
      try {
        if (Platform.isAndroid) {
          return await Navigator.of(context).push<Uint8List>(
            MaterialPageRoute(builder: (_) => CameraScanPage(l10n: text)),
          );
        }
        return await capturePhoto();
      } catch (error) {
        if (!context.mounted) return null;
        await _reportCameraError(context, text, error);
        return null;
      }
    case ScanSource.gallery:
      final file = await openFile(acceptedTypeGroups: const [_imageTypes]);
      if (file == null) return null;
      try {
        return await file.readAsBytes();
      } catch (_) {
        if (!context.mounted) return null;
        await showDialog<void>(
          context: context,
          builder: (context) => AlertDialog(
            title: Text(text.t('scan.result.failed')),
            content: Text(text.t('scan.error.image')),
            actions: [
              TextButton(
                onPressed: () => Navigator.of(context).pop(),
                child: Text(text.t('ui.close')),
              ),
            ],
          ),
        );
        return null;
      }
    case ScanSource.ink:
      return Navigator.of(context).push<Uint8List>(
        MaterialPageRoute(builder: (_) => HandwritingBoardPage(l10n: text)),
      );
    case ScanSource.type:
      return null;
  }
}

Future<void> _reportCameraError(BuildContext context, L10n l10n, Object error) {
  final reason = error is CameraUnavailable ? error.reason : '$error';
  return showDialog<void>(
    context: context,
    builder: (context) => AlertDialog(
      title: Text(l10n.t('scan.error.permission')),
      content: Text(reason),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: Text(l10n.t('ui.close')),
        ),
      ],
    ),
  );
}

class _SourceSheet extends StatelessWidget {
  const _SourceSheet({required this.l10n});

  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final options = <(ScanSource, IconData, String)>[
      if (Platform.isAndroid)
        (
          ScanSource.camera,
          Icons.photo_camera_outlined,
          l10n.t('scan.source.camera'),
        ),
      (
        ScanSource.gallery,
        Icons.photo_library_outlined,
        l10n.t('scan.source.gallery'),
      ),
      (ScanSource.ink, Icons.draw_outlined, l10n.t('scan.source.handwrite')),
      (ScanSource.type, Icons.keyboard_outlined, l10n.t('scan.source.type')),
    ];
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 4, 20, 20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              l10n.t('scan.entry_title'),
              style: Theme.of(context).textTheme.titleMedium,
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 4),
            Text(
              l10n.t('scan.entry_hint'),
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: c.muted),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 16),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              alignment: WrapAlignment.center,
              children: [
                for (final (source, icon, label) in options)
                  SizedBox(
                    width: 150,
                    child: _SourceButton(
                      icon: icon,
                      label: label,
                      onTap: () => Navigator.of(context).pop(source),
                    ),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _SourceButton extends StatelessWidget {
  const _SourceButton({
    required this.icon,
    required this.label,
    required this.onTap,
  });

  final IconData icon;
  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return InkWell(
      borderRadius: BorderRadius.circular(16),
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 18),
        decoration: BoxDecoration(
          color: c.cardAlt,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: c.line),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 26, color: c.accent),
            const SizedBox(height: 8),
            Text(
              label,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ],
        ),
      ),
    );
  }
}

enum _Stage { loading, crop, processing, result, failed, manual }

class ScanFlowPage extends StatefulWidget {
  const ScanFlowPage({
    super.key,
    required this.l10n,
    required this.bytes,
    this.allowCrop = true,
    this.debugResult,
  });

  final L10n l10n;
  final Uint8List bytes;

  /// Test-only: open straight on the review screen with this reading, so the
  /// widget tests can cover every result state (valid, needs review, failed,
  /// multi-line with a bad line) without a native scan.
  final ScanResult? debugResult;
  final bool allowCrop;

  @override
  State<ScanFlowPage> createState() => _ScanFlowPageState();
}

class _ScanFlowPageState extends State<ScanFlowPage> {
  final _editor = TextEditingController();
  ScannerEngine? _scanner;
  ui.Image? _preview;
  final _previews = <ui.Image>[];
  int _sourceWidth = 0;
  int _sourceHeight = 0;
  ImageQuality? _quality;
  /// Optional brightness/contrast (§12B.5). Always applied to the upright
  /// original so repeated taps never compound.
  double _brightness = 0;
  double _contrast = 1;
  _Stage _stage = _Stage.loading;
  ScanResult? _result;
  Uint8List? _cropped;

  /// The image the recognizer sees: EXIF-rotated upright by Flutter's decoder
  /// and capped at 1600 px, so the preview and the boxes share one space.
  Uint8List? _upright;
  String? _error;
  bool _busy = false;
  bool _overlay = true;
  bool _edited = false;
  /// LaTeX of the alternative the user picked, if any. The native reading is
  /// still the source of truth; this only changes the preview.
  String? _pickedLatex;

  /// Per-line card state. The native output is immutable; these lists only
  /// decide what the editor receives, so deleting or moving a line never
  /// re-runs recognition.
  List<int> _lineOrder = const [];
  Set<int> _selectedLines = {};

  Timer? _validateTimer;
  String? _parseError;
  bool _validating = false;

  /// Crop rotation in quarter turns. The bytes are re-encoded, so recognition
  /// and the overlay see the same upright image the user is looking at.
  int _cropTurns = 0;

  /// Where the manual editor goes back to when the user cancels it.
  _Stage _manualReturn = _Stage.processing;

  /// Every symbol fix the user has made for the current crop. Keeping the map
  /// means a second correction does not undo the first one: each read sends the
  /// complete set, and the native side re-labels all of them in one pass.
  final _overrides = <int, String>{};

  L10n get l => widget.l10n;

  @override
  void initState() {
    super.initState();
    _prepare();
  }

  @override
  void dispose() {
    _validateTimer?.cancel();
    _editor.dispose();
    for (final image in _previews) {
      image.dispose();
    }
    super.dispose();
  }

  /// Decodes the reading image for the overlay at display resolution. The
  /// preview is never the full sensor image: the phone does not need it and
  /// the memory stays bounded.
  Future<ui.Image> _decodePreview(Uint8List bytes) async {
    final buffer = await ui.ImmutableBuffer.fromUint8List(bytes);
    final descriptor = await ui.ImageDescriptor.encoded(buffer);
    final longest = math.max(descriptor.width, descriptor.height);
    final scale = longest > 1400 ? 1400 / longest : 1.0;
    final codec = await descriptor.instantiateCodec(
      targetWidth: math.max(1, (descriptor.width * scale).round()),
      targetHeight: math.max(1, (descriptor.height * scale).round()),
    );
    final frame = await codec.getNextFrame();
    return frame.image;
  }

  Future<void> _prepare() async {
    try {
      final buffer = await ui.ImmutableBuffer.fromUint8List(widget.bytes);
      final descriptor = await ui.ImageDescriptor.encoded(buffer);
      _sourceWidth = descriptor.width;
      _sourceHeight = descriptor.height;
      if (_sourceWidth < 80 || _sourceHeight < 80) {
        setState(() {
          _error = l.t('scan.error.tiny');
          _stage = _Stage.failed;
        });
        return;
      }
      // The preview never needs the full sensor resolution; 1600 px is the
      // same cap the recognizer uses.
      final longest = math.max(_sourceWidth, _sourceHeight);
      final scale = longest > 1600 ? 1600 / longest : 1.0;
      final codec = await descriptor.instantiateCodec(
        targetWidth: math.max(1, (_sourceWidth * scale).round()),
        targetHeight: math.max(1, (_sourceHeight * scale).round()),
      );
      final frame = await codec.getNextFrame();
      // stb_image on the native side ignores EXIF, while Flutter's decoder
      // applies it. Re-encoding the upright frame keeps the preview, the
      // corner handles and the recognizer on the same pixels.
      final png = await frame.image.toByteData(format: ui.ImageByteFormat.png);
      final upright = png?.buffer.asUint8List();
      final bytes = upright != null && upright.isNotEmpty
          ? upright
          : widget.bytes;
      _upright = bytes;
      _quality = await measureImage(bytes);
      if (!mounted) return;
      _previews.add(frame.image);
      final debug = widget.debugResult;
      setState(() {
        _preview = frame.image;
        _stage = widget.allowCrop ? _Stage.crop : _Stage.processing;
        if (debug != null) {
          // Test-only shortcut: the review screen shows a reading that was
          // handed in instead of one produced by the native scanner.
          _result = debug;
          _cropped = bytes;
          _editor.text = debug.expression;
          _resetLines(debug);
          _stage = _Stage.result;
        }
      });
      if (!widget.allowCrop && widget.debugResult == null) {
        await _recognize(bytes);
      }
    } catch (error) {
      if (!mounted) return;
      // A file the platform codec cannot decode is the user's most likely
      // mistake here, so it gets its own message and a retry path instead of a
      // raw exception string.
      setState(() {
        _error = error is FormatException
            ? l.t('scan.error.image')
            : l.t('ui.scanner_failed', {'error': '$error'});
        _stage = _Stage.failed;
      });
    }
  }

  Future<void> _recognize(Uint8List bytes) async {
    setState(() {
      _stage = _Stage.processing;
      _error = null;
    });
    try {
      final scanner = _scanner ??= await ScannerEngine.load();
      final result = await scanner.read(bytes);
      final preview = await _decodePreview(bytes);
      if (!mounted) return;
      _previews.add(preview);
      _editor.text = result.kind == 'matrix' && result.matrix.isNotEmpty
          ? result.matrix
          : result.expression;
      _overrides
        ..clear()
        ..addAll(result.overrides);
      _resetLines(result);
      _parseError = null;
      _validating = false;
      _validateTimer?.cancel();
      setState(() {
        _preview = preview;
        _cropped = bytes;
        _result = result;
        _edited = false;
        _pickedLatex = null;
        _stage = _Stage.result;
      });
      _announce(result);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _error = l.t('ui.scanner_failed', {'error': '$error'});
        _stage = _Stage.failed;
      });
    }
  }

  Future<void> _correct(ScanGlyph glyph, String label) async {
    final bytes = _cropped;
    final scanner = _scanner;
    final current = _result;
    if (bytes == null || scanner == null || label.isEmpty) return;
    setState(() => _busy = true);
    _overrides[glyph.id] = label;
    try {
      final fixes = Map<int, String>.of(_overrides);
      // A live session rebuilds the reading from the cached pixels; the
      // stateless fallback repeats the vision stages but keeps the same API.
      final result = current != null && current.session != 0
          ? await scanner.relabel(current.session, fixes)
          : await scanner.read(bytes, overrides: fixes);
      if (!mounted) return;
      _editor.text = result.kind == 'matrix' && result.matrix.isNotEmpty
          ? result.matrix
          : result.expression;
      setState(() {
        _result = result;
        _edited = false;
        _busy = false;
      });
      // A correction is also a labelled sample: keep the user's own glyph so
      // handwriting improves without a separate calibration screen.
      await _learnGlyph(glyph, label);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = l.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  /// Stores the corrected symbol as a personal prototype and exports the set.
  Future<void> _learnGlyph(ScanGlyph glyph, String label) async {
    final scanner = _scanner;
    final image = _preview;
    if (scanner == null || image == null || glyph.box.length < 4) return;
    try {
      final png = await _cropGlyphPng(image, glyph.box);
      if (png == null) return;
      if (!scanner.addPrototype(label, png)) return;
      final bytes = scanner.exportPrototypes();
      if (bytes != null && bytes.isNotEmpty) {
        await PrototypeStore().save(bytes);
      }
    } catch (_) {
      // The correction already applied; learning is a bonus.
    }
  }

  /// Crops one symbol with a little padding, exactly the pixels the classifier
  /// scored, and encodes it as PNG for `scanner_add_prototype`.
  Future<Uint8List?> _cropGlyphPng(ui.Image image, List<int> box) async {
    final full = Rect.fromLTWH(
      0,
      0,
      image.width.toDouble(),
      image.height.toDouble(),
    );
    final rect = Rect.fromLTRB(
      box[0].toDouble(),
      box[1].toDouble(),
      box[2].toDouble(),
      box[3].toDouble(),
    ).intersect(full);
    if (rect.width < 3 || rect.height < 3) return null;
    final padded = rect
        .inflate(math.max(2.0, rect.shortestSide * 0.08))
        .intersect(full);
    final width = padded.width.round().clamp(4, 512);
    final height = padded.height.round().clamp(4, 512);
    final recorder = ui.PictureRecorder();
    final canvas = Canvas(recorder);
    canvas.drawImageRect(
      image,
      padded,
      Rect.fromLTWH(0, 0, width.toDouble(), height.toDouble()),
      Paint()..filterQuality = FilterQuality.medium,
    );
    final cropped = await recorder.endRecording().toImage(width, height);
    try {
      final data = await cropped.toByteData(format: ui.ImageByteFormat.png);
      return data?.buffer.asUint8List();
    } finally {
      cropped.dispose();
    }
  }

  /// Replaces the shown result after a native session edit. Symbol ids change
  /// when the segmentation is rebuilt, so the correction map is dropped.
  void _applyResult(ScanResult result) {
    _overrides.clear();
    _resetLines(result);
    _parseError = null;
    _validating = false;
    _validateTimer?.cancel();
    _editor.text = result.kind == 'matrix' && result.matrix.isNotEmpty
        ? result.matrix
        : result.expression;
    setState(() {
      _result = result;
      _edited = false;
      _pickedLatex = null;
      _busy = false;
    });
    _announce(result);
  }

  /// Screen readers get the plain Infinit expression, not LaTeX source.
  void _announce(ScanResult result) {
    if (!mounted) return;
    final spoken = result.expression.isEmpty ? result.latex : result.expression;
    if (spoken.isEmpty) return;
    SemanticsService.sendAnnouncement(
      View.of(context),
      l.t('scan.semantics.recognized', {'expression': spoken}),
      Directionality.of(context),
    );
  }

  Future<void> _merge(int first, int second) async {
    final scanner = _scanner;
    final current = _result;
    if (scanner == null || current == null || current.session == 0) return;
    setState(() => _busy = true);
    try {
      final result = await scanner.mergeSymbols(current.session, first, second);
      if (!mounted) return;
      _applyResult(result);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = l.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  Future<void> _split(int symbol) async {
    final scanner = _scanner;
    final current = _result;
    if (scanner == null || current == null || current.session == 0) return;
    setState(() => _busy = true);
    try {
      final result = await scanner.splitSymbol(current.session, symbol);
      if (!mounted) return;
      _applyResult(result);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = l.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  /// Turns the crop image a quarter clockwise. Rotation is applied to the
  /// encoded bytes, not to a transform, so the four corner handles always map
  /// onto the pixels the recognizer will receive.
  /// Brightness/contrast dialog. Applied on confirm; the crop screen then shows
  /// the adjusted image and the recognizer sees the same pixels.
  Future<void> _adjustCrop() async {
    final result = await showDialog<(double, double)>(
      context: context,
      builder: (dialogContext) {
        var brightness = _brightness;
        var contrast = _contrast;
        return StatefulBuilder(
          builder: (dialogContext, setDialogState) => AlertDialog(
            title: Text(l.t('scan.adjust.title')),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(l.t('scan.adjust.brightness')),
                Slider(
                  value: brightness,
                  min: -60,
                  max: 60,
                  divisions: 24,
                  onChanged: (value) =>
                      setDialogState(() => brightness = value),
                ),
                Text(l.t('scan.adjust.contrast')),
                Slider(
                  value: contrast,
                  min: 0.6,
                  max: 1.8,
                  divisions: 12,
                  onChanged: (value) => setDialogState(() => contrast = value),
                ),
              ],
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.of(dialogContext).pop(),
                child: Text(l.t('ui.cancel')),
              ),
              FilledButton(
                onPressed: () => Navigator.of(
                  dialogContext,
                ).pop((brightness, contrast)),
                child: Text(l.t('scan.action.apply')),
              ),
            ],
          ),
        );
      },
    );
    if (result == null || !mounted) return;
    _brightness = result.$1;
    _contrast = result.$2;
    final source = _upright ?? widget.bytes;
    setState(() => _busy = true);
    try {
      final adjusted = _brightness == 0 && _contrast == 1
          ? source
          : await adjustImage(
              source,
              brightness: _brightness,
              contrast: _contrast,
            );
      _upright = adjusted;
      _quality = await measureImage(adjusted);
      final preview = await _decodePreview(adjusted);
      if (!mounted) return;
      _previews.add(preview);
      setState(() {
        _preview = preview;
        _busy = false;
      });
    } catch (_) {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _rotateCrop() async {
    final source = _upright;
    if (source == null || _busy) return;
    setState(() => _busy = true);
    try {
      final decoded = await Isolate.run(() {
        final image = img.decodeImage(source);
        if (image == null) return null;
        final rotated = img.copyRotate(image, angle: 90);
        return Uint8List.fromList(img.encodeJpg(rotated, quality: 92));
      });
      if (decoded == null) throw const FormatException('image decode failed');
      final preview = await _decodePreview(decoded);
      if (!mounted) {
        preview.dispose();
        return;
      }
      _previews.add(preview);
      setState(() {
        _upright = decoded;
        _preview = preview;
        _cropTurns = (_cropTurns + 1) % 4;
        _busy = false;
      });
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = l.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  Future<void> _cropAgain() async {
    if (!widget.allowCrop || _preview == null) return;
    _overrides.clear();
    _lineOrder = const [];
    _selectedLines = {};
    setState(() {
      _result = null;
      _cropped = null;
      _stage = _Stage.crop;
    });
  }

  /// The editor validates against the same Core parser the repair stage uses.
  /// A short debounce keeps typing responsive and avoids one isolate per key.
  void _onEdited() {
    setState(() => _edited = true);
    if (_result?.kind != 'expression') return;
    _validateTimer?.cancel();
    _validateTimer = Timer(const Duration(milliseconds: 400), _validate);
  }

  Future<void> _validate() async {
    final text = _editor.text.trim();
    if (text.isEmpty) {
      if (mounted) setState(() => _parseError = null);
      return;
    }
    setState(() => _validating = true);
    try {
      final result = await NativeEngine.instance.solveAsync({
        'mode': 'parse',
        'input': text,
        'x': 'x',
        'lang': 'en',
      });
      if (!mounted) return;
      setState(() {
        _parseError = result.ok ? null : result.error;
        _validating = false;
      });
    } catch (_) {
      // Validation is a hint; a failed engine must not block use of the text.
      if (mounted) setState(() => _validating = false);
    }
  }

  /// Plan §12B.7: Solve stays disabled until every selected line is valid and
  /// the editor has had nothing to complain about. A wrong-but-parseable line
  /// is the dangerous case, so the gate is the parser verdict, not the
  /// confidence.
  bool get _canUse {
    if (_busy) return false;
    if (_editor.text.trim().isEmpty) return false;
    if (_parseError != null) return false;
    final result = _result;
    if (result == null) return true;
    for (final index in _selectedLines) {
      if (index < 0 || index >= result.lineDetails.length) continue;
      if (result.lineDetails[index].parseValid == false) return false;
    }
    return true;
  }

  void _resetLines(ScanResult result) {
    _lineOrder = [for (var i = 0; i < result.lineDetails.length; i++) i];
    _selectedLines = {for (var i = 0; i < result.lineDetails.length; i++) i};
  }

  /// Rebuilds the editor from the selected cards, in card order. Joining with a
  /// newline keeps the workbenches' multi-line parser happy; a single line
  /// still produces exactly the old text.
  void _syncLines() {
    final details = _result?.lineDetails;
    if (details == null || details.isEmpty) return;
    final text = [
      for (final index in _lineOrder)
        if (_selectedLines.contains(index)) details[index].expression,
    ].where((line) => line.isNotEmpty).join('\n');
    _editor.text = text;
    setState(() => _edited = true);
  }

  void _lineToggle(int index, bool selected) {
    setState(() {
      if (selected) {
        _selectedLines.add(index);
      } else {
        _selectedLines.remove(index);
      }
    });
    _syncLines();
  }

  void _lineMove(int index, int delta) {
    final at = _lineOrder.indexOf(index);
    final target = at + delta;
    if (at < 0 || target < 0 || target >= _lineOrder.length) return;
    setState(() {
      final moved = _lineOrder.removeAt(at);
      _lineOrder.insert(target, moved);
    });
    _syncLines();
  }

  void _lineUse(int index) {
    final details = _result?.lineDetails;
    if (details == null || index < 0 || index >= details.length) return;
    _editor.text = details[index].expression;
    setState(() => _edited = true);
  }

  Future<void> _setRelation(ScanRelation relation, String value) async {
    final scanner = _scanner;
    final current = _result;
    if (scanner == null || current == null || current.session == 0) return;
    setState(() => _busy = true);
    try {
      final result = await scanner.setRelation(
        current.session,
        relation.symbol,
        relation.kind,
        value,
      );
      if (!mounted) return;
      _applyResult(result);
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = l.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  void _retake() {
    final result = _result;
    final image = _cropped ?? widget.bytes;
    if (result == null) return;
    Navigator.of(context).pop(
      ScanOutcome(
        result: result,
        image: image,
        text: _editor.text.trim(),
        retake: true,
      ),
    );
  }

  void _use() {
    final result = _result;
    final image = _cropped ?? widget.bytes;
    if (result == null) return;
    Navigator.of(
      context,
    ).pop(ScanOutcome(result: result, image: image, text: _editor.text.trim()));
  }

  /// Desktop shortcuts from the plan: Esc leaves the flow, Ctrl+Enter confirms
  /// the reviewed reading. Text fields keep Flutter's own Ctrl+Z/Ctrl+Y undo, so
  /// no binding here shadows them.
  Map<ShortcutActivator, VoidCallback> get _shortcuts => {
    const SingleActivator(LogicalKeyboardKey.escape): () {
      if (_stage == _Stage.manual) {
        setState(() => _stage = _result != null ? _Stage.result : _manualReturn);
        return;
      }
      Navigator.of(context).maybePop();
    },
    const SingleActivator(LogicalKeyboardKey.enter, control: true): () {
      if (_stage == _Stage.result && !_busy && _editor.text.trim().isNotEmpty) {
        _use();
      }
    },
  };

  @override
  Widget build(BuildContext context) {
    return CallbackShortcuts(
      bindings: _shortcuts,
      child: Focus(autofocus: true, child: _scaffold(context)),
    );
  }

  Widget _scaffold(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(l.t('scan.title')),
        actions: [
          if (_result != null && widget.allowCrop)
            TextButton(
              onPressed: _cropAgain,
              child: Text(l.t('scan.action.crop_again')),
            ),
          if (_result != null)
            TextButton(
              onPressed: _retake,
              child: Text(l.t('scan.action.retake')),
            ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            if (_error != null && _stage != _Stage.failed)
              _ErrorStrip(message: _error!),
            Expanded(child: _body()),
            if (_stage == _Stage.result)
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
                        onPressed: _canUse ? _use : null,
                        icon: const Icon(Icons.check_rounded, size: 18),
                        label: Text(l.t('scan.action.use')),
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

  Widget _body() {
    final preview = _preview;
    switch (_stage) {
      case _Stage.loading:
        return const Center(child: CircularProgressIndicator());
      case _Stage.crop:
        if (preview == null) return const SizedBox.shrink();
        return CropStage(
          key: ValueKey('crop-$_cropTurns'),
          l10n: l,
          image: preview,
          quality: _quality,
          onRotate: _rotateCrop,
          onAdjust: _adjustCrop,
          onRetake: () => Navigator.of(context).pop(),
          onCancel: () => Navigator.of(context).pop(),
          onConfirm: (corners) async {
            setState(() => _stage = _Stage.processing);
            final cropped = await perspectiveCrop(_upright ?? widget.bytes, [
              for (final corner in corners)
                math.Point<double>(corner.dx, corner.dy),
            ]);
            if (!mounted) return;
            await _recognize(cropped.bytes);
          },
        );
      case _Stage.processing:
        return _ProcessingView(
          l10n: l,
          stage: ScannerEngine.currentStage,
          onCancel: () {
            // Stop the native worker between stages; the isolate result is
            // ignored because the page is gone.
            _scanner?.cancel();
            Navigator.of(context).pop();
          },
          onManual: () {
            _scanner?.cancel();
            setState(() {
              _manualReturn = _Stage.processing;
              _stage = _Stage.manual;
            });
          },
        );
      case _Stage.failed:
        return _FailedView(
          l10n: l,
          message: _error ?? l.t('scan.result.failed'),
          onRetry: () => Navigator.of(context).pop(),
          onCropAgain: _cropAgain,
          onManual: () => setState(() {
            _manualReturn = _Stage.failed;
            _stage = _Stage.manual;
          }),
        );
      case _Stage.manual:
        return _ManualEntry(
          l10n: l,
          initial: _editor.text,
          onCancel: () => setState(() {
            _stage = _result != null ? _Stage.result : _manualReturn;
          }),
          onUse: (text) {
            if (text.isEmpty) return;
            Navigator.of(context).pop(
              ScanOutcome(image: widget.bytes, text: text, manual: true),
            );
          },
        );
      case _Stage.result:
        final image = _preview;
        if (image == null) return const SizedBox.shrink();
        return _ResultImageScope(
          image: image,
          child: _ResultView(
            l10n: l,
            result: _result!,
            editor: _editor,
            busy: _busy,
            overlay: _overlay,
            edited: _edited,
            onToggleOverlay: () => setState(() => _overlay = !_overlay),
            onEdited: _onEdited,
            onCorrect: _correct,
            onMerge: _merge,
            onSplit: _split,
            previewLatex: _pickedLatex ?? _result!.latex,
            onPickAlternative: (alternative) => setState(() {
              _editor.text = alternative.expression;
              _pickedLatex = alternative.latex;
              _edited = true;
            }),
            onRelation: _setRelation,
            lineOrder: _lineOrder,
            selectedLines: _selectedLines,
            onLineToggle: _lineToggle,
            onLineMove: _lineMove,
            onLineUse: _lineUse,
            parseError: _parseError,
            validating: _validating,
          ),
        );
    }
  }
}

class _ErrorStrip extends StatelessWidget {
  const _ErrorStrip({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      width: double.infinity,
      color: c.warningSoft,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      child: Text(message, style: TextStyle(color: c.warning)),
    );
  }
}

class _ProcessingView extends StatefulWidget {
  const _ProcessingView({
    required this.l10n,
    required this.onCancel,
    required this.onManual,
    required this.stage,
  });

  final L10n l10n;
  final VoidCallback onCancel;
  final VoidCallback onManual;

  /// Reads the native stage counter, so the stepper names the stage the
  /// worker is actually inside instead of animating a decorative spinner.
  final int Function() stage;

  @override
  State<_ProcessingView> createState() => _ProcessingViewState();
}

class _ProcessingViewState extends State<_ProcessingView> {
  Timer? _poll;
  int _stage = 0;
  bool _slow = false;
  late final DateTime _started = DateTime.now();

  @override
  void initState() {
    super.initState();
    _tick();
    _poll = Timer.periodic(const Duration(milliseconds: 120), (_) => _tick());
  }

  void _tick() {
    final next = widget.stage();
    final slow = DateTime.now().difference(_started) >
        const Duration(seconds: 4);
    if (!mounted) return;
    if (next == _stage && slow == _slow) return;
    setState(() {
      _stage = next;
      _slow = slow;
    });
  }

  @override
  void dispose() {
    _poll?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final l = widget.l10n;
    final stages = [
      l.t('scan.stage.prepare'),
      l.t('scan.stage.symbols'),
      l.t('scan.stage.recognize'),
      l.t('scan.stage.check'),
    ];
    // Native stages 1..6 map onto the four labels the plan names; the repair
    // pass is part of checking the expression, structure is part of reading it.
    const stageOf = [0, 0, 1, 2, 2, 3, 4];
    final active = _stage >= 0 && _stage < stageOf.length
        ? stageOf[_stage]
        : 0;
    return Center(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 340),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const LinearProgressIndicator(minHeight: 3),
            const SizedBox(height: 20),
            for (var i = 0; i < stages.length; i++)
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 5),
                child: Row(
                  children: [
                    Icon(
                      i < active
                          ? Icons.check_circle
                          : i == active
                          ? Icons.radio_button_checked
                          : Icons.circle,
                      size: i == active ? 14 : 10,
                      color: i <= active ? c.accent : c.muted,
                    ),
                    const SizedBox(width: 10),
                    Text(
                      stages[i],
                      style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                        color: i == active ? c.text : c.muted,
                        fontWeight: i == active ? FontWeight.w600 : null,
                      ),
                    ),
                  ],
                ),
              ),
            if (_slow) ...[
              const SizedBox(height: 14),
              Row(
                children: [
                  Icon(Icons.timer_outlined, size: 16, color: c.warning),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      l.t('scan.processing.slow'),
                      style: Theme.of(context).textTheme.bodySmall
                          ?.copyWith(color: c.warning),
                    ),
                  ),
                ],
              ),
            ],
            const SizedBox(height: 14),
            Wrap(
              spacing: 8,
              alignment: WrapAlignment.center,
              children: [
                TextButton(onPressed: widget.onCancel, child: Text(l.t('ui.cancel'))),
                TextButton(
                  onPressed: widget.onManual,
                  child: Text(l.t('scan.action.manual')),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

/// Manual entry: the last resort when recognition fails, and the fastest path
/// when the user already knows the expression. It validates with the same Core
/// parser, so a typo is caught before the field receives the text.
class _ManualEntry extends StatefulWidget {
  const _ManualEntry({
    required this.l10n,
    required this.initial,
    required this.onCancel,
    required this.onUse,
  });

  final L10n l10n;
  final String initial;
  final VoidCallback onCancel;
  final void Function(String text) onUse;

  @override
  State<_ManualEntry> createState() => _ManualEntryState();
}

class _ManualEntryState extends State<_ManualEntry> {
  late final TextEditingController _controller = TextEditingController(
    text: widget.initial,
  );
  Timer? _timer;
  String? _error;
  bool _validating = false;

  @override
  void dispose() {
    _timer?.cancel();
    _controller.dispose();
    super.dispose();
  }

  void _check() {
    _timer?.cancel();
    final text = _controller.text.trim();
    if (text.isEmpty) {
      setState(() => _error = null);
      return;
    }
    _timer = Timer(const Duration(milliseconds: 400), () async {
      setState(() => _validating = true);
      try {
        final result = await NativeEngine.instance.solveAsync({
          'mode': 'parse',
          'input': text,
          'x': 'x',
          'lang': 'en',
        });
        if (!mounted) return;
        setState(() {
          _error = result.ok ? null : result.error;
          _validating = false;
        });
      } catch (_) {
        if (mounted) setState(() => _validating = false);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Padding(
      padding: const EdgeInsets.fromLTRB(20, 20, 20, 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            widget.l10n.t('scan.manual.title'),
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 6),
          Text(
            widget.l10n.t('scan.manual.hint'),
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: c.muted),
          ),
          const SizedBox(height: 16),
          Directionality(
            textDirection: TextDirection.ltr,
            child: TextField(
              controller: _controller,
              autofocus: true,
              textInputAction: TextInputAction.done,
              onChanged: (_) => _check(),
              onSubmitted: (text) => widget.onUse(text.trim()),
              style: const TextStyle(fontSize: 18),
              decoration: const InputDecoration(
                border: OutlineInputBorder(),
                isDense: true,
              ),
            ),
          ),
          const SizedBox(height: 8),
          if (_validating)
            Text(
              widget.l10n.t('scan.stage.check'),
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: c.muted),
            )
          else if (_error != null)
            Text(
              _error!,
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: c.warning),
            ),
          const Spacer(),
          Row(
            children: [
              Expanded(
                child: OutlinedButton(
                  onPressed: widget.onCancel,
                  child: Text(widget.l10n.t('ui.cancel')),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                flex: 2,
                child: FilledButton.icon(
                  onPressed: () => widget.onUse(_controller.text.trim()),
                  icon: const Icon(Icons.check_rounded, size: 18),
                  label: Text(widget.l10n.t('scan.action.use')),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _FailedView extends StatelessWidget {
  const _FailedView({
    required this.l10n,
    required this.message,
    required this.onRetry,
    required this.onCropAgain,
    required this.onManual,
  });

  final L10n l10n;
  final String message;
  final VoidCallback onRetry;
  final VoidCallback onCropAgain;
  final VoidCallback onManual;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.error_outline_rounded, size: 40, color: c.warning),
            const SizedBox(height: 12),
            Text(
              l10n.t('scan.result.failed'),
              style: Theme.of(context).textTheme.titleMedium,
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 6),
            Text(
              message,
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: c.muted),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 18),
            Wrap(
              spacing: 10,
              runSpacing: 8,
              alignment: WrapAlignment.center,
              children: [
                OutlinedButton(
                  onPressed: onCropAgain,
                  child: Text(l10n.t('scan.action.crop_again')),
                ),
                FilledButton(
                  onPressed: onRetry,
                  child: Text(l10n.t('ui.back')),
                ),
                TextButton(
                  onPressed: onManual,
                  child: Text(l10n.t('scan.action.manual')),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

/// Four-corner crop. The image is fitted with BoxFit.contain; the handles live
/// in normalized coordinates so they survive rotation and different screens.
class CropStage extends StatefulWidget {
  const CropStage({
    super.key,
    required this.l10n,
    required this.image,
    required this.onCancel,
    required this.onConfirm,
    required this.onRotate,
    required this.onRetake,
    required this.onAdjust,
    this.quality,
  });

  final L10n l10n;
  final ui.Image image;
  final ImageQuality? quality;
  final VoidCallback onCancel;
  final VoidCallback onRotate;
  final VoidCallback onRetake;
  final VoidCallback onAdjust;
  final Future<void> Function(List<Offset> corners) onConfirm;

  @override
  State<CropStage> createState() => _CropStageState();
}

class _CropStageState extends State<CropStage> {
  static const _defaultCorners = <Offset>[
    Offset(0.05, 0.08),
    Offset(0.95, 0.06),
    Offset(0.96, 0.94),
    Offset(0.04, 0.92),
  ];

  var _corners = List<Offset>.of(_defaultCorners);
  int _dragging = -1;
  Offset? _loupe;
  bool _busy = false;
  bool _hintsDismissed = false;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final quality = widget.quality;
    final hints = <String>[
      if (quality != null && quality.isBlurry) widget.l10n.t('scan.hint.blur'),
      if (quality != null && quality.isDark) widget.l10n.t('scan.hint.dark'),
      if (quality != null && quality.isSmall) widget.l10n.t('scan.hint.small'),
    ];
    return Column(
      children: [
        if (hints.isNotEmpty && !_hintsDismissed)
          Container(
            width: double.infinity,
            color: c.warningSoft,
            padding: const EdgeInsets.fromLTRB(16, 8, 8, 8),
            child: Row(
              children: [
                Icon(Icons.info_outline, size: 16, color: c.warning),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    hints.join(' · '),
                    style: Theme.of(context).textTheme.bodySmall
                        ?.copyWith(color: c.warning),
                  ),
                ),
                TextButton(
                  onPressed: widget.onRetake,
                  child: Text(widget.l10n.t('scan.action.retake')),
                ),
                TextButton(
                  onPressed: () => setState(() => _hintsDismissed = true),
                  child: Text(widget.l10n.t('scan.action.continue')),
                ),
              ],
            ),
          ),
        Expanded(
          child: Padding(
            padding: const EdgeInsets.all(12),
            child: LayoutBuilder(
              builder: (context, constraints) {
                final imageAspect = widget.image.width / widget.image.height;
                var width = constraints.maxWidth;
                var height = width / imageAspect;
                if (height > constraints.maxHeight) {
                  height = constraints.maxHeight;
                  width = height * imageAspect;
                }
                final left = (constraints.maxWidth - width) / 2;
                final top = (constraints.maxHeight - height) / 2;
                final rect = Rect.fromLTWH(left, top, width, height);

                Offset toPixels(Offset normalized) => Offset(
                  rect.left + normalized.dx * rect.width,
                  rect.top + normalized.dy * rect.height,
                );

                void moveTo(int index, Offset pixels) {
                  final dx = ((pixels.dx - rect.left) / rect.width).clamp(
                    0.0,
                    1.0,
                  );
                  final dy = ((pixels.dy - rect.top) / rect.height).clamp(
                    0.0,
                    1.0,
                  );
                  setState(() => _corners[index] = Offset(dx, dy));
                }

                return GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onPanStart: (details) {
                    var best = -1;
                    var bestDistance = 44.0;
                    for (var i = 0; i < _corners.length; i++) {
                      final distance =
                          (toPixels(_corners[i]) - details.localPosition)
                              .distance;
                      if (distance < bestDistance) {
                        bestDistance = distance;
                        best = i;
                      }
                    }
                    _dragging = best;
                    if (best >= 0) {
                      setState(() => _loupe = details.localPosition);
                    }
                  },
                  onPanUpdate: (details) {
                    if (_dragging < 0) return;
                    moveTo(_dragging, details.localPosition);
                    setState(() => _loupe = details.localPosition);
                  },
                  onPanEnd: (_) {
                    _dragging = -1;
                    setState(() => _loupe = null);
                  },
                  child: Stack(
                    children: [
                      Positioned.fromRect(
                        rect: rect,
                        child: RawImage(image: widget.image, fit: BoxFit.fill),
                      ),
                      Positioned.fill(
                        child: CustomPaint(
                          painter: _CropPainter(
                            rect: rect,
                            corners: _corners,
                            dim: Colors.black.withValues(alpha: 0.45),
                            accent: c.accent,
                            image: widget.image,
                            loupe: _loupe,
                          ),
                        ),
                      ),
                    ],
                  ),
                );
              },
            ),
          ),
        ),
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
          child: Text(
            widget.l10n.t('scan.crop_hint'),
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(color: c.muted),
          ),
        ),
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
          child: Row(
            children: [
              OutlinedButton(
                onPressed: widget.onCancel,
                child: Text(widget.l10n.t('ui.cancel')),
              ),
              const SizedBox(width: 10),
              IconButton(
                tooltip: widget.l10n.t('scan.action.rotate'),
                onPressed: _busy ? null : widget.onRotate,
                icon: const Icon(Icons.rotate_90_degrees_cw_outlined),
              ),
              IconButton(
                tooltip: widget.l10n.t('scan.action.adjust'),
                onPressed: _busy ? null : widget.onAdjust,
                icon: const Icon(Icons.tune_rounded),
              ),
              IconButton(
                tooltip: widget.l10n.t('scan.action.reset'),
                onPressed: () =>
                    setState(() => _corners = List<Offset>.of(_defaultCorners)),
                icon: const Icon(Icons.crop_free_rounded),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: FilledButton.icon(
                  onPressed: _busy
                      ? null
                      : () async {
                          setState(() => _busy = true);
                          await widget.onConfirm(List<Offset>.of(_corners));
                          if (mounted) setState(() => _busy = false);
                        },
                  icon: _busy
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.document_scanner_outlined, size: 18),
                  label: Text(widget.l10n.t('scan.action.recognize')),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _CropPainter extends CustomPainter {
  _CropPainter({
    required this.rect,
    required this.corners,
    required this.dim,
    required this.accent,
    required this.image,
    required this.loupe,
  });

  final Rect rect;
  final List<Offset> corners;
  final Color dim;
  final Color accent;
  final ui.Image image;

  /// Finger position while a corner is dragged. A 2.5x loupe is drawn above it
  /// so the corner can be placed on the glyph edge, not near it.
  final Offset? loupe;

  @override
  void paint(Canvas canvas, Size size) {
    final points = [
      for (final corner in corners)
        Offset(
          rect.left + corner.dx * rect.width,
          rect.top + corner.dy * rect.height,
        ),
    ];
    final quad = Path()..addPolygon(points, true);
    final outside = Path.combine(
      PathOperation.difference,
      Path()..addRect(Offset.zero & size),
      quad,
    );
    canvas.drawPath(outside, Paint()..color = dim);
    canvas.drawPath(
      quad,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2
        ..color = accent,
    );
    for (final point in points) {
      canvas.drawCircle(point, 10, Paint()..color = Colors.white);
      canvas.drawCircle(
        point,
        10,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 3
          ..color = accent,
      );
    }

    final focus = loupe;
    if (focus != null) {
      const radius = 58.0;
      const zoom = 2.5;
      var center = focus + const Offset(0, -104);
      center = Offset(
        center.dx.clamp(radius + 4, size.width - radius - 4),
        center.dy.clamp(radius + 4, size.height - radius - 4),
      );
      final lens = Rect.fromCircle(center: center, radius: radius);

      // The finger point in normalized image space, then in source pixels.
      final nx = ((focus.dx - rect.left) / rect.width).clamp(0.0, 1.0);
      final ny = ((focus.dy - rect.top) / rect.height).clamp(0.0, 1.0);
      final sourceHalfWidth =
          radius / zoom * image.width / math.max(1.0, rect.width);
      final sourceHalfHeight =
          radius / zoom * image.height / math.max(1.0, rect.height);
      final sourceCenter = Offset(
        (nx * image.width).clamp(
          sourceHalfWidth,
          math.max(sourceHalfWidth, image.width - sourceHalfWidth),
        ),
        (ny * image.height).clamp(
          sourceHalfHeight,
          math.max(sourceHalfHeight, image.height - sourceHalfHeight),
        ),
      );
      final source = Rect.fromCenter(
        center: sourceCenter,
        width: math.min(image.width.toDouble(), sourceHalfWidth * 2),
        height: math.min(image.height.toDouble(), sourceHalfHeight * 2),
      );

      canvas.save();
      canvas.clipPath(Path()..addOval(lens));
      canvas.drawImageRect(
        image,
        source,
        lens,
        Paint()..filterQuality = FilterQuality.medium,
      );
      canvas.restore();
      canvas.drawCircle(
        center,
        radius,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 3
          ..color = Colors.white,
      );
      canvas.drawCircle(
        center,
        radius - 2,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.5
          ..color = accent,
      );
      canvas.drawCircle(
        center,
        2,
        Paint()..color = accent,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _CropPainter oldDelegate) =>
      oldDelegate.corners != corners ||
      oldDelegate.rect != rect ||
      oldDelegate.accent != accent ||
      oldDelegate.loupe != loupe;
}

class _ResultView extends StatelessWidget {
  const _ResultView({
    required this.l10n,
    required this.result,
    required this.editor,
    required this.busy,
    required this.overlay,
    required this.edited,
    required this.onToggleOverlay,
    required this.onEdited,
    required this.onCorrect,
    required this.onMerge,
    required this.onSplit,
    required this.previewLatex,
    required this.onPickAlternative,
    required this.onRelation,
    required this.lineOrder,
    required this.selectedLines,
    required this.onLineToggle,
    required this.onLineMove,
    required this.onLineUse,
    required this.parseError,
    required this.validating,
  });

  final L10n l10n;
  final ScanResult result;
  final TextEditingController editor;
  final bool busy;
  final bool overlay;
  final bool edited;
  final VoidCallback onToggleOverlay;
  final VoidCallback onEdited;
  final Future<void> Function(ScanGlyph glyph, String label) onCorrect;
  final void Function(int first, int second) onMerge;
  final void Function(int symbol) onSplit;
  final String previewLatex;
  final void Function(ScanReading alternative) onPickAlternative;
  final Future<void> Function(ScanRelation relation, String value) onRelation;
  final List<int> lineOrder;
  final Set<int> selectedLines;
  final void Function(int index, bool selected) onLineToggle;
  final void Function(int index, int delta) onLineMove;
  final void Function(int index) onLineUse;
  final String? parseError;
  final bool validating;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final valid = result.confidence >= 0.80 && !edited;
    final failed = result.expression.isEmpty;
    final status = failed
        ? l10n.t('scan.result.failed')
        : valid
        ? l10n.t('scan.result.valid')
        : l10n.t('scan.result.review');
    final tone = failed
        ? c.warning
        : valid
        ? c.success
        : c.accentAlt;
    final multiLine =
        result.kind == 'expression' && result.lineDetails.length > 1;
    // Wide windows (tablet, desktop, phone landscape) get the two-pane layout
    // the plan asks for: the source image stays visible while the reading is
    // edited. Narrow windows keep the single scrolling column.
    return LayoutBuilder(
      builder: (context, constraints) {
        final wide = constraints.maxWidth >= 900;
        final source = Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _statusRow(context, status, tone, overlay, busy),
            const SizedBox(height: 8),
            if (wide)
              Expanded(child: _imageBlock(context))
            else
              _imageBlock(context),
          ],
        );
        if (!wide) {
          return SingleChildScrollView(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                source,
                ..._details(context, multiLine),
              ],
            ),
          );
        }
        return Row(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              flex: 3,
              child: Padding(
                padding: const EdgeInsets.fromLTRB(16, 8, 8, 8),
                child: source,
              ),
            ),
            VerticalDivider(width: 1, color: c.line),
            Expanded(
              flex: 2,
              child: SingleChildScrollView(
                padding: const EdgeInsets.fromLTRB(8, 8, 16, 8),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: _details(context, multiLine),
                ),
              ),
            ),
          ],
        );
      },
    );
  }

  Widget _statusRow(
    BuildContext context,
    String status,
    Color tone,
    bool overlay,
    bool busy,
  ) {
    return Row(
      children: [
        _StatusChip(text: status, tone: tone),
        const Spacer(),
        IconButton(
          tooltip: l10n.t('scan.action.overlay'),
          isSelected: overlay,
          icon: const Icon(Icons.select_all_rounded),
          onPressed: onToggleOverlay,
        ),
        if (busy)
          const SizedBox(
            width: 18,
            height: 18,
            child: CircularProgressIndicator(strokeWidth: 2),
          ),
      ],
    );
  }

  /// The cropped source with the symbol overlay, fitted to whatever box the
  /// layout gives it.
  Widget _imageBlock(BuildContext context) {
    return _OverlayImage(
      l10n: l10n,
      result: result,
      showOverlay: overlay,
      onGlyph: (glyph) => _showGlyphSheet(context, glyph),
      onGlyphMenu: (glyph) => _showGlyphMenu(context, glyph),
    );
  }

  /// Everything below the source image: relations, rendered math, the editor,
  /// the per-line cards and the alternative readings.
  List<Widget> _details(BuildContext context, bool multiLine) {
    final c = context.colors;
    return [
      if (result.relations.isNotEmpty) ...[
        const SizedBox(height: 10),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            for (final relation in result.relations)
              ActionChip(
                avatar: Icon(
                  relation.kind == 'bar'
                      ? Icons.horizontal_rule_rounded
                      : Icons.superscript_rounded,
                  size: 16,
                ),
                label: Text(_relationLabel(relation)),
                onPressed: () => _showRelationMenu(context, relation),
              ),
          ],
        ),
      ],
      const SizedBox(height: 14),
      if (!multiLine && previewLatex.isNotEmpty)
        Container(
          width: double.infinity,
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
          decoration: BoxDecoration(
            color: c.cardAlt,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: c.line),
          ),
          child: Directionality(
            textDirection: TextDirection.ltr,
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: Math.tex(
                previewLatex,
                textStyle: TextStyle(fontSize: 20, color: c.text),
                onErrorFallback: (_) => Text(
                  result.expression,
                  style: const TextStyle(
                    fontFamily: 'DejaVu Sans Mono',
                    fontFamilyFallback: ['monospace', 'Vazirmatn'],
                    fontSize: 15,
                  ),
                ),
              ),
            ),
          ),
        ),
      if (multiLine) ...[
        Text(
          l10n.t('scan.result.lines'),
          style: Theme.of(context).textTheme.titleSmall,
        ),
        const SizedBox(height: 8),
        for (final index in lineOrder) _lineCard(context, index),
      ],
      if (result.alternatives.isNotEmpty) ...[
        const SizedBox(height: 4),
        Theme(
          data: Theme.of(context).copyWith(dividerColor: Colors.transparent),
          child: ExpansionTile(
            tilePadding: EdgeInsets.zero,
            childrenPadding: EdgeInsets.zero,
            title: Text(
              l10n.t('scan.result.other_readings'),
              style: Theme.of(
                context,
              ).textTheme.bodyMedium?.copyWith(color: c.accent),
            ),
            children: [
              for (final alternative in result.alternatives)
                ListTile(
                  dense: true,
                  contentPadding: EdgeInsets.zero,
                  title: Directionality(
                    textDirection: TextDirection.ltr,
                    child: Text(
                      alternative.expression,
                      style: const TextStyle(
                        fontFamily: 'DejaVu Sans Mono',
                        fontFamilyFallback: ['monospace', 'Vazirmatn'],
                        fontSize: 15,
                      ),
                    ),
                  ),
                  trailing: alternative.valid
                      ? Icon(
                          Icons.check_circle_outline_rounded,
                          size: 18,
                          color: c.success,
                        )
                      : null,
                  onTap: () => onPickAlternative(alternative),
                ),
            ],
          ),
        ),
      ],
      const SizedBox(height: 12),
      TextField(
        controller: editor,
        textDirection: TextDirection.ltr,
        textAlign: TextAlign.left,
        minLines: 1,
        maxLines: 3,
        onChanged: (_) => onEdited(),
        style: const TextStyle(
          fontFamily: 'DejaVu Sans Mono',
          fontFamilyFallback: ['monospace', 'Vazirmatn'],
          fontSize: 16,
        ),
        decoration: InputDecoration(
          labelText: l10n.t('ui.expression'),
          hintText: l10n.t('scan.result.editor_hint'),
          prefixIcon: Icon(Icons.edit_outlined, size: 20, color: c.muted),
          suffixIcon: IconButton(
            tooltip: l10n.t('scan.action.fix_symbols'),
            icon: const Icon(Icons.spellcheck_rounded, size: 20),
            onPressed: result.glyphs.isEmpty
                ? null
                : () => _showGlyphList(context),
          ),
        ),
      ),
      if (validating)
        Padding(
          padding: const EdgeInsets.only(top: 6),
          child: Text(
            l10n.t('scan.stage.check'),
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: c.muted),
          ),
        )
      else if (parseError != null)
        Padding(
          padding: const EdgeInsets.only(top: 6),
          child: Text(
            l10n.t('scan.line.invalid'),
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: c.warning),
          ),
        ),
      const SizedBox(height: 8),
      Row(
        children: [
          Icon(Icons.info_outline_rounded, size: 15, color: c.muted),
          const SizedBox(width: 6),
          Expanded(
            child: Text(
              l10n.t('scan.result.warning'),
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: c.muted),
            ),
          ),
        ],
      ),
      if (result.kind == 'matrix') ...[
        const SizedBox(height: 8),
        Text(
          l10n.t('scan.result.matrix_shape', {
            'rows': '${result.rows}',
            'columns': '${result.columns}',
          }),
          style: Theme.of(
            context,
          ).textTheme.bodySmall?.copyWith(color: c.muted),
        ),
      ],
    ];
  }

  String _relationLabel(ScanRelation relation) {
    final value = switch (relation.value) {
      'fraction' => l10n.t('scan.relation.fraction'),
      'minus' => l10n.t('scan.relation.minus'),
      'superscript' => l10n.t('scan.relation.superscript'),
      'inline' => l10n.t('scan.relation.inline'),
      _ => l10n.t('scan.relation.auto'),
    };
    return '${l10n.t(relation.kind == 'bar' ? 'scan.relation.bar' : 'scan.relation.script')}: $value';
  }

  Future<void> _showRelationMenu(
    BuildContext context,
    ScanRelation relation,
  ) async {
    final options = relation.kind == 'bar'
        ? const ['auto', 'fraction', 'minus']
        : const ['auto', 'superscript', 'inline'];
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              title: Text(l10n.t('scan.relation.title')),
              subtitle: Text(l10n.t('scan.semantics.symbol', {
                'glyph': '${relation.symbol}',
              })),
            ),
            for (final value in options)
              ListTile(
                leading: Icon(
                  value == relation.value
                      ? Icons.radio_button_checked_rounded
                      : Icons.radio_button_unchecked_rounded,
                ),
                title: Text(
                  _relationLabel(ScanRelation(
                    symbol: relation.symbol,
                    kind: relation.kind,
                    value: value,
                  )).split(': ').last,
                ),
                onTap: () {
                  Navigator.of(sheetContext).pop();
                  onRelation(relation, value);
                },
              ),
          ],
        ),
      ),
    );
  }

  Widget _lineCard(BuildContext context, int index) {
    final c = context.colors;
    final line = result.lineDetails[index];
    final selected = selectedLines.contains(index);
    final invalid = line.parseValid == false;
    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.fromLTRB(6, 4, 6, 10),
      decoration: BoxDecoration(
        color: c.cardAlt,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: invalid ? c.warning.withValues(alpha: 0.7) : c.line,
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Checkbox(
                value: selected,
                onChanged: (value) => onLineToggle(index, value ?? false),
              ),
              Text(
                l10n.t('scan.line.card', {'n': '${index + 1}'}),
                style: Theme.of(context).textTheme.labelLarge,
              ),
              if (invalid) ...[
                const SizedBox(width: 6),
                Icon(Icons.error_outline_rounded,
                    size: 16, color: c.warning),
              ],
              const Spacer(),
              IconButton(
                tooltip: l10n.t('scan.line.up'),
                visualDensity: VisualDensity.compact,
                icon: const Icon(Icons.arrow_upward_rounded, size: 18),
                onPressed: () => onLineMove(index, -1),
              ),
              IconButton(
                tooltip: l10n.t('scan.line.down'),
                visualDensity: VisualDensity.compact,
                icon: const Icon(Icons.arrow_downward_rounded, size: 18),
                onPressed: () => onLineMove(index, 1),
              ),
              IconButton(
                tooltip: l10n.t('scan.line.use'),
                visualDensity: VisualDensity.compact,
                icon: const Icon(Icons.edit_outlined, size: 18),
                onPressed: () => onLineUse(index),
              ),
              IconButton(
                tooltip: l10n.t('scan.line.delete'),
                visualDensity: VisualDensity.compact,
                icon: const Icon(Icons.close_rounded, size: 18),
                onPressed: () => onLineToggle(index, false),
              ),
            ],
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 8),
            child: Directionality(
              textDirection: TextDirection.ltr,
              child: line.latex.isEmpty
                  ? Text(
                      line.expression,
                      style: const TextStyle(
                        fontFamily: 'DejaVu Sans Mono',
                        fontFamilyFallback: ['monospace', 'Vazirmatn'],
                        fontSize: 15,
                      ),
                    )
                  : Math.tex(
                      line.latex,
                      textStyle: TextStyle(fontSize: 17, color: c.text),
                      onErrorFallback: (_) => Text(line.expression),
                    ),
            ),
          ),
          if (invalid) ...[
            const SizedBox(height: 4),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8),
              child: Text(
                l10n.t('scan.line.invalid'),
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(color: c.warning),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Future<void> _showGlyphList(BuildContext context) {
    return showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (context) => _GlyphSheet(
        l10n: l10n,
        glyphs: result.glyphs,
        selected: null,
        onPick: (glyph, label) {
          Navigator.of(context).pop();
          onCorrect(glyph, label);
        },
      ),
    );
  }

  Future<void> _showGlyphSheet(BuildContext context, ScanGlyph glyph) {
    return showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (context) => _GlyphSheet(
        l10n: l10n,
        glyphs: [glyph],
        selected: glyph.id,
        onPick: (glyph, label) {
          Navigator.of(context).pop();
          onCorrect(glyph, label);
        },
      ),
    );
  }

  /// Nearest glyph to the right on the same line, used by Merge. Two symbols
  /// belong to one line when their vertical overlap covers half of the shorter
  /// box.
  ScanGlyph? _rightNeighbour(ScanGlyph glyph) {
    if (glyph.box.length < 4) return null;
    final glyphY0 = glyph.box[1];
    final glyphY1 = glyph.box[3];
    final glyphX0 = glyph.box[0];
    ScanGlyph? best;
    for (final other in result.glyphs) {
      if (other.id == glyph.id || other.box.length < 4) continue;
      if (other.box[0] <= glyphX0) continue;
      final overlap =
          (glyphY1 < other.box[3] ? glyphY1 : other.box[3]) -
          (glyphY0 > other.box[1] ? glyphY0 : other.box[1]);
      final shorter = (glyphY1 - glyphY0) < (other.box[3] - other.box[1])
          ? glyphY1 - glyphY0
          : other.box[3] - other.box[1];
      if (shorter <= 0 || overlap < 0.5 * shorter) continue;
      if (best == null || other.box[0] < best.box[0]) best = other;
    }
    return best;
  }

  Future<void> _showGlyphMenu(BuildContext context, ScanGlyph glyph) async {
    final neighbour = _rightNeighbour(glyph);
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (sheetContext) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              leading: const Icon(Icons.merge_rounded),
              title: Text(l10n.t('scan.action.merge')),
              enabled: neighbour != null,
              onTap: neighbour == null
                  ? null
                  : () {
                      Navigator.of(sheetContext).pop();
                      onMerge(glyph.id, neighbour.id);
                    },
            ),
            ListTile(
              leading: const Icon(Icons.call_split_rounded),
              title: Text(l10n.t('scan.action.split')),
              onTap: () {
                Navigator.of(sheetContext).pop();
                onSplit(glyph.id);
              },
            ),
            ListTile(
              leading: const Icon(Icons.edit_rounded),
              title: Text(l10n.t('scan.action.replace')),
              onTap: () {
                Navigator.of(sheetContext).pop();
                _showGlyphSheet(context, glyph);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _StatusChip extends StatelessWidget {
  const _StatusChip({required this.text, required this.tone});

  final String text;
  final Color tone;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: tone.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: tone.withValues(alpha: 0.35)),
      ),
      child: Text(
        text,
        style: Theme.of(context).textTheme.labelMedium
            ?.copyWith(color: tone, fontWeight: FontWeight.w600),
      ),
    );
  }
}

/// The crop with the per-symbol boxes drawn on top, scaled the same way the
/// image is scaled. Tapping a box opens its alternatives.
class _OverlayImage extends StatelessWidget {
  const _OverlayImage({
    required this.l10n,
    required this.result,
    required this.showOverlay,
    required this.onGlyph,
    required this.onGlyphMenu,
  });

  final L10n l10n;
  final ScanResult result;
  final bool showOverlay;
  final void Function(ScanGlyph glyph) onGlyph;
  final void Function(ScanGlyph glyph) onGlyphMenu;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final image = _ResultImageScope.of(context);
    if (image == null) return const SizedBox.shrink();
    return LayoutBuilder(
      builder: (context, constraints) {
        final aspect = result.width > 0 && result.height > 0
            ? result.width / result.height
            : 1.0;
        var width = constraints.maxWidth;
        var height = width / aspect;
        final maxHeight = MediaQuery.sizeOf(context).height * 0.32;
        if (height > maxHeight) {
          height = maxHeight;
          width = height * aspect;
        }
        final scaleX = result.width == 0 ? 1.0 : width / result.width;
        final scaleY = result.height == 0 ? 1.0 : height / result.height;
        return Center(
          child: SizedBox(
            width: width,
            height: height,
            child: GestureDetector(
              onTapUp: (details) {
                if (!showOverlay) return;
                final glyph = _hitGlyph(details.localPosition, scaleX, scaleY);
                if (glyph != null) onGlyph(glyph);
              },
              onLongPressStart: (details) {
                if (!showOverlay) return;
                final glyph = _hitGlyph(details.localPosition, scaleX, scaleY);
                if (glyph != null) onGlyphMenu(glyph);
              },
              child: Stack(
                fit: StackFit.expand,
                children: [
                  ClipRRect(
                    borderRadius: BorderRadius.circular(14),
                    child: RawImage(image: image, fit: BoxFit.fill),
                  ),
                  if (showOverlay)
                    for (final glyph in result.glyphs)
                      Positioned.fromRect(
                        rect: _glyphRect(glyph, scaleX, scaleY),
                        child: IgnorePointer(
                          child: Semantics(
                            label: l10n.t('scan.semantics.symbol', {
                              'glyph': glyph.label,
                            }),
                            child: DecoratedBox(
                              decoration: BoxDecoration(
                                border: Border.all(
                                  color: _tone(c, glyph).withValues(alpha: 0.9),
                                  width: glyph.score >= 0.8 ? 1.2 : 2,
                                ),
                                borderRadius: BorderRadius.circular(3),
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
      },
    );
  }

  /// Hit-testing uses a 48 dp minimum target; when two enlarged rects overlap,
  /// the glyph whose box centre is closest wins, so a small dot does not steal
  /// its neighbour's tap.
  ScanGlyph? _hitGlyph(Offset position, double scaleX, double scaleY) {
    ScanGlyph? best;
    var bestDistance = double.infinity;
    for (final glyph in result.glyphs) {
      final rect = _glyphRect(glyph, scaleX, scaleY);
      if (!_hitRect(rect).contains(position)) continue;
      final distance = (position - rect.center).distanceSquared;
      if (distance < bestDistance) {
        bestDistance = distance;
        best = glyph;
      }
    }
    return best;
  }

  Rect _hitRect(Rect rect) {
    var out = rect.inflate(6);
    if (out.width < 48 || out.height < 48) {
      final padX = out.width < 48 ? (48 - out.width) / 2 : 0.0;
      final padY = out.height < 48 ? (48 - out.height) / 2 : 0.0;
      out = out.inflate(padX > padY ? padX : padY);
    }
    return out;
  }

  Rect _glyphRect(ScanGlyph glyph, double scaleX, double scaleY) {
    final box = glyph.box;
    if (box.length < 4) return Rect.zero;
    return Rect.fromLTRB(
      box[0] * scaleX,
      box[1] * scaleY,
      box[2] * scaleX,
      box[3] * scaleY,
    );
  }

  Color _tone(OdeColors c, ScanGlyph glyph) {
    if (glyph.label == '?') return c.warning;
    if (glyph.score >= 0.80) return c.success;
    return c.accentAlt;
  }
}

/// Carries the recognized image down to the overlay without threading it
/// through every constructor.
class _ResultImageScope extends InheritedWidget {
  const _ResultImageScope({required this.image, required super.child});

  final ui.Image image;

  static ui.Image? of(BuildContext context) =>
      context.dependOnInheritedWidgetOfExactType<_ResultImageScope>()?.image;

  @override
  bool updateShouldNotify(_ResultImageScope oldWidget) =>
      oldWidget.image != image;
}

class _GlyphSheet extends StatefulWidget {
  const _GlyphSheet({
    required this.l10n,
    required this.glyphs,
    required this.selected,
    required this.onPick,
  });

  final L10n l10n;
  final List<ScanGlyph> glyphs;
  final int? selected;
  final void Function(ScanGlyph glyph, String label) onPick;

  @override
  State<_GlyphSheet> createState() => _GlyphSheetState();
}

class _GlyphSheetState extends State<_GlyphSheet> {
  final _custom = TextEditingController();
  int? _active;

  @override
  void initState() {
    super.initState();
    _active = widget.selected;
  }

  @override
  void dispose() {
    _custom.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final glyphs = widget.glyphs;
    return SafeArea(
      child: Padding(
        padding: EdgeInsets.fromLTRB(
          16,
          4,
          16,
          MediaQuery.viewInsetsOf(context).bottom + 16,
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              widget.l10n.t('scan.fix.title'),
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 4),
            Text(
              widget.l10n.t('scan.fix.hint'),
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: c.muted),
            ),
            const SizedBox(height: 12),
            ConstrainedBox(
              constraints: const BoxConstraints(maxHeight: 220),
              child: ListView.separated(
                shrinkWrap: true,
                itemCount: glyphs.length,
                separatorBuilder: (context, index) => const SizedBox(height: 8),
                itemBuilder: (context, index) {
                  final glyph = glyphs[index];
                  final active = _active == null || _active == glyph.id;
                  return Opacity(
                    opacity: active ? 1 : 0.45,
                    child: InkWell(
                      borderRadius: BorderRadius.circular(12),
                      onTap: () => setState(() => _active = glyph.id),
                      child: Container(
                        padding: const EdgeInsets.all(10),
                        decoration: BoxDecoration(
                          color: c.cardAlt,
                          borderRadius: BorderRadius.circular(12),
                          border: Border.all(color: c.line),
                        ),
                        child: Row(
                          children: [
                            Container(
                              width: 40,
                              height: 40,
                              alignment: Alignment.center,
                              decoration: BoxDecoration(
                                color: c.card,
                                borderRadius: BorderRadius.circular(8),
                                border: Border.all(color: c.line),
                              ),
                              child: Text(
                                glyph.label,
                                textDirection: TextDirection.ltr,
                                style: const TextStyle(
                                  fontFamily: 'DejaVu Sans Mono',
                                  fontFamilyFallback: ['monospace'],
                                  fontSize: 20,
                                ),
                              ),
                            ),
                            const SizedBox(width: 10),
                            Expanded(
                              child: Wrap(
                                spacing: 6,
                                runSpacing: 6,
                                children: [
                                  for (final alternative in glyph.alternatives)
                                    ActionChip(
                                      label: Text(alternative.glyph),
                                      onPressed: () => widget.onPick(
                                        glyph,
                                        alternative.glyph,
                                      ),
                                    ),
                                ],
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  );
                },
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _custom,
              textDirection: TextDirection.ltr,
              decoration: InputDecoration(
                labelText: widget.l10n.t('scan.fix.type_symbol'),
                hintText: widget.l10n.t('scan.fix.type_hint'),
                suffixIcon: IconButton(
                  icon: const Icon(Icons.check_rounded),
                  onPressed: () {
                    final text = _custom.text.trim();
                    if (text.isEmpty) return;
                    final glyph = glyphs.firstWhere(
                      (glyph) => glyph.id == _active,
                      orElse: () => glyphs.first,
                    );
                    widget.onPick(glyph, text);
                  },
                ),
              ),
              onSubmitted: (text) {
                if (text.trim().isEmpty) return;
                final glyph = glyphs.firstWhere(
                  (glyph) => glyph.id == _active,
                  orElse: () => glyphs.first,
                );
                widget.onPick(glyph, text.trim());
              },
            ),
          ],
        ),
      ),
    );
  }
}
