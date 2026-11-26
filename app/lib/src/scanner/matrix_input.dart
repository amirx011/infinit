// Matrix entry as a real grid: rows × columns selectors plus one small box
// per cell. The core's matrix fields still take the compact text form
// "a, b; c, d"; this widget is the only place that string is assembled, so a
// user never has to type a semicolon.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../theme.dart';
import 'scan_button.dart';

const int kMatrixMaxSize = 8;

/// Splits the compact matrix text into rows of cells. Accepts the Arabic
/// semicolon and comma too, and pads ragged rows so the grid stays square.
List<List<String>> parseMatrixCells(String text) {
  final trimmed = text.trim();
  if (trimmed.isEmpty) {
    return [
      <String>['', ''],
    ];
  }
  final rows = trimmed
      .split(RegExp('[;\u061B]'))
      .map(
        (row) =>
            row.split(RegExp('[,\u060C]')).map((cell) => cell.trim()).toList(),
      )
      .toList();
  var columns = 1;
  for (final row in rows) {
    if (row.length > columns) columns = row.length;
  }
  for (final row in rows) {
    while (row.length < columns) {
      row.add('');
    }
  }
  return rows;
}

/// Joins the grid back into the compact text the core parses.
String joinMatrixCells(List<List<String>> cells) {
  return cells
      .map((row) => row.map((cell) => cell.trim()).join(', '))
      .join('; ');
}

class MatrixInput extends StatefulWidget {
  const MatrixInput({
    super.key,
    required this.controller,
    required this.l10n,
    this.width = 380,
  });

  /// The field value, kept in the core's compact text form.
  final TextEditingController controller;
  final L10n l10n;
  final double width;

  @override
  State<MatrixInput> createState() => _MatrixInputState();
}

class _MatrixInputState extends State<MatrixInput> {
  List<List<TextEditingController>> _cells = const [];
  var _rows = 2;
  var _columns = 2;
  var _syncing = false;

  L10n get l => widget.l10n;

  @override
  void initState() {
    super.initState();
    _rebuild(parseMatrixCells(widget.controller.text));
    widget.controller.addListener(_onExternalChange);
  }

