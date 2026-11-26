// Image geometry for the scanner flow: perspective correction and a few cheap
// quality measurements. All of this is plain pixel arithmetic on the decoded
// image; nothing here depends on the native scanner.
import 'dart:isolate';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:image/image.dart' as img;

/// One cropped, upright image ready for the recognizer.
class CroppedImage {
  const CroppedImage(this.bytes, this.width, this.height);

  final Uint8List bytes;
  final int width;
  final int height;
}

/// Result of the pre-recognition quality check.
class ImageQuality {
  const ImageQuality({
    required this.width,
    required this.height,
    required this.blur,
    required this.brightness,
    this.symbolHeight = 0,
    this.symbolCount = 0,
  });

  final int width;
  final int height;

  /// Variance of the Laplacian: lower means blurrier.
  final double blur;

  /// Mean luminance in 0..1.
  final double brightness;

  /// Median ink component height, in pixels of the original image. This is the
  /// closest cheap proxy for "how tall is one symbol", which is what decides
  /// whether the photo is too far away for the classifier.
  final double symbolHeight;

  /// Ink components that passed the speck filter.
  final int symbolCount;

  bool get isBlurry => blur < 90;

  bool get isDark => brightness < 0.22;

  /// Under about 20 px a glyph loses the details the template matcher needs
  /// (dots, primes, thin fraction bars), so the plan asks for a hint.
  bool get isSmall => symbolHeight > 0 && symbolHeight < 20;
}

/// Otsu's threshold on the luminance histogram.
int _otsu(Int32List histogram, int total) {
  double sumAll = 0;
  for (var i = 0; i < 256; i++) {
    sumAll += i * histogram[i];
  }
  double sumBack = 0;
  var weightBack = 0;
  var best = 0.0;
  var threshold = 128;
  for (var t = 0; t < 256; t++) {
    weightBack += histogram[t];
    if (weightBack == 0) continue;
    final weightFore = total - weightBack;
    if (weightFore == 0) break;
    sumBack += t * histogram[t];
    final meanBack = sumBack / weightBack;
    final meanFore = (sumAll - sumBack) / weightFore;
    final between = weightBack * weightFore * (meanBack - meanFore) *
        (meanBack - meanFore);
    if (between > best) {
      best = between;
      threshold = t;
    }
  }
  return threshold;
}

/// Median height of the ink components, measured on a downscaled copy and
/// scaled back to original pixels.
///
/// This is a size estimate, not a segmentation: specks are filtered by area so
/// a paper fiber does not win, and the median survives a few merged glyphs.
({double height, int count}) _inkHeight(
  img.Image gray,
  double scale,
) {
  final w = gray.width;
  final h = gray.height;
  if (w < 8 || h < 8) return (height: 0, count: 0);
  final histogram = Int32List(256);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      histogram[gray.getPixel(x, y).r.toInt().clamp(0, 255)]++;
    }
  }
  final threshold = _otsu(histogram, w * h);
  final ink = Uint8List(w * h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      ink[y * w + x] = gray.getPixel(x, y).r <= threshold ? 1 : 0;
    }
  }

  final labels = Int32List(w * h);
  final heights = <int>[];
  final minArea = math.max(6, (w * h * 0.00008).round());
  final stack = <int>[];
  var next = 0;
  for (var start = 0; start < ink.length; start++) {
    if (ink[start] == 0 || labels[start] != 0) continue;
    next++;
    var area = 0;
    var top = h;
    var bottom = 0;
    stack
      ..clear()
      ..add(start);
    labels[start] = next;
    while (stack.isNotEmpty) {
      final index = stack.removeLast();
      final y = index ~/ w;
      final x = index % w;
      area++;
      if (y < top) top = y;
      if (y > bottom) bottom = y;
      for (var dy = -1; dy <= 1; dy++) {
        final ny = y + dy;
        if (ny < 0 || ny >= h) continue;
        for (var dx = -1; dx <= 1; dx++) {
          final nx = x + dx;
          if (nx < 0 || nx >= w) continue;
          final n = ny * w + nx;
          if (ink[n] == 0 || labels[n] != 0) continue;
          labels[n] = next;
          stack.add(n);
        }
      }
    }
    if (area >= minArea) heights.add(bottom - top + 1);
  }
  if (heights.isEmpty) return (height: 0, count: 0);
  heights.sort();
  final median = heights[heights.length ~/ 2].toDouble();
  return (height: scale > 0 ? median / scale : median, count: heights.length);
}

