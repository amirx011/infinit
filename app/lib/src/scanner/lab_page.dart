// Scanner Lab (plan 13.4). A classical pipeline fails visually, so this page
// shows one image per stage next to the reading it produced: gray, illumination
// flattening, the three binarisations, the chosen binary and the symbol boxes.
// It is a debug surface: nothing here solves anything.
import 'dart:typed_data';

import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../scanner_engine.dart';
import '../theme.dart';

/// Every stage the native side can render, in pipeline order.
const labStages = <String>[
  'gray',
  'normalized',
  'otsu',
  'sauvola',
  'adaptive',
  'binary',
  'symbols',
];

class ScannerLabPage extends StatefulWidget {
  const ScannerLabPage({super.key, required this.l10n, this.initialImage});

  final L10n l10n;

  /// An image the caller already has (the scanner page hands its picture over
  /// so the Lab opens on something instead of an empty screen).
  final Uint8List? initialImage;

  @override
  State<ScannerLabPage> createState() => _ScannerLabPageState();
}

class _ScannerLabPageState extends State<ScannerLabPage> {
  static const _imageTypes = XTypeGroup(
    label: 'images',
    extensions: ['png', 'jpg', 'jpeg', 'webp', 'bmp', 'gif', 'tif', 'tiff'],
  );

