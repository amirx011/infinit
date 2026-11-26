// End-to-end checks on the committed samples: decode, preprocess, segment,
// classify, structure. The clean and the degraded ("_photo") render of every
// sample must produce the same expression.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "glyphs.hpp"
#include "image.hpp"
#include "preprocess.hpp"
#include "reader.hpp"
#include "segment.hpp"

extern "C" {
int scanner_load_templates(const unsigned char* data, std::size_t size);
std::uint64_t scanner_open_session(const unsigned char* data, std::size_t size);
char* scanner_session_read(std::uint64_t id);
char* scanner_session_relabel(std::uint64_t id, const char* overrides_json);
void scanner_close_session(std::uint64_t id);
void scanner_free(char* text);
char* scanner_read(const unsigned char* data, std::size_t size);
void scanner_cancel();
}

#ifndef SCANNER_DATA_DIR
#define SCANNER_DATA_DIR "data"
#endif
#ifndef SCANNER_SAMPLES_DIR
#define SCANNER_SAMPLES_DIR "samples"
#endif

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, bool ok, const std::string& detail = "") {
  std::cout << (ok ? "PASS " : "FAIL ") << label;
  if (!ok) {
    ++failures;
    if (!detail.empty()) std::cout << "   (" << detail << ")";
  }
  std::cout << "\n";
}

ExpressionReading readSampleReading(const std::string& name,
                                   const TemplateBank& bank) {
  const std::string path = std::string(SCANNER_SAMPLES_DIR) + "/" + name;
  const PreprocessResult pre = preprocess(loadImage(path));
  const Segmentation seg = segment(pre.binary);
  const TemplateClassifier classifier(bank);
  return readExpression(pre.binary, seg, classifier);
}

std::string readSample(const std::string& name, const TemplateBank& bank,
                       double* confidence = nullptr) {
  const ExpressionReading reading = readSampleReading(name, bank);
  if (confidence != nullptr) *confidence = reading.confidence;
  return reading.text;
}

std::string partOf(const ExpressionReading& reading, const std::string& name) {
  for (const ReadingPart& part : reading.parts) {
    if (part.name == name) return part.value;
  }
  return "<missing>";
}

