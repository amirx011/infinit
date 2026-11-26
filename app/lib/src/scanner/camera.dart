// Camera capture through the platform channel. Android hands the job to the
// system camera app, so no camera plugin or preview surface is bundled.
import 'dart:io';

import 'package:flutter/services.dart';

class CameraUnavailable implements Exception {
  CameraUnavailable(this.reason);
  final String reason;
  @override
  String toString() => reason;
}

/// Opens the system camera and returns the captured JPEG, or null when the
/// user cancels.
Future<Uint8List?> capturePhoto() async {
  const channel = MethodChannel('ir.infinit/scanner');
  try {
    final path = await channel.invokeMethod<String>('capturePhoto');
    if (path == null) return null;
    final file = File(path);
    if (!await file.exists()) throw CameraUnavailable('no_camera');
    final bytes = await file.readAsBytes();
    // The capture is a temporary working copy; the scanner keeps it in memory.
    try {
      await file.delete();
    } catch (_) {
      // A failed cleanup is harmless: the cache directory is app-private and
      // the OS reclaims it.
    }
    return bytes;
  } on MissingPluginException {
    throw CameraUnavailable('no_camera');
  } on PlatformException catch (error) {
    throw CameraUnavailable(error.code);
  }
}
