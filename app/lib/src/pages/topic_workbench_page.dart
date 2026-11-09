// Generic curriculum workbench.
//
// Every implemented topic shares the same contract: a set of named inputs, a
// `mode: "topic"` request and a result that already contains the short answer,
// the steps and the checks.  This page renders that contract without knowing
// which topic it is showing.
import 'package:flutter/material.dart';

import '../calc_widgets.dart';
import '../l10n.dart';
import '../models.dart';
import '../native_engine.dart';
import '../theme.dart';
import '../topic_catalog.dart';
import '../widgets.dart';

class TopicWorkbenchPage extends StatefulWidget {
  const TopicWorkbenchPage({
    super.key,
    required this.spec,
    required this.engine,
    required this.l10n,
    this.engineError,
  });

  final TopicSpec spec;
  final NativeEngine? engine;
  final L10n l10n;
  final String? engineError;

  @override
  State<TopicWorkbenchPage> createState() => _TopicWorkbenchPageState();
}

class _TopicWorkbenchPageState extends State<TopicWorkbenchPage> {
  final _controllers = <String, TextEditingController>{};
  final _resultKey = GlobalKey();
  SolveResult? _result;
  String? _error;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    for (final field in widget.spec.fields) {
      _controllers[field.key] = TextEditingController(text: field.initial);
    }
  }

  @override
  void didUpdateWidget(covariant TopicWorkbenchPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.spec.id != widget.spec.id) {
      for (final controller in _controllers.values) {
        controller.dispose();
      }
      _controllers.clear();
      for (final field in widget.spec.fields) {
        _controllers[field.key] = TextEditingController(text: field.initial);
      }
      _result = null;
      _error = null;
    } else if (oldWidget.l10n.isFa != widget.l10n.isFa) {
      _result = null;
      _error = null;
    }
  }

  @override
  void dispose() {
    for (final controller in _controllers.values) {
      controller.dispose();
    }
    super.dispose();
  }

  Future<void> _run() async {
    final engine = widget.engine;
    final l = widget.l10n;
    if (engine == null) {
      setState(
        () => _error =
            widget.engineError ??
            l.t('ui.the_computation_engine_is_unavailable'),
      );
      return;
    }
    final params = <String, dynamic>{};
    for (final field in widget.spec.fields) {
      final text = _controllers[field.key]?.text.trim() ?? '';
      if (text.isEmpty) continue;
      if (field.numeric) {
        final value = double.tryParse(text.replaceAll('−', '-'));
        if (value == null) {
          setState(
            () => _error = l.t('ui.field_must_be_a_number', {
              'field': l.t(field.labelKey),
            }),
          );
          return;
        }
        params[field.key] = value;
      } else {
        params[field.key] = text;
      }
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final r = await engine.topicAsync(
        widget.spec.backendId,
        params: params,
        lang: l.isFa ? 'fa' : 'en',
      );
      if (!mounted) return;
      setState(() {
        _result = r;
        _busy = false;
      });
      revealWidget(_resultKey);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = l.t('ui.engine_error');
        _busy = false;
      });
    }
  }

  bool _wideField(TopicField field) =>
      field.key == 'coeffs' ||
      field.key == 'ics' ||
      field.key == 'f' ||
      field.key == 'F';

  @override
  Widget build(BuildContext context) {
    final l = widget.l10n;
    final c = context.colors;
    final spec = widget.spec;
    final r = _result;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                tone: c.accent,
                icon: spec.icon,
                title: l.t(spec.titleKey),
                subtitle: l.t(spec.blurbKey),
              ),
              const SizedBox(height: 18),
              Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  for (final field in spec.fields)
                    if (_wideField(field))
                      SizedBox(
                        width: field.key == 'f' || field.key == 'F' ? 360 : 300,
                        child: LabelledField(
                          controller: _controllers[field.key]!,
                          label: l.t(field.labelKey),
                          hint: field.hintKey.isEmpty
                              ? null
                              : l.t(field.hintKey),
                          width: field.key == 'f' || field.key == 'F'
                              ? 360
                              : 300,
                          onSubmitted: _run,
                        ),
                      )
                    else
                      LabelledField(
                        controller: _controllers[field.key]!,
                        label: l.t(field.labelKey),
                        hint: field.hintKey.isEmpty ? null : l.t(field.hintKey),
                        width: 160,
                        onSubmitted: _run,
                      ),
                ],
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: _busy ? null : _run,
                icon: _busy
                    ? const SizedBox(
                        width: 17,
                        height: 17,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(Icons.play_arrow_rounded, size: 19),
                label: Text(
                  _busy
                      ? l.t('ui.computing')
                      : l.t('ui.show_the_answer_and_steps'),
                ),
              ),
            ],
          ),
        ),
        if (spec.examples.isNotEmpty) ...[
          const SizedBox(height: 16),
          LearningCard(
            l10n: l,
            title: l.t('ui.worked_example'),
            idea: l.t('ui.try_these_values_then_change_one_number_and_comp'),
            points: spec.examples,
          ),
        ],
        if (_error != null) ...[
          const SizedBox(height: 16),
          ErrorCard(message: _error!),
        ],
        if (r != null) ...[
          const SizedBox(height: 18),
          Container(
            key: _resultKey,
            child: ResultView(result: r, l10n: l),
          ),
        ],
      ],
    );
  }
}
