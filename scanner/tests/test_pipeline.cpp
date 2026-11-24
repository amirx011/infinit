// End-to-end checks on the committed samples: decode, preprocess, segment,
// classify, structure. The clean and the degraded ("_photo") render of every
// sample must produce the same expression.
#include <iostream>
#include <string>
#include <vector>

#include "glyphs.hpp"
#include "image.hpp"
#include "preprocess.hpp"
#include "reader.hpp"
#include "segment.hpp"

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
  return readExpression(pre.binary, seg, bank);
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
  }

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all pipeline tests passed\n";
  return 0;
}
