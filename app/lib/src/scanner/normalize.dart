// One import normaliser for every source: the platform image codec decodes the
// file (so HEIC/HEIF works where the OS supports it), applies EXIF rotation,
// caps the working size, and re-encodes PNG. The native scanner therefore only
// ever sees upright, bounded, self-describing pixels.
import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

/// Decodes [bytes] with the platform codec and returns an upright PNG no
/// larger than [maxSide] on the long edge. Throws [FormatException] when the
/// platform cannot decode the file.
Future<Uint8List> normalizeForScanner(
  Uint8List bytes, {
  int maxSide = 1600,
}) async {
  if (bytes.isEmpty) {
    throw const FormatException('the selected file is empty');
  }
  final buffer = await ui.ImmutableBuffer.fromUint8List(bytes);
  try {
    final descriptor = await ui.ImageDescriptor.encoded(buffer);
    try {
      final longest = math.max(descriptor.width, descriptor.height);
      final scale = longest > maxSide ? maxSide / longest : 1.0;
      final codec = await descriptor.instantiateCodec(
        targetWidth: math.max(1, (descriptor.width * scale).round()),
        targetHeight: math.max(1, (descriptor.height * scale).round()),
      );
      final frame = await codec.getNextFrame();
      try {
        final png = await frame.image.toByteData(format: ui.ImageByteFormat.png);
        if (png == null) {
          throw const FormatException('the image could not be converted');
        }
        return png.buffer.asUint8List();
      } finally {
        frame.image.dispose();
      }
    } finally {
      descriptor.dispose();
    }
  } finally {
    buffer.dispose();
  }
}