  final Map<String, Uint8List?> _stageImages = {};
  Uint8List? _image;
  String? _name;
  String _stage = 'binary';
  bool _handwriting = false;
  ScanResult? _result;
  String? _error;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    final initial = widget.initialImage;
    if (initial != null && initial.isNotEmpty) {
      _image = initial;
      _name = widget.l10n.t('scan.lab.initial');
      _loadStage(_stage);
      _read();
    }
  }

  Future<void> _pick() async {
    final file = await openFile(acceptedTypeGroups: const [_imageTypes]);
    if (file == null) return;
    final bytes = await file.readAsBytes();
    if (!mounted) return;
    setState(() {
      _image = bytes;
      _name = file.name;
      _stageImages.clear();
      _result = null;
      _error = null;
    });
    await _loadStage(_stage);
    await _read();
  }

  Future<void> _loadStage(String stage) async {
    final image = _image;
    if (image == null) return;
    setState(() => _stage = stage);
    if (_stageImages.containsKey(stage)) return;
    try {
      final engine = await ScannerEngine.load();
      final bytes = await engine.debugStage(image, stage);
      if (!mounted) return;
      setState(() => _stageImages[stage] = bytes);
    } catch (error) {
      if (!mounted) return;
      setState(() => _error = '$error');
    }
  }

  Future<void> _read() async {
    final image = _image;
    if (image == null) return;
    setState(() => _busy = true);
    try {
      final engine = await ScannerEngine.load();
      // The handwriting path is experimental (§12A.8), so it lives in the Lab
      // behind a switch instead of the normal scan flow.
      final result = _handwriting
          ? await engine.readHandwriting(image)
          : await engine.read(image);
      if (!mounted) return;
      setState(() {
        _result = result;
        _busy = false;
        _error = null;
      });
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = '$error';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    return Scaffold(
      appBar: AppBar(
        title: Text(l.t('scan.lab.title')),
        actions: [
          TextButton(onPressed: _busy ? null : _pick, child: Text(l.t('ui.choose_image'))),
          const SizedBox(width: 8),
        ],
      ),
      body: SafeArea(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final wide = constraints.maxWidth >= 900;
            final preview = _previewPane(context, expand: wide);
            final details = _detailsPane(context);
            if (!wide) {
              return SingleChildScrollView(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [preview, const SizedBox(height: 16), details],
                ),
              );
            }
            return Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(flex: 3, child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: preview,
                )),
                VerticalDivider(width: 1, color: c.line),
                Expanded(
                  flex: 2,
                  child: SingleChildScrollView(
                    padding: const EdgeInsets.all(16),
                    child: details,
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  /// One timing entry, printed as milliseconds with one decimal.
  String _ms(ScanResult result, String stage) {
    final value = result.timing[stage];
    return value == null ? '-' : '${value.toStringAsFixed(1)} ms';
  }

  Widget _previewPane(BuildContext context, {required bool expand}) {
    final l = widget.l10n;
    final c = context.colors;
    final bytes = _stageImages[_stage];
    final viewer = Container(
      decoration: BoxDecoration(
        color: c.cardAlt,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: c.line),
      ),
      clipBehavior: Clip.antiAlias,
      child: bytes == null
          ? Center(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Text(
                  _image == null ? l.t('scan.lab.pick') : l.t('scan.lab.loading'),
                  style: Theme.of(
                    context,
                  ).textTheme.bodySmall?.copyWith(color: c.muted),
                  textAlign: TextAlign.center,
                ),
              ),
            )
          : InteractiveViewer(
              maxScale: 6,
              child: Image.memory(
                bytes,
                fit: BoxFit.contain,
                gaplessPlayback: true,
              ),
            ),
    );
    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Wrap(
          spacing: 8,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            FilterChip(
              label: Text(l.t('scan.lab.handwriting')),
              selected: _handwriting,
              onSelected: (value) {
                setState(() => _handwriting = value);
                _read();
              },
            ),
            for (final stage in labStages)
              ChoiceChip(
                label: Text(l.t('scan.lab.stage.$stage')),
                selected: _stage == stage,
                onSelected: (_) => _loadStage(stage),
              ),
          ],
        ),
        const SizedBox(height: 12),
        if (expand)
          Expanded(child: viewer)
        else
          SizedBox(height: 420, child: viewer),
      ],
    );
  }

  Widget _detailsPane(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    final result = _result;
    final style = Theme.of(context).textTheme.bodySmall;
    Widget row(String label, String value) => Padding(
      padding: const EdgeInsets.only(bottom: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 116,
            child: Text(label, style: style?.copyWith(color: c.muted)),
          ),
          Expanded(
            child: SelectableText(value, style: style),
          ),
        ],
      ),
    );
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(l.t('scan.lab.reading'), style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 8),
        if (_name != null) row(l.t('scan.lab.file'), _name!),
        if (_error != null)
          Text(_error!, style: style?.copyWith(color: c.warning)),
        if (result != null) ...[
          row(l.t('scan.lab.status'), result.status),
          row(l.t('scan.lab.kind'), result.kind),
          row(
            l.t('scan.lab.confidence'),
            result.confidence.toStringAsFixed(3),
          ),
          row(l.t('scan.lab.profile'), result.binarization),
          row(
            l.t('scan.lab.skew'),
            '${result.skewDegrees.toStringAsFixed(2)}°',
          ),
          if (result.warnings.isNotEmpty)
            row(l.t('scan.lab.diagnostic'), result.warnings.join(', ')),
          row(
            l.t('scan.lab.timing'),
            'pre ${_ms(result, 'preprocess')} · seg ${_ms(result, 'segment')} · '
                'cls ${_ms(result, 'classify')} · str ${_ms(result, 'structure')} · '
                'rep ${_ms(result, 'repair')}',
          ),
          const SizedBox(height: 8),
          row(l.t('scan.lab.expression'), result.expression),
          if (result.latex.isNotEmpty) row(l.t('scan.lab.latex'), result.latex),
          const SizedBox(height: 12),
          Text(
            '${l.t('scan.lab.symbols')} (${result.symbols})',
            style: Theme.of(context).textTheme.titleSmall,
          ),
          const SizedBox(height: 6),
          for (final glyph in result.glyphs)
            Padding(
              padding: const EdgeInsets.only(bottom: 2),
              child: SelectableText(
                '#${glyph.id}  ${glyph.label}'
                '  ${glyph.score.toStringAsFixed(2)}'
                '  [${glyph.box.length >= 4 ? '${glyph.box[0]},${glyph.box[1]},'
                    '${glyph.box[2]}×${glyph.box[3]}' : '?'}]',
                style: const TextStyle(
                  fontFamily: 'DejaVu Sans Mono',
                  fontFamilyFallback: ['monospace'],
                  fontSize: 12,
                ),
              ),
            ),
        ],
      ],
    );
  }
}
