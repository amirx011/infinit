// Camera screen states (§12B.4 and §12B.13). The preview itself is a native
// texture, so the tests drive the platform channel: permission refused, camera
// unavailable, and a working preview. Nothing here opens a real camera.
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:infinit/src/i18n/catalog.dart';
import 'package:infinit/src/l10n.dart';
import 'package:infinit/src/scanner/camera_page.dart';
import 'package:infinit/src/theme.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const channel = MethodChannel('ir.infinit/camera');
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;

  setUpAll(() async {
    I18n.instance = await I18n.load();
  });

  tearDown(() {
    messenger.setMockMethodCallHandler(channel, null);
  });

  Future<void> pumpCamera(WidgetTester tester, L10n l10n) async {
    await tester.pumpWidget(
      MaterialApp(
        theme: buildOdeTheme(dark: false),
        // The test host is Linux, so pin the Camera2 channel path every time.
        home: CameraScanPage(l10n: l10n, useDesktopFeed: false),
      ),
    );
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 50));
  }

  testWidgets('permission denied explains itself and offers a retry', (
    tester,
  ) async {
    messenger.setMockMethodCallHandler(channel, (call) async {
      if (call.method == 'previewStart') {
        throw PlatformException(code: 'permission');
      }
      return null;
    });

    final l10n = const L10n(Locale('en'));
    await pumpCamera(tester, l10n);

    expect(find.text(l10n.t('scan.camera.permission')), findsOneWidget);
    // Never a dead end: retry re-asks for the permission.
    expect(find.text(l10n.t('scan.camera.retry')), findsOneWidget);
  });

  testWidgets('an unavailable camera says so instead of showing a blank box', (
    tester,
  ) async {
    messenger.setMockMethodCallHandler(channel, (call) async {
      if (call.method == 'previewStart') return null;
      return null;
    });

    final l10n = const L10n(Locale('en'));
    await pumpCamera(tester, l10n);
    expect(find.text(l10n.t('scan.camera.unavailable')), findsOneWidget);
  });

  testWidgets('a working preview shows the shutter and the framing hint', (
    tester,
  ) async {
    messenger.setMockMethodCallHandler(channel, (call) async {
      switch (call.method) {
        case 'previewStart':
          return <String, dynamic>{
            'textureId': 7,
            'rotation': 0,
            'width': 1080,
            'height': 1920,
            'zoom': 1.0,
            'maxZoom': 4.0,
          };
        default:
          return null;
      }
    });

    final l10n = const L10n(Locale('en'));
    await pumpCamera(tester, l10n);

    expect(find.text(l10n.t('scan.camera.hint')), findsOneWidget);
    expect(find.byTooltip(l10n.t('scan.camera.capture')), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('the camera screen is localized in Persian', (tester) async {
    messenger.setMockMethodCallHandler(channel, (call) async {
      if (call.method == 'previewStart') return null;
      return null;
    });

    final l10n = const L10n(Locale('fa'));
    await pumpCamera(tester, l10n);
    expect(find.text(l10n.t('scan.camera.title')), findsOneWidget);
  });
}
