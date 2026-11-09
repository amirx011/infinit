import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/result.dart';

void main() {
  test('step accepts a plain math string', () {
    final step = LocalizedStep.fromJson({
      'step': 'taylor.value_at_center',
      'math': '4y',
      'params': {'a': 0},
    });

    expect(step.key, 'taylor.value_at_center');
    expect(step.math?.plain, '4y');
    expect(step.math?.latex, '4y');
    expect(step.params['a'], 0);
    expect(step.noteKey, isNull);
  });

  test('step accepts the structured engine math object', () {
    final step = LocalizedStep.fromJson({
      'key': 'laplace.transform',
      'note_key': 'note.interpreted_as',
      'math': {'plain': 'exp(x)', 'pretty': 'e^x', 'latex': 'e^{x}'},
      'params': {'order': 2},
    });

    expect(step.key, 'laplace.transform');
    expect(step.noteKey, 'note.interpreted_as');
    expect(step.math?.pretty, 'e^x');
    expect(step.params['order'], 2);
  });

  test('notice keeps its level and params', () {
    final notice = LocalizedNotice.fromJson({
      'notice': 'note.interpreted_as',
      'level': 'warning',
      'params': {'n': 3},
    });

    expect(notice.key, 'note.interpreted_as');
    expect(notice.level, NoticeLevel.warning);
    expect(notice.params['n'], 3);
  });

  test('validation carries the ok flag', () {
    final validation = LocalizedValidation.fromJson({
      'validation': 'taylor.match',
      'ok': true,
      'params': {'x': 0.1},
    });

    expect(validation.key, 'taylor.match');
    expect(validation.ok, isTrue);
    expect(validation.params['x'], 0.1);
  });

  test('result parses steps, validations, notices and a string error', () {
    final result = LocalizedResult.fromJson({
      'answer_math': 'x^2',
      'steps': [
        {'step': 'taylor.value_at_center', 'math': '4y'},
      ],
      'validations': [
        {'validation': 'taylor.match', 'ok': true},
      ],
      'notices': [
        {'notice': 'note.interpreted_as', 'level': 'info'},
      ],
      'error': 'error.singular',
    });

    expect(result.answerMath?.plain, 'x^2');
    expect(result.steps, hasLength(1));
    expect(result.validations.single.ok, isTrue);
    expect(result.notices.single.level, NoticeLevel.info);
    expect(result.error?.key, 'error.singular');
    expect(result.error?.level, NoticeLevel.error);
  });

  test('result accepts an error object and defaults missing lists', () {
    final result = LocalizedResult.fromJson({
      'error': {'error': 'error.singular', 'params': {'point': 0}},
    });

    expect(result.steps, isEmpty);
    expect(result.validations, isEmpty);
    expect(result.notices, isEmpty);
    expect(result.error?.key, 'error.singular');
    expect(result.error?.params['point'], 0);
  });
}