/// Decodes the image header and the quality statistics in one pass.
ImageQuality _measure(Uint8List bytes) {
  final decoded = img.decodeImage(bytes);
  if (decoded == null) {
    return const ImageQuality(width: 0, height: 0, blur: 0, brightness: 0);
  }
  final image = img.bakeOrientation(decoded);
  final gray = img.grayscale(image);

  // Downscale for the measurement: full resolution is noise for a hint.
  const target = 320;
  final scale = math.min(1.0, target / math.max(gray.width, gray.height));
  final small = scale < 1.0
      ? img.copyResize(
          gray,
          width: (gray.width * scale).round(),
          height: (gray.height * scale).round(),
          interpolation: img.Interpolation.average,
        )
      : gray;

  double sum = 0;
  final w = small.width;
  final h = small.height;
  final values = Float64List(w * h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      final v = small.getPixel(x, y).r.toDouble();
      values[y * w + x] = v;
      sum += v;
    }
  }

  double lapSum = 0;
  double lapSq = 0;
  var count = 0;
  for (var y = 1; y < h - 1; y++) {
    for (var x = 1; x < w - 1; x++) {
      final i = y * w + x;
      final lap =
          4 * values[i] -
          values[i - 1] -
          values[i + 1] -
          values[i - w] -
          values[i + w];
      lapSum += lap;
      lapSq += lap * lap;
      count++;
    }
  }
  final mean = count == 0 ? 0.0 : lapSum / count;
  final variance = count == 0 ? 0.0 : lapSq / count - mean * mean;

  final ink = _inkHeight(small, scale > 0 ? scale : 1.0);
  return ImageQuality(
    width: image.width,
    height: image.height,
    blur: variance,
    brightness: w * h == 0 ? 0 : sum / (w * h) / 255.0,
    symbolHeight: ink.height,
    symbolCount: ink.count,
  );
}

Future<ImageQuality> measureImage(Uint8List bytes) =>
    Isolate.run(() => _measure(bytes));

/// Solves the 8x8 system that maps the unit-ish destination rectangle
/// (0,0)-(1,1) onto [corners] (top-left, top-right, bottom-right, bottom-left).
List<double> _homography(List<math.Point<double>> corners) {
  final matrix = List.generate(8, (_) => List<double>.filled(9, 0.0));
  const us = [0.0, 1.0, 1.0, 0.0];
  const vs = [0.0, 0.0, 1.0, 1.0];
  for (var i = 0; i < 4; i++) {
    final u = us[i];
    final v = vs[i];
    final x = corners[i].x;
    final y = corners[i].y;
    matrix[2 * i]
      ..[0] = u
      ..[1] = v
      ..[2] = 1
      ..[6] = -u * x
      ..[7] = -v * x
      ..[8] = x;
    matrix[2 * i + 1]
      ..[3] = u
      ..[4] = v
      ..[5] = 1
      ..[6] = -u * y
      ..[7] = -v * y
      ..[8] = y;
  }
  for (var col = 0; col < 8; col++) {
    var pivot = col;
    for (var row = col + 1; row < 8; row++) {
      if (matrix[row][col].abs() > matrix[pivot][col].abs()) pivot = row;
    }
    if (matrix[pivot][col].abs() < 1e-9) return const [1, 0, 0, 0, 1, 0, 0, 0];
    final tmp = matrix[col];
    matrix[col] = matrix[pivot];
    matrix[pivot] = tmp;
    final div = matrix[col][col];
    for (var k = col; k < 9; k++) {
      matrix[col][k] /= div;
    }
    for (var row = 0; row < 8; row++) {
      if (row == col) continue;
      final factor = matrix[row][col];
      if (factor == 0) continue;
      for (var k = col; k < 9; k++) {
        matrix[row][k] -= factor * matrix[col][k];
      }
    }
  }
  return [for (var i = 0; i < 8; i++) matrix[i][8]];
}