std::vector<unsigned char> readBytes(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(stream),
                                    std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  TemplateBank bank;
  std::string error;
  const bool loaded =
      bank.load(std::string(SCANNER_DATA_DIR) + "/glyph_templates.bin", &error);
  check("bank loads", loaded, error);
  if (!loaded) return 1;
  check("bank size", bank.templates().size() >= 200,
        std::to_string(bank.templates().size()) + " templates");
  check("bank has labels", bank.labels().size() >= 50,
        std::to_string(bank.labels().size()) + " labels");

  struct Case {
    const char* sample;
    const char* expected;
  };
  const std::vector<Case> cases = {
      {"plain_linear.png", "3x+5=2x-7"},
      {"plain_trig.png", "sin(2x)=cos(x)"},
      {"plain_frac_like.png", "x+1=2(x-3)"},
      {"plain_linear_photo.png", "3x+5=2x-7"},
      {"plain_trig_photo.png", "sin(2x)=cos(x)"},
      {"plain_frac_like_photo.png", "x+1=2(x-3)"},
      {"quadratic.png", "x^2-5x+6=0"},
      {"quadratic_photo.png", "x^2-5x+6=0"},
      {"power.png", "2^(x+1)=8"},
      {"power_photo.png", "2^(x+1)=8"},
      {"fraction.png", "(x+1)/(x-3)=2"},
      {"fraction_photo.png", "(x+1)/(x-3)=2"},
      {"nested_fraction.png", "(1/x+1)/(x^2-4)"},
      {"root.png", "sqrt(x^2+9)=x-1"},
      {"root_photo.png", "sqrt(x^2+9)=x-1"},
      {"derivative.png", "(dy)/(dx)=3x^2-2x"},
      {"derivative_photo.png", "(dy)/(dx)=3x^2-2x"},
      {"log.png", "ln(x+1)+ln(x-1)=0"},
      {"sum.png", "sum(1/(n^2), n=1, inf)"},
      {"sum_photo.png", "sum(1/(n^2), n=1, inf)"},
      {"integral.png", "integral(x^2, x, 0, 1)"},
      {"integral_photo.png", "integral(x^2, x, 0, 1)"},
      {"limit.png", "lim(sin(x)/x, x->0)"},
      {"limit_photo.png", "lim(sin(x)/x, x->0)"},
      {"system.png", "2x+y=5,x-y=1"},
      {"system_photo.png", "2x+y=5,x-y=1"},
  };

  for (const Case& item : cases) {
    double confidence = 0.0;
    const std::string text = readSample(item.sample, bank, &confidence);
    check(std::string("read ") + item.sample, text == item.expected,
          "got \"" + text + "\", want \"" + item.expected + "\"");
    check(std::string("confidence ") + item.sample, confidence > 0.5,
          std::to_string(confidence));
  }

  // Structured readings must name their parts, so the app can route a scan to
  // the series, integral or limit workbench instead of guessing from the text.
  {
    const ExpressionReading sum = readSampleReading("sum.png", bank);
    check("sum kind", sum.kind == "sum", sum.kind);
    check("sum term", partOf(sum, "term") == "1/(n^2)", partOf(sum, "term"));
    check("sum index", partOf(sum, "index") == "n", partOf(sum, "index"));
    check("sum start", partOf(sum, "start") == "1", partOf(sum, "start"));
    check("sum end", partOf(sum, "end") == "inf", partOf(sum, "end"));

    const ExpressionReading integral = readSampleReading("integral.png", bank);
    check("integral kind", integral.kind == "integral", integral.kind);
    check("integral integrand", partOf(integral, "integrand") == "x^2",
          partOf(integral, "integrand"));
    check("integral variable", partOf(integral, "variable") == "x",
          partOf(integral, "variable"));
    check("integral bounds",
          partOf(integral, "lower") == "0" && partOf(integral, "upper") == "1",
          partOf(integral, "lower") + ".." + partOf(integral, "upper"));

    const ExpressionReading limit = readSampleReading("limit.png", bank);
    check("limit kind", limit.kind == "limit", limit.kind);
    check("limit expression", partOf(limit, "expression") == "sin(x)/x",
          partOf(limit, "expression"));
    check("limit point",
          partOf(limit, "variable") == "x" && partOf(limit, "point") == "0",
          partOf(limit, "variable") + "->" + partOf(limit, "point"));

    const ExpressionReading system = readSampleReading("system.png", bank);
    check("system kind", system.kind == "system", system.kind);
    check("system first", partOf(system, "equation1") == "2x+y=5",
          partOf(system, "equation1"));
    check("system second", partOf(system, "equation2") == "x-y=1",
          partOf(system, "equation2"));

    // A bracketed grid on clean paper and on the degraded photo must both give
    // the same cells: the shape is geometry, not pixel luck.
    for (const char* sample : {"matrix.png", "matrix_photo.png"}) {
      const ExpressionReading matrix = readSampleReading(sample, bank);
      check(std::string("matrix kind ") + sample, matrix.kind == "matrix",
            matrix.kind);
      check(std::string("matrix cells ") + sample,
            partOf(matrix, "matrix") == "1, 2; 3, 4",
            partOf(matrix, "matrix"));
      check(std::string("matrix shape ") + sample,
            partOf(matrix, "rows") == "2" &&
                partOf(matrix, "columns") == "2",
            partOf(matrix, "rows") + "x" + partOf(matrix, "columns"));
    }
  }

  // The session API keeps the binarised image and the segmentation, so the
  // correction sheet can rebuild a reading from a new label map alone.
  const std::vector<unsigned char> bankBytes =
      readBytes(std::string(SCANNER_DATA_DIR) + "/glyph_templates.bin");
  const std::vector<unsigned char> imageBytes =
      readBytes(std::string(SCANNER_SAMPLES_DIR) + "/quadratic.png");
  check("session bank loads",
        scanner_load_templates(bankBytes.data(), bankBytes.size()) == 1);
  const std::uint64_t session =
      scanner_open_session(imageBytes.data(), imageBytes.size());
  check("session opens", session != 0);

  char* raw = scanner_session_read(session);
  const std::string first = raw == nullptr ? "" : raw;
  scanner_free(raw);
  check("session reads", first.find("\"ok\":true") != std::string::npos);
  check("session id is in the json",
        first.find("\"session\":" + std::to_string(session)) !=
            std::string::npos);
  check("session expression", first.find("x^2-5x+6=0") != std::string::npos,
        first);

  char* fixed = scanner_session_relabel(session, "{\"0\":\"z\"}");
  const std::string changed = fixed == nullptr ? "" : fixed;
  scanner_free(fixed);
  check("relabel keeps the session",
        changed.find("\"ok\":true") != std::string::npos, changed);
  check("relabel changes the reading",
        changed.find("\"expression\":\"x^2-5x+6=0\"") == std::string::npos,
        changed);
  check("relabel uses the new label", changed.find("z") != std::string::npos,
        changed);

  char* stale = scanner_session_relabel(session + 1, "{\"0\":\"z\"}");
  check("stale session is rejected",
        stale != nullptr &&
            std::string(stale).find("\"ok\":false") != std::string::npos);
  scanner_free(stale);
  scanner_close_session(session);
  char* closed = scanner_session_read(session);
  check("closed session is rejected",
        closed != nullptr &&
            std::string(closed).find("\"ok\":false") != std::string::npos);
  scanner_free(closed);

  // The output contract: status, parse validity, per-stage timing and the
  // structured line entry are all present even without an injected parser.
  char* contract = scanner_read(imageBytes.data(), imageBytes.size());
  const std::string document = contract == nullptr ? "" : contract;
  scanner_free(contract);
  check("contract has a status",
        document.find("\"status\":\"ok\"") != std::string::npos);
  check("contract has parse_valid",
        document.find("\"parse_valid\":null") != std::string::npos);
  check("contract has timing",
        document.find("\"timing_ms\":{") != std::string::npos);
  check("contract has line details",
        document.find("\"lineDetails\":[") != std::string::npos);
  check("contract has no error on success",
        document.find("\"error\":null") != std::string::npos);

  // A cancel request before a read is cleared by the next call, so a stale
  // request can never poison a fresh scan.
  scanner_cancel();
  char* afterCancel = scanner_read(imageBytes.data(), imageBytes.size());
  check("a fresh read clears a stale cancel",
        afterCancel != nullptr &&
            std::string(afterCancel).find("\"ok\":true") != std::string::npos);
  scanner_free(afterCancel);

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all pipeline tests passed\n";
  return 0;
}
