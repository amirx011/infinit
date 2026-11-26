// Photo → expression. The classical scanner (no ML) runs inside libodecore;
// this page only picks an image, shows the reading and routes it to the
// matching workbench.
import 'dart:io' show File, Platform;

import 'package:file_selector/file_selector.dart';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../l10n.dart';
import '../scanner/lab_page.dart';
import '../scanner/normalize.dart';
import '../scanner_engine.dart';
import '../theme.dart';
import '../widgets.dart';

class ScannerPage extends StatefulWidget {
  const ScannerPage({super.key, required this.l10n, this.onOpen});

  final L10n l10n;

  /// Opens a workbench with prefilled fields. The shell supplies this so the
  /// scanner stays a leaf page and never owns navigation state.
  final void Function(int page, Map<String, String> fields)? onOpen;

  @override
  State<ScannerPage> createState() => _ScannerPageState();
}

class _ScannerPageState extends State<ScannerPage> {
  /// Native desktop import: OS drag-and-drop and clipboard paste. The runner
  /// owns the channel; on Android/iOS no handler is registered and every
  /// call fails as a MissingPluginException, which the paste path tolerates.
  static const _importChannel = MethodChannel('infinit/import');
  static bool get _desktop =>
      !kIsWeb && (Platform.isLinux || Platform.isWindows || Platform.isMacOS);