  @override
  void didUpdateWidget(covariant MatrixInput oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.removeListener(_onExternalChange);
      widget.controller.addListener(_onExternalChange);
      _rebuild(parseMatrixCells(widget.controller.text));
    }
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onExternalChange);
    for (final row in _cells) {
      for (final cell in row) {
        cell.dispose();
      }
    }
    super.dispose();
  }

  /// Replaces the cell controllers with the parsed grid. Values that already
  /// exist are copied into the new shape; the rest start empty.
  void _rebuild(List<List<String>> parsed) {
    final old = <List<String>>[
      for (final row in _cells) [for (final cell in row) cell.text],
    ];
    for (final row in _cells) {
      for (final cell in row) {
        cell.dispose();
      }
    }
    _rows = parsed.length.clamp(1, kMatrixMaxSize);
    _columns = parsed.first.length.clamp(1, kMatrixMaxSize);
    _cells = [
      for (var r = 0; r < _rows; r++)
        [
          for (var c = 0; c < _columns; c++)
            TextEditingController(
              text: r < old.length && c < old[r].length
                  ? old[r][c]
                  : r < parsed.length && c < parsed[r].length
                  ? parsed[r][c]
                  : '',
            ),
        ],
    ];
    for (final row in _cells) {
      for (final cell in row) {
        cell.addListener(_onCellChanged);
      }
    }
  }

  void _onCellChanged() => _pushToField();

  void _onExternalChange() {
    if (_syncing) return;
    final current = joinMatrixCells([
      for (final row in _cells) [for (final cell in row) cell.text],
    ]);
    if (current == widget.controller.text) return;
    setState(() => _rebuild(parseMatrixCells(widget.controller.text)));
  }

  void _pushToField() {
    final text = joinMatrixCells([
      for (final row in _cells) [for (final cell in row) cell.text],
    ]);
    _syncing = true;
    widget.controller.text = text;
    _syncing = false;
  }

  void _resize(int rows, int columns) {
    final old = [
      for (final row in _cells) [for (final cell in row) cell.text],
    ];
    for (final row in _cells) {
      for (final cell in row) {
        cell.dispose();
      }
    }
    setState(() {
      _rows = rows.clamp(1, kMatrixMaxSize);
      _columns = columns.clamp(1, kMatrixMaxSize);
      _cells = [
        for (var r = 0; r < _rows; r++)
          [
            for (var c = 0; c < _columns; c++)
              TextEditingController(
                text: r < old.length && c < old[r].length ? old[r][c] : '',
              ),
          ],
      ];
      for (final row in _cells) {
        for (final cell in row) {
          cell.addListener(_onCellChanged);
        }
      }
    });
    _pushToField();
  }

  void _clear() {
    for (final row in _cells) {
      for (final cell in row) {
        cell.text = '';
      }
    }
    _pushToField();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return SizedBox(
      width: widget.width,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Wrap(
            spacing: 10,
            runSpacing: 6,
            crossAxisAlignment: WrapCrossAlignment.center,
            children: [
              Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    l.t('scan.matrix.rows'),
                    style: Theme.of(context).textTheme.bodySmall
                        ?.copyWith(color: c.muted),
                  ),
                  const SizedBox(width: 6),
                  _SizePicker(
                    value: _rows,
                    l10n: l,
                    onChanged: (value) => _resize(value, _columns),
                  ),
                ],
              ),
              Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    l.t('scan.matrix.columns'),
                    style: Theme.of(context).textTheme.bodySmall
                        ?.copyWith(color: c.muted),
                  ),
                  const SizedBox(width: 6),
                  _SizePicker(
                    value: _columns,
                    l10n: l,
                    onChanged: (value) => _resize(_rows, value),
                  ),
                ],
              ),
              IconButton(
                tooltip: l.t('scan.matrix.clear'),
                visualDensity: VisualDensity.compact,
                iconSize: 18,
                icon: const Icon(Icons.backspace_outlined),
                onPressed: _clear,
              ),
              ScanFieldButton(
                controller: widget.controller,
                iconSize: 20,
                onFilled: (_) => setState(
                  () => _rebuild(parseMatrixCells(widget.controller.text)),
                ),
              ),
            ],
          ),
          const SizedBox(height: 4),
          SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: c.cardAlt,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: c.line),
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  for (var r = 0; r < _rows; r++)
                    Padding(
                      padding: EdgeInsets.only(bottom: r == _rows - 1 ? 0 : 6),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          for (var col = 0; col < _columns; col++)
                            Padding(
                              padding: EdgeInsets.only(
                                right: col == _columns - 1 ? 0 : 6,
                              ),
                              child: SizedBox(
                                width: 64,
                                child: TextField(
                                  controller: _cells[r][col],
                                  textDirection: TextDirection.ltr,
                                  textAlign: TextAlign.center,
                                  style: const TextStyle(
                                    fontFamily: 'DejaVu Sans Mono',
                                    fontFamilyFallback: ['monospace'],
                                    fontSize: 14,
                                  ),
                                  decoration: InputDecoration(
                                    isDense: true,
                                    contentPadding: const EdgeInsets.symmetric(
                                      horizontal: 6,
                                      vertical: 10,
                                    ),
                                    hintText: '0',
                                  ),
                                ),
                              ),
                            ),
                        ],
                      ),
                    ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 6),
          Text(
            l.t('scan.matrix.hint'),
            style: Theme.of(context).textTheme.bodySmall
                ?.copyWith(color: c.muted),
          ),
        ],
      ),
    );
  }
}

class _SizePicker extends StatelessWidget {
  const _SizePicker({
    required this.value,
    required this.l10n,
    required this.onChanged,
  });

  final int value;
  final L10n l10n;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8),
      decoration: BoxDecoration(
        color: c.cardAlt,
        borderRadius: BorderRadius.circular(9),
        border: Border.all(color: c.line),
      ),
      child: DropdownButtonHideUnderline(
        child: DropdownButton<int>(
          value: value.clamp(1, kMatrixMaxSize),
          isDense: true,
          icon: const Icon(Icons.expand_more_rounded, size: 18),
          items: [
            for (var size = 1; size <= kMatrixMaxSize; size++)
              DropdownMenuItem<int>(value: size, child: Text('$size')),
          ],
          onChanged: (next) {
            if (next != null) onChanged(next);
          },
        ),
      ),
    );
  }
}
