// Debug driver for the classical scanner: every stage lands in <outdir> as a
// PNG so the pipeline can be inspected without a GUI, and --read prints the
// glyph classifier's reading of each line.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "classify.hpp"
#include "glyphs.hpp"
#include "image.hpp"
#include "preprocess.hpp"
#include "reader.hpp"
#include "segment.hpp"

#ifndef SCANNER_DATA_DIR
#define SCANNER_DATA_DIR "data"
#endif

namespace fs = std::filesystem;
using namespace infinit::scan;

namespace {

void usage() {
  std::cout << "usage: scan_debug <input-image> <outdir> [options]\n"
               "  --max-long-side N   working resolution (default 1600)\n"
               "  --min-pixels N      noise floor for components (default 6)\n"
               "  --no-deskew         keep the original rotation\n"
               "  --symbols           print every symbol box as JSON\n"
               "  --read              classify glyphs and print the reading\n"
               "  --no-clahe          skip the low-contrast CLAHE pass\n"
               "  --no-lines          keep ruled notebook lines\n"
               "  --max-skew N        deskew search limit in degrees (default 8)\n"
               "  --bank PATH         template bank (default "
            << SCANNER_DATA_DIR << "/glyph_templates.bin)\n";
}

std::string join(const fs::path& dir, const char* name) {
  return (dir / name).string();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    usage();
    return 2;
  }

  const std::string input = argv[1];
  const fs::path outdir = argv[2];
  PreprocessOptions pre;
  SegmentOptions segOptions;
  bool verbose = false;
  bool readGlyphs = false;
  std::string bankPath = std::string(SCANNER_DATA_DIR) + "/glyph_templates.bin";

  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--max-long-side" && i + 1 < argc) {
      pre.maxLongSide = std::atoi(argv[++i]);
    } else if (arg == "--min-pixels" && i + 1 < argc) {
      segOptions.minPixels = std::atoi(argv[++i]);
    } else if (arg == "--bank" && i + 1 < argc) {
      bankPath = argv[++i];
    } else if (arg == "--symbols") {
      verbose = true;
    } else if (arg == "--read") {
      readGlyphs = true;
    } else if (arg == "--no-deskew") {
      pre.maxDeskewDegrees = 0.0;
    } else if (arg == "--no-clahe") {
      pre.applyClahe = false;
    } else if (arg == "--no-lines") {
      pre.removeRuledLines = false;
    } else if (arg == "--max-skew" && i + 1 < argc) {
      pre.maxDeskewDegrees = std::atof(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "unknown argument: " << arg << "\n";
      return 2;
    }
  }

  std::error_code ec;
  fs::create_directories(outdir, ec);
  if (ec) {
    std::cerr << "cannot create output directory: " << ec.message() << "\n";
    return 1;
  }

  GrayImage source;
  try {
    source = loadImage(input);
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }

  const PreprocessResult pre_ = preprocess(source, pre);
  if (pre_.gray.empty()) {
    std::cerr << "empty image\n";
    return 1;
  }
  const Segmentation segmentation = segment(pre_.binary, segOptions);

  savePng(pre_.gray, join(outdir, "01-gray.png"));
  savePng(pre_.normalized, join(outdir, "02-normalized.png"));
  savePng(pre_.binaryOtsu, join(outdir, "03-otsu.png"));
  savePng(pre_.binarySauvola, join(outdir, "04-sauvola.png"));
  savePng(pre_.binaryAdaptive, join(outdir, "04b-adaptive.png"));
  savePng(pre_.binary, join(outdir, "05-binary.png"));
  savePng(renderSegmentation(pre_.binary, segmentation), join(outdir, "06-symbols.png"));

  TemplateBank bank;
  if (readGlyphs) {
    std::string error;
    if (!bank.load(bankPath, &error)) {
      std::cerr << error << "\n";
      return 1;
    }
  }

  std::cout << "{\n";
  std::cout << "  \"input\": \"" << input << "\",\n";
  std::cout << "  \"working\": [" << pre_.gray.width << ", " << pre_.gray.height << "],\n";
  std::cout << "  \"inverted\": " << (pre_.inverted ? "true" : "false") << ",\n";
  std::cout << "  \"skewDegrees\": " << pre_.skewDegrees << ",\n";
  std::cout << "  \"skewEstimates\": {\"baseline\": " << pre_.baselineDegrees
            << ", \"projection\": " << pre_.projectionDegrees
            << ", \"rectangle\": " << pre_.rectangleDegrees
            << ", \"hough\": " << pre_.houghDegrees << "},\n";
  std::cout << "  \"otsuThreshold\": " << pre_.otsuThreshold << ",\n";
  std::cout << "  \"binarization\": \"" << pre_.chosen << "\",\n";
  std::cout << "  \"components\": " << segmentation.components.size() << ",\n";
  std::cout << "  \"symbols\": " << segmentation.symbols.size() << ",\n";
  std::cout << "  \"lines\": " << segmentation.lines.size() << ",\n";
  if (verbose) {
    std::cout << "  \"symbolBoxes\": [";
    for (size_t i = 0; i < segmentation.symbols.size(); ++i) {
      const Symbol& symbol = segmentation.symbols[i];
      if (i) std::cout << ", ";
      std::cout << "{\"line\": " << symbol.line << ", \"box\": [" << symbol.box.x0
                << ", " << symbol.box.y0 << ", " << symbol.box.x1 << ", "
                << symbol.box.y1 << "], \"parts\": " << symbol.components.size()
                << "}";
    }
    std::cout << "],\n";
  }
  if (readGlyphs) {
    const TemplateClassifier classifier(bank);
    const ExpressionReading expression =
        readExpression(pre_.binary, segmentation, classifier);
    std::cout << "  \"expression\": \"" << expression.text << "\",\n";
    std::cout << "  \"kind\": \"" << expression.kind << "\",\n";
    std::cout << "  \"confidence\": " << expression.confidence << ",\n";
    std::cout << "  \"parts\": {";
    for (size_t i = 0; i < expression.parts.size(); ++i) {
      if (i) std::cout << ", ";
      std::cout << "\"" << expression.parts[i].name << "\": \""
                << expression.parts[i].value << "\"";
    }
    std::cout << "},\n";
    std::cout << "  \"reading\": [";
    for (size_t l = 0; l < expression.lines.size(); ++l) {
      if (l) std::cout << ", ";
      std::cout << "\"" << expression.lines[l].text << "\"";
    }
    std::cout << "],\n";
    std::cout << "  \"glyphs\": [";
    bool first = true;
    for (const LineReading& reading : expression.lines) {
      for (const SymbolReading& glyph : reading.glyphs) {
        if (!first) std::cout << ", ";
        first = false;
        std::cout << "{\"label\": \"" << glyph.match.label
                  << "\", \"score\": " << glyph.match.score
                  << ", \"runnerUp\": \"" << glyph.match.runnerUp
                  << "\", \"margin\": " << glyph.match.margin << ", \"box\": ["
                  << glyph.box.x0 << ", " << glyph.box.y0 << ", " << glyph.box.x1
                  << ", " << glyph.box.y1 << "]}";
      }
    }
    std::cout << "],\n";
  }
  std::cout << "  \"lineBoxes\": [";
  for (size_t i = 0; i < segmentation.lines.size(); ++i) {
    const TextLine& line = segmentation.lines[i];
    if (i) std::cout << ", ";
    std::cout << "[" << line.box.x0 << ", " << line.box.y0 << ", "
              << line.box.x1 << ", " << line.box.y1 << "]";
  }
  std::cout << "]\n";
  std::cout << "}\n";
  return 0;
}