double _distance(math.Point<double> a, math.Point<double> b) =>
    math.sqrt(math.pow(a.x - b.x, 2) + math.pow(a.y - b.y, 2));

CroppedImage _warp(Uint8List bytes, List<double> flatCorners, int maxSide) {
  final decoded = img.decodeImage(bytes);
  if (decoded == null) {
    return CroppedImage(bytes, 0, 0);
  }
  // EXIF orientation is applied here, so normalized corners are multiplied by
  // the upright dimensions. Flutter's decoder orients the preview the same
  // way, which keeps the handles and the pixels in the same space.
  final image = img.bakeOrientation(decoded);
  final corners = <math.Point<double>>[
    for (var i = 0; i < flatCorners.length; i += 2)
      math.Point(
        flatCorners[i] * image.width,
        flatCorners[i + 1] * image.height,
      ),
  ];
  if (corners.length != 4) {
    final png = img.encodePng(image);
    return CroppedImage(png, image.width, image.height);
  }

  final top = _distance(corners[0], corners[1]);
  final bottom = _distance(corners[3], corners[2]);
  final left = _distance(corners[0], corners[3]);
  final right = _distance(corners[1], corners[2]);
  var width = math.max(top, bottom).round();
  var height = math.max(left, right).round();
  if (width < 32 || height < 32) {
    final png = img.encodePng(image);
    return CroppedImage(png, image.width, image.height);
  }
  final longSide = math.max(width, height);
  if (longSide > maxSide) {
    final scale = maxSide / longSide;
    width = math.max(32, (width * scale).round());
    height = math.max(32, (height * scale).round());
  }

  final h = _homography(corners);
  final out = img.Image(width: width, height: height);
  for (var y = 0; y < height; y++) {
    final v = height == 1 ? 0.0 : y / (height - 1);
    for (var x = 0; x < width; x++) {
      final u = width == 1 ? 0.0 : x / (width - 1);
      final denom = h[6] * u + h[7] * v + 1.0;
      final sx = (h[0] * u + h[1] * v + h[2]) / denom;
      final sy = (h[3] * u + h[4] * v + h[5]) / denom;
      out.setPixelRgba(
        x,
        y,
        _sample(image, sx, sy, 0),
        _sample(image, sx, sy, 1),
        _sample(image, sx, sy, 2),
        255,
      );
    }
  }
  final png = img.encodePng(out);
  return CroppedImage(png, width, height);
}

int _sample(img.Image image, double x, double y, int channel) {
  final x0 = x.floor();
  final y0 = y.floor();
  final fx = x - x0;
  final fy = y - y0;
  int at(int px, int py) {
    final cx = px.clamp(0, image.width - 1);
    final cy = py.clamp(0, image.height - 1);
    final p = image.getPixel(cx, cy);
    return channel == 0
        ? p.r.toInt()
        : (channel == 1 ? p.g.toInt() : p.b.toInt());
  }

  final top = at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx;
  final bottom = at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx;
  return (top * (1 - fy) + bottom * fy).round().clamp(0, 255);
}

/// Brightness and contrast, the optional §12B.5 control. Applied to the
/// upright original, never to an already-adjusted copy, so the sliders do not
/// compound.
Uint8List _adjust(Uint8List bytes, double brightness, double contrast) {
  final decoded = img.decodeImage(bytes);
  if (decoded == null) return bytes;
  final adjusted = img.adjustColor(
    decoded,
    brightness: brightness,
    contrast: contrast,
  );
  return Uint8List.fromList(img.encodePng(adjusted));
}

Future<Uint8List> adjustImage(
  Uint8List bytes, {
  double brightness = 0,
  double contrast = 1,
}) => Isolate.run(() => _adjust(bytes, brightness, contrast));

/// Applies the four-corner crop. [corners] are normalized (0..1) positions in
/// reading order: top-left, top-right, bottom-right, bottom-left.
Future<CroppedImage> perspectiveCrop(
  Uint8List bytes,
  List<math.Point<double>> corners, {
  int maxSide = 1600,
}) {
  final flat = <double>[
    for (final corner in corners) ...[corner.x, corner.y],
  ];
  return Isolate.run(() => _warp(bytes, flat, maxSide));
}
