import '../models.dart';

/// Severity of a [LocalizedNotice].
enum NoticeLevel {
  info,
  warning,
  error;

  static NoticeLevel parse(Object? value) {
    switch (value) {
      case 'warning':
        return NoticeLevel.warning;
      case 'error':
        return NoticeLevel.error;
      default:
        return NoticeLevel.info;
    }
  }
}

/// One solver step: a translation key, optional math payload and plain params.
///
/// Math is never embedded in [key] or [params]. It travels in [math] so the UI
/// can render it in its own left-to-right widget next to the sentence.
class LocalizedStep {
  const LocalizedStep({
    required this.key,
    this.math,
    this.params = const {},
    this.noteKey,
  });

  final String key;
  final ExView? math;
  final Map<String, Object?> params;
  final String? noteKey;

  static LocalizedStep fromJson(Map value) => LocalizedStep(
    key: _first(value, const ['step', 'key']),
    math: _math(value['math']),
    params: _params(value['params']),
    noteKey: _optional(value, const ['note_key', 'noteKey', 'note']),
  );
}

/// A translated notice. The key resolves to a whole sentence; [params] may only
/// hold plain values, never formulas.
class LocalizedNotice {
  const LocalizedNotice({
    required this.key,
    this.params = const {},
    this.level = NoticeLevel.info,
  });

  final String key;
  final Map<String, Object?> params;
  final NoticeLevel level;

  static LocalizedNotice fromJson(Map value) => LocalizedNotice(
    key: _first(value, const ['notice', 'notice_key', 'key', 'error']),
    params: _params(value['params']),
    level: NoticeLevel.parse(value['level']),
  );
}

/// A validation result with its own translation key.
class LocalizedValidation {
  const LocalizedValidation({
    required this.key,
    this.params = const {},
    required this.ok,
  });

  final String key;
  final Map<String, Object?> params;
  final bool ok;

  static LocalizedValidation fromJson(Map value) => LocalizedValidation(
    key: _first(value, const ['validation', 'validation_key', 'key']),
    params: _params(value['params']),
    ok: value['ok'] == true,
  );
}

/// Language-free solver output shared by every section.
///
/// The UI renders this with one generic view: [answerMath] and each step's math
/// are left-to-right siblings of the translated sentences, and [error] holds a
/// key instead of raw text.
class LocalizedResult {
  const LocalizedResult({
    this.answerMath,
    this.steps = const [],
    this.validations = const [],
    this.notices = const [],
    this.error,
  });

  final ExView? answerMath;
  final List<LocalizedStep> steps;
  final List<LocalizedValidation> validations;
  final List<LocalizedNotice> notices;
  final LocalizedNotice? error;

  static LocalizedResult fromJson(Map value) => LocalizedResult(
    answerMath: _math(value['answer_math'] ?? value['answer']),
    steps: _list(value['steps'], LocalizedStep.fromJson),
    validations: _list(
      value['validations'] ?? value['checks'],
      LocalizedValidation.fromJson,
    ),
    notices: _list(
      value['notices'] ?? value['notes'],
      LocalizedNotice.fromJson,
    ),
    error: _error(value['error']),
  );
}

String _first(Map value, List<String> keys) {
  for (final key in keys) {
    final found = value[key];
    if (found != null) return found.toString();
  }
  return '';
}

String? _optional(Map value, List<String> keys) {
  for (final key in keys) {
    final found = value[key];
    if (found is String && found.isNotEmpty) return found;
  }
  return null;
}

Map<String, Object?> _params(Object? value) {
  if (value is! Map) return const {};
  return {for (final entry in value.entries) entry.key.toString(): entry.value};
}

ExView? _math(Object? value) {
  if (value is Map) return ExView.fromJson(value);
  if (value is String && value.isNotEmpty) {
    return ExView(plain: value, pretty: value, latex: value);
  }
  return null;
}

List<T> _list<T>(Object? value, T Function(Map) parse) {
  if (value is! List) return <T>[];
  return [
    for (final item in value)
      if (item is Map) parse(item),
  ];
}

LocalizedNotice? _error(Object? value) {
  if (value is String && value.isNotEmpty) {
    return LocalizedNotice(key: value, level: NoticeLevel.error);
  }
  if (value is Map) {
    final notice = LocalizedNotice.fromJson(value);
    return LocalizedNotice(
      key: notice.key,
      params: notice.params,
      level: NoticeLevel.error,
    );
  }
  return null;
}
