import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/models.dart';
import 'package:infinit/src/native_engine.dart';
import 'package:infinit/src/topic_catalog.dart';

/// Guard: every curriculum topic emits catalogue keys that resolve.
void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final engine = NativeEngine.instance;

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  void expectResolves(String key) {
    if (key.isEmpty) return;
    expect(I18n.instance.hasKey(key), isTrue, reason: 'missing $key');
  }

  void expectResult(SolveResult result, String topic) {
    for (final step in result.steps) {
      expectResolves(step.titleKey);
      expectResolves(step.noteKey);
    }
    for (final check in result.checks) {
      expectResolves(check.nameKey);
      expectResolves(check.detailKey);
    }
    for (final warning in result.warnings) {
      expectResolves(warning);
    }
    for (final assumption in result.assumptions) {
      expectResolves(assumption);
    }
    if (result.errorCode.isNotEmpty) {
      expectResolves('error.${result.errorCode}');
    }
    // A topic that fails for its default inputs must carry a code, not text.
    if (!result.ok) {
      expect(result.errorCode, isNotEmpty, reason: '$topic has no error code');
    }
  }

  for (final spec in kTopicCatalog.values) {
    test('${spec.id} resolves its keys', () async {
      final params = <String, dynamic>{};
      for (final field in spec.fields) {
        if (field.initial.isEmpty) continue;
        params[field.key] = field.numeric
            ? (double.tryParse(field.initial) ?? 0.0)
            : field.initial;
      }
      final result = await engine.topicAsync(spec.backendId, params: params);
      expectResult(result, spec.id);
    });
  }
}