  static const _imageTypes = XTypeGroup(
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
      'heic',
      'heif',
    ],
  );

  ScannerEngine? _scanner;
  String? _error;
  Uint8List? _image;
  String? _imageName;
  ScanResult? _result;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    if (_desktop) {
      _importChannel.setMethodCallHandler(_onImportCall);
    }
    _loadScanner();
  }

  @override
  void dispose() {
    if (_desktop) {
      _importChannel.setMethodCallHandler(null);
    }
    super.dispose();
  }

  Future<dynamic> _onImportCall(MethodCall call) async {
    if (call.method == 'drop' || call.method == 'paste') {
      final path = call.arguments;
      if (path is String && path.isNotEmpty) {
        await _importPath(path);
      }
    }
    return null;
  }

  /// Reads a path handed over by the desktop runner: a dropped file, a pasted
  /// image saved to temp, or a copied file path.
  Future<void> _importPath(String path) async {
    try {
      final file = File(path);
      if (!await file.exists()) {
        throw const FormatException('the selected file no longer exists');
      }
      final bytes = await normalizeForScanner(await file.readAsBytes());
      // A clipboard image was written to the temp folder by the runner; the
      // working copy above is in memory, so the file can go now.
      if (path.contains('infinit-paste')) {
        try {
          await file.delete();
        } catch (_) {
          // The OS reclaims the temp folder eventually.
        }
      }
      if (!mounted) return;
      setState(() {
        _image = bytes;
        _imageName = path.split(Platform.pathSeparator).last;
        _result = null;
        _error = null;
      });
      await _read();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _error = widget.l10n.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  Future<void> _pasteImage() async {
    if (!_desktop) return;
    try {
      final path = await _importChannel.invokeMethod<String>('paste');
      if (path != null && path.isNotEmpty) {
        await _importPath(path);
        return;
      }
    } on MissingPluginException {
      // Old runner without the channel: try the text clipboard below.
    } catch (_) {
      // A clipboard that cannot produce an image is not fatal.
    }
    final data = await Clipboard.getData(Clipboard.kTextPlain);
    final text = data?.text?.trim();
    if (text != null && text.isNotEmpty && File(text).existsSync()) {
      await _importPath(text);
      return;
    }
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(widget.l10n.t('scan.import.empty'))),
    );
  }

  Future<void> _loadScanner() async {
    try {
      final scanner = await ScannerEngine.load();
      if (mounted) setState(() => _scanner = scanner);
    } catch (error) {
      if (mounted) {
        setState(
          () => _error = widget.l10n.t('ui.scanner_unavailable', {
            'error': '$error',
          }),
        );
      }
    }
  }

  Future<void> _pickImage() async {
    try {
      final file = await openFile(acceptedTypeGroups: const [_imageTypes]);
      if (file == null) return;
      // Decode through the platform codec: HEIC/HEIF has no native decoder in
      // the classical pipeline, and the re-encode also applies EXIF rotation
      // and caps the working size.
      final bytes = await normalizeForScanner(await file.readAsBytes());
      if (!mounted) return;
      setState(() {
        _image = bytes;
        _imageName = file.name;
        _result = null;
        _error = null;
      });
      await _read();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _error = widget.l10n.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  Future<void> _read() async {
    final image = _image;
    final scanner = _scanner;
    if (image == null) return;
    if (scanner == null) {
      setState(() => _error = widget.l10n.t('ui.scanner_unavailable', {
        'error': widget.l10n.t('ui.scanner'),
      }));
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final result = await scanner.read(image);
      if (!mounted) return;
      setState(() {
        _result = result;
        _busy = false;
      });
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = widget.l10n.t('ui.scanner_failed', {'error': '$error'});
      });
    }
  }

  Future<void> _copy(String text) async {
    await Clipboard.setData(ClipboardData(text: text));
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(widget.l10n.t('ui.copied'))),
    );
  }

  int? get _targetPage => switch (_result?.kind) {
    'sum' => 26,
    'integral' => 6,
    'limit' => 7,
    _ => null,
  };

  /// The scanner reports stable English field names; the UI shows them in the
  /// current language.
  String _partLabel(L10n l10n, String name) {
    if (name.startsWith('equation')) {
      return l10n.t('ui.part_equation', {
        'n': name.substring('equation'.length),
      });
    }
    return switch (name) {
      'term' => l10n.t('ui.part_term'),
      'index' => l10n.t('ui.part_index'),
      'start' => l10n.t('ui.part_start'),
      'end' => l10n.t('ui.part_end'),
      'lower' => l10n.t('ui.part_lower'),
      'upper' => l10n.t('ui.part_upper'),
      'integrand' => l10n.t('ui.integrand'),
      'variable' => l10n.t('ui.variable'),
      'expression' => l10n.t('ui.expression'),
      'point' => l10n.t('ui.point'),
      _ => name,
    };
  }

  String _kindLabel(L10n l10n, String kind) => switch (kind) {
    'sum' => l10n.t('ui.kind_sum'),
    'integral' => l10n.t('ui.integral'),
    'limit' => l10n.t('ui.limit'),
    'system' => l10n.t('ui.kind_system'),
    _ => l10n.t('ui.kind_expression'),
  };

  @override
  Widget build(BuildContext context) {
    final l10n = widget.l10n;
    final c = context.colors;
    final result = _result;
    final target = _targetPage;

    return CallbackShortcuts(
      bindings: {
        const SingleActivator(LogicalKeyboardKey.keyV, control: true):
            _pasteImage,
        const SingleActivator(LogicalKeyboardKey.keyV, meta: true): _pasteImage,
      },
      child: Focus(
        autofocus: _desktop,
        child: SoftCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          PageTitle(
            tone: c.accent,
            icon: Icons.document_scanner_rounded,
            title: l10n.t('ui.scanner'),
            subtitle: l10n.t('ui.scanner_subtitle'),
          ),
          const SizedBox(height: 18),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            crossAxisAlignment: WrapCrossAlignment.center,
            children: [
              FilledButton.icon(
                onPressed: _busy ? null : _pickImage,
                icon: const Icon(Icons.image_outlined, size: 18),
                label: Text(l10n.t('ui.choose_image')),
              ),
              if (_image != null)
                OutlinedButton.icon(
                  onPressed: _busy ? null : _read,
                  icon: const Icon(Icons.refresh_rounded, size: 18),
                  label: Text(l10n.t('ui.scan_again')),
                ),
              OutlinedButton.icon(
                onPressed: () => Navigator.of(context).push(
                  MaterialPageRoute<void>(
                    builder: (_) => ScannerLabPage(
                      l10n: l10n,
                      initialImage: _image,
                    ),
                  ),
                ),
                icon: const Icon(Icons.science_outlined, size: 18),
                label: Text(l10n.t('scan.lab.open')),
              ),
            ],
          ),
          if (_desktop) ...[
            const SizedBox(height: 8),
            Text(
              l10n.t('scan.import.drop_hint'),
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: c.muted,
              ),
            ),
          ],
          if (_imageName != null) ...[
            const SizedBox(height: 10),
            Text(
              _imageName!,
              style: Theme.of(context).textTheme.bodySmall,
              overflow: TextOverflow.ellipsis,
            ),
          ],
          if (_image != null) ...[
            const SizedBox(height: 14),
            Container(
              width: double.infinity,
              constraints: const BoxConstraints(maxHeight: 240),
              decoration: BoxDecoration(
                color: c.cardAlt,
                borderRadius: BorderRadius.circular(15),
                border: Border.all(color: c.line),
              ),
              padding: const EdgeInsets.all(10),
              child: Image.memory(
                _image!,
                fit: BoxFit.contain,
                gaplessPlayback: true,
              ),
            ),
          ],
          if (_busy) ...[
            const SizedBox(height: 18),
            const LinearProgressIndicator(minHeight: 3),
            const SizedBox(height: 8),
            Text(
              l10n.t('ui.reading_image'),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
          if (_error != null) ...[
            const SizedBox(height: 16),
            Text(_error!, style: TextStyle(color: c.warning)),
          ],
          if (result != null) ...[
            const SizedBox(height: 20),
            Row(
              children: [
                SectionLabel(l10n.t('ui.expression')),
                const Spacer(),
                IconButton(
                  tooltip: l10n.t('ui.copy_expression'),
                  icon: const Icon(Icons.copy_rounded, size: 19),
                  onPressed: () => _copy(result.expression),
                ),
              ],
            ),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
              decoration: BoxDecoration(
                color: c.cardAlt,
                borderRadius: BorderRadius.circular(13),
                border: Border.all(color: c.line),
              ),
              child: SelectableText(
                result.expression.isEmpty ? '—' : result.expression,
                textDirection: TextDirection.ltr,
                style: const TextStyle(
                  fontFamily: 'DejaVu Sans Mono',
                  fontFamilyFallback: ['monospace', 'Vazirmatn'],
                  fontSize: 16,
                ),
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _Badge(text: _kindLabel(l10n, result.kind), tone: c.accent),
                _Badge(
                  text:
                      '${l10n.t('ui.scanner_confidence')} '
                      '${(result.confidence * 100).round()}%',
                  tone: c.muted,
                ),
                _Badge(
                  text:
                      '${result.symbols} ${l10n.t('ui.scanner_symbols')} · '
                      '${result.binarization}',
                  tone: c.muted,
                ),
              ],
            ),
            if (result.parts.isNotEmpty) ...[
              const SizedBox(height: 18),
              SectionLabel(l10n.t('ui.scanner_parts')),
              const SizedBox(height: 6),
              for (final entry in result.parts.entries)
                _PartRow(
                  name: _partLabel(l10n, entry.key),
                  value: entry.value,
                  onCopy: () => _copy(entry.value),
                  tooltip: l10n.t('ui.copy'),
                ),
            ],
            if (target != null) ...[
              const SizedBox(height: 18),
              FilledButton.tonalIcon(
                onPressed: () {
                  widget.onOpen?.call(target, result.parts);
                },
                icon: const Icon(Icons.arrow_forward_rounded, size: 18),
                label: Text(
                  '${l10n.t('ui.scanner_open_in')} '
                  '${_kindLabel(l10n, result.kind)}',
                ),
              ),
            ],
            const SizedBox(height: 14),
            Text(
              l10n.t('ui.scanner_tip'),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ] else if (!_busy && _image == null) ...[
            const SizedBox(height: 18),
            Text(
              l10n.t('ui.scanner_no_image'),
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
        ],
      ),
        ),
      ),
    );
  }
}

class _Badge extends StatelessWidget {
  const _Badge({required this.text, required this.tone});

  final String text;
  final Color tone;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      decoration: BoxDecoration(
        color: tone.withValues(alpha: 0.10),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: tone.withValues(alpha: 0.28)),
      ),
      child: Text(
        text,
        style: Theme.of(
          context,
        ).textTheme.labelSmall?.copyWith(color: tone, fontWeight: FontWeight.w600),
      ),
    );
  }
}

class _PartRow extends StatelessWidget {
  const _PartRow({
    required this.name,
    required this.value,
    required this.onCopy,
    required this.tooltip,
  });

  final String name;
  final String value;
  final VoidCallback onCopy;
  final String tooltip;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 92,
            child: Text(
              name,
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: c.muted),
            ),
          ),
          Expanded(
            child: Text(
              value,
              textDirection: TextDirection.ltr,
              style: const TextStyle(
                fontFamily: 'DejaVu Sans Mono',
                fontFamilyFallback: ['monospace', 'Vazirmatn'],
              ),
            ),
          ),
          IconButton(
            tooltip: tooltip,
            visualDensity: VisualDensity.compact,
            icon: Icon(Icons.copy_rounded, size: 17, color: c.muted),
            onPressed: onCopy,
          ),
        ],
      ),
    );
  }
}
