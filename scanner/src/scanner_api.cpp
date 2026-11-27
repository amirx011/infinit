// C ABI for the app: one photo in, one JSON reading out.
//
// The Flutter side loads the template bank from its assets once
// (scanner_load_templates), then sends image bytes (scanner_read). Every
// returned string is allocated here and released with scanner_free.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "camera.hpp"
#include "glyphs.hpp"
#include "image.hpp"
#include "ink.hpp"
#include "inkproto.hpp"
#include "preprocess.hpp"
#include "reader.hpp"
#include "repair.hpp"
#include "segment.hpp"
#include "trace.hpp"

namespace {

/// Parser callback injected by the host. Returns 1 when `expression` parses,
/// 0 otherwise. `error` may be set to a host-owned string; the scanner copies
/// it and never frees it.
using ScannerValidator = int (*)(const char* expression, const char** error);

std::mutex gMutex;
/// Personal stroke prototypes. Separate from the raster prototypes because the
/// ink path can use information the bitmap lost (§12A.5).
infinit::scan::InkPrototypeBank gInkBank;
infinit::scan::TemplateBank gBank;
std::unique_ptr<infinit::scan::HybridClassifier> gClassifier;
ScannerValidator gValidator = nullptr;
std::atomic<bool> gCancelled{false};
bool gBankLoaded = false;

/// Which stage the current read is inside, so the UI can name the running
/// stage instead of showing a spinner that hides it. 0 means idle.
/// 1 prepare, 2 find symbols, 3 recognize, 4 structure, 5 check, 6 done.
std::atomic<int> gStage{0};

bool cancelled() { return gCancelled.load(std::memory_order_relaxed); }

void setStage(int stage) { gStage.store(stage, std::memory_order_relaxed); }

/// A scanned image kept between calls so the correction sheet can rebuild the
/// reading without decoding, binarising or segmenting again. Only the newest
/// session is kept: the app has one review screen at a time, and the id stops
/// a stale result from editing a newer scan.
struct Session {
  std::uint64_t id = 0;
  infinit::scan::GrayImage binary;
  infinit::scan::Segmentation segmentation;
  double skewDegrees = 0.0;
  std::string binarization = "otsu";
  std::string profile = "photo";
  int width = 0;
  int height = 0;
  /// Vision stages measured when the session opened. The reading call only
  /// re-runs classification, structure and repair.
  double preprocessMs = 0.0;
  double segmentMs = 0.0;
  infinit::scan::StructureOverrides structureOverrides;
};

std::unique_ptr<Session> gSession;
std::uint64_t gNextSession = 1;

std::string escape(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out += ' ';
        } else {
          out += c;
        }
    }
  }
  return out;
}

char* duplicate(const std::string& text) {
  char* out = static_cast<char*>(std::malloc(text.size() + 1));
  if (out == nullptr) return nullptr;
  std::memcpy(out, text.data(), text.size());
  out[text.size()] = '\0';
  return out;
}

std::string errorJson(const std::string& message,
                      const std::string& status = "failed") {
  return "{\"ok\":false,\"status\":\"" + status + "\",\"error\":\"" +
         escape(message) + "\"}";
}

/// Parses the correction map sent by the UI: {"3": "1", "7": "l"}.
/// Keys are symbol ids, values replace the classifier's label before the
/// structure is rebuilt. Malformed input simply yields no overrides.
std::map<int, std::string> parseOverrides(const char* json) {
  std::map<int, std::string> out;
  if (json == nullptr) return out;
  const std::string text = json;
  size_t i = 0;
  while (i < text.size()) {
    if (text[i] != '"') {
      ++i;
      continue;
    }
    ++i;
    const size_t keyBegin = i;
    while (i < text.size() && text[i] != '"') ++i;
    if (i >= text.size()) break;
    const std::string key = text.substr(keyBegin, i - keyBegin);
    ++i;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i >= text.size() || text[i] != ':') continue;
    ++i;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i >= text.size() || text[i] != '"') continue;
    ++i;
    std::string value;
    while (i < text.size() && text[i] != '"') {
      if (text[i] == '\\' && i + 1 < text.size()) ++i;
      value += text[i];
      ++i;
    }
    ++i;
    bool digits = !key.empty();
    for (char c : key) digits = digits && std::isdigit(static_cast<unsigned char>(c));
    if (digits) out[std::stoi(key)] = value;
  }
  return out;
}

std::string glyphJson(const infinit::scan::GlyphMatch& match,
                      const infinit::scan::Box& box, int id) {
  std::string json = "{\"id\":" + std::to_string(id);
  json += ",\"label\":\"" + escape(match.label) + "\"";
  json += ",\"score\":" + std::to_string(match.score);
  json += ",\"margin\":" + std::to_string(match.margin);
  json += ",\"box\":[" + std::to_string(box.x0) + "," +
          std::to_string(box.y0) + "," + std::to_string(box.x1) + "," +
          std::to_string(box.y1) + "]";
  json += ",\"alternatives\":[";
  for (size_t i = 0; i < match.alternatives.size(); ++i) {
    if (i) json += ',';
    json += "{\"glyph\":\"" + escape(match.alternatives[i].first) +
            "\",\"score\":" + std::to_string(match.alternatives[i].second) +
            "}";
  }
  json += "]}";
  return json;
}

struct ReadingTiming {
  double preprocessMs = 0.0;
  double segmentMs = 0.0;
  double classifyMs = 0.0;
  double structureMs = 0.0;
  double repairMs = 0.0;
};

double nowMs() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double, std::milli>(
             clock::now().time_since_epoch())
      .count();
}

std::vector<std::string> readingWarnings(
    const infinit::scan::ExpressionReading& reading) {
  std::vector<std::string> out;
  for (const infinit::scan::LineReading& line : reading.lines) {
    for (const infinit::scan::SymbolReading& glyph : line.glyphs) {
      if (glyph.match.label == "?") {
        out.push_back("unknown_construct:id=" + std::to_string(glyph.symbol));
      } else if (glyph.match.margin < 0.02) {
        out.push_back("low_margin:id=" + std::to_string(glyph.symbol));
      }
    }
  }
  return out;
}

/// The contract's symbol shape: bbox is [x, y, w, h] and `glyph` is the label.
std::string contractGlyphJson(const infinit::scan::GlyphMatch& match,
                              const infinit::scan::Box& box, int id) {
  std::string json = "{\"id\":" + std::to_string(id);
  json += ",\"bbox\":[" + std::to_string(box.x0) + "," +
          std::to_string(box.y0) + "," + std::to_string(box.width()) + "," +
          std::to_string(box.height()) + "]";
  json += ",\"glyph\":\"" + escape(match.label) + "\"";
  json += ",\"confidence\":" + std::to_string(match.score);
  json += ",\"alternatives\":[";
  for (size_t i = 0; i < match.alternatives.size(); ++i) {
    if (i) json += ',';
    json += "{\"glyph\":\"" + escape(match.alternatives[i].first) +
            "\",\"score\":" + std::to_string(match.alternatives[i].second) +
            "}";
  }
  json += "]}";
  return json;
}

std::string relationsJson(const infinit::scan::Segmentation& segmentation,
                           const infinit::scan::StructureOverrides* overrides) {
  using namespace infinit::scan;
  std::string json = "[";
  bool first = true;
  std::vector<double> heights;
  std::vector<double> widths;
  heights.reserve(segmentation.symbols.size());
  widths.reserve(segmentation.symbols.size());
  for (const Symbol& symbol : segmentation.symbols) {
    heights.push_back(symbol.box.height());
    widths.push_back(symbol.box.width());
  }
  if (heights.empty()) return "[]";
  std::sort(heights.begin(), heights.end());
  std::sort(widths.begin(), widths.end());
  const double medianH = heights[heights.size() / 2];
  const double medianW = widths[widths.size() / 2];
  for (const TextLine& line : segmentation.lines) {
    for (size_t i = 0; i < line.symbols.size(); ++i) {
      const int id = line.symbols[i];
      const Box& box = segmentation.symbols[static_cast<size_t>(id)].box;
      std::string kind;
      std::string value = "auto";
      if (medianW > 0 && box.height() <= 0.35 * box.width() &&
          box.width() >= 0.55 * medianW && box.width() <= 4.0 * medianW) {
        kind = "bar";
        if (overrides != nullptr) {
          switch (overrides->barFor(id)) {
            case BarRelation::Fraction: value = "fraction"; break;
            case BarRelation::Minus: value = "minus"; break;
            default: break;
          }
        }
      } else if (i > 0 && medianH > 0 && box.height() < 0.82 * medianH) {
        const Box& previous =
            segmentation.symbols[static_cast<size_t>(line.symbols[i - 1])].box;
        const double rise =
            static_cast<double>(previous.centerY()) - box.centerY();
        const double gap = static_cast<double>(box.x0 - previous.x1);
        if (rise > 0.12 * medianH && gap <= 0.6 * medianH) {
          kind = "script";
          if (overrides != nullptr) {
            switch (overrides->scriptFor(id)) {
              case ScriptRelation::Superscript: value = "superscript"; break;
              case ScriptRelation::Inline: value = "inline"; break;
              default: break;
            }
          }
        }
      }
      if (kind.empty()) continue;
      if (!first) json += ',';
      first = false;
      json += "{\"symbol\":" + std::to_string(id) + ",\"kind\":\"" + kind +
              "\",\"value\":\"" + value + "\"}";
    }
  }
  json += "]";
  return json;
}

std::string readingJson(const infinit::scan::ExpressionReading& reading,
                        const infinit::scan::Segmentation& segmentation,
                        double skewDegrees, const std::string& binarization,
                        int width, int height, std::uint64_t session,
                        const infinit::scan::RepairResult* repair = nullptr,
                        const ReadingTiming* timing = nullptr,
                        const std::string& profile = "photo",
                        const infinit::scan::StructureOverrides* overrides =
                            nullptr) {
  using namespace infinit::scan;
  const bool hasParse = repair != nullptr;
  std::string json;
  json.reserve(1536 + reading.text.size());
  json += "{\"ok\":true";
  json += ",\"status\":\"";
  json += (hasParse && !repair->valid) ? "partial" : "ok";
  json += "\"";
  json += ",\"session\":" + std::to_string(session);
  json += ",\"expression\":\"" + escape(reading.text) + "\"";
  json += ",\"latex\":\"" + escape(reading.latex) + "\"";
  json += ",\"kind\":\"" + escape(reading.kind) + "\"";
  json += ",\"parse_valid\":";
  json += hasParse ? (repair->valid ? "true" : "false") : "null";
  json += ",\"repaired\":";
  json += (hasParse && repair->repaired) ? "true" : "false";
  json += ",\"overrides\":{";
  if (hasParse) {
    bool firstOverride = true;
    for (const auto& entry : repair->changes) {
      if (!firstOverride) json += ',';
      firstOverride = false;
      json += "\"" + std::to_string(entry.first) + "\":\"" +
              escape(entry.second) + "\"";
    }
  }
  json += "}";
  json += ",\"error\":null";
  json += ",\"parts\":{";
  for (size_t i = 0; i < reading.parts.size(); ++i) {
    if (i) json += ',';
    json += "\"" + escape(reading.parts[i].name) + "\":\"" +
            escape(reading.parts[i].value) + "\"";
  }
  json += "}";
  json += ",\"confidence\":" + std::to_string(reading.confidence);
  json += ",\"skewDegrees\":" + std::to_string(skewDegrees);
  json += ",\"binarization\":\"" + escape(binarization) + "\"";
  json += ",\"profile\":\"" + escape(profile) + "\"";
  json += ",\"width\":" + std::to_string(width);
  json += ",\"height\":" + std::to_string(height);
  json += ",\"symbols\":" + std::to_string(segmentation.symbols.size());

  json += ",\"lines\":[";
  for (size_t i = 0; i < reading.lines.size(); ++i) {
    if (i) json += ',';
    json += "\"" + escape(reading.lines[i].text) + "\"";
  }
  json += "]";

  json += ",\"glyphs\":[";
  bool first = true;
  for (const LineReading& line : reading.lines) {
    for (const SymbolReading& glyph : line.glyphs) {
      if (!first) json += ',';
      first = false;
      json += glyphJson(glyph.match, glyph.box, glyph.symbol);
    }
  }
  json += "]";

  const std::vector<std::string> warnings = readingWarnings(reading);
  json += ",\"relations\":" + relationsJson(segmentation, overrides);

  json += ",\"warnings\":[";
  for (size_t i = 0; i < warnings.size(); ++i) {
    if (i) json += ',';
    json += "\"" + escape(warnings[i]) + "\"";
  }
  json += "]";

  if (repair != nullptr) {
    json += ",\"alternatives\":[";
    bool firstAlternative = true;
    for (const RepairCandidate& candidate : repair->candidates) {
      if (candidate.text == reading.text) continue;
      if (!firstAlternative) json += ',';
      firstAlternative = false;
      json += "{\"expression\":\"" + escape(candidate.text) + "\"";
      json += ",\"latex\":\"" + escape(candidate.latex) + "\"";
      json += ",\"cost\":" + std::to_string(candidate.cost);
      json += candidate.valid ? ",\"valid\":true}" : ",\"valid\":false}";
    }
    json += "]";
  } else {
    json += ",\"alternatives\":[]";
  }

  json += ",\"timing_ms\":{\"preprocess\":";
  json += std::to_string(timing != nullptr ? timing->preprocessMs : 0.0);
  json += ",\"segment\":";
  json += std::to_string(timing != nullptr ? timing->segmentMs : 0.0);
  json += ",\"classify\":";
  json += std::to_string(timing != nullptr ? timing->classifyMs : 0.0);
  json += ",\"structure\":";
  json += std::to_string(timing != nullptr ? timing->structureMs : 0.0);
  json += ",\"repair\":";
  json += std::to_string(timing != nullptr ? timing->repairMs : 0.0);
  json += "}";

  // One structured entry per reviewed expression. A page with several
  // independent lines gets one card per line; systems and matrices stay one
  // grouped card because their lines belong together.
  const bool multiLine =
      reading.kind == "expression" && reading.lines.size() > 1;
  if (!multiLine) {
    json += ",\"lineDetails\":[{\"line\":0";
    json += ",\"infinit\":\"" + escape(reading.text) + "\"";
    json += ",\"latex\":\"" + escape(reading.latex) + "\"";
    json += ",\"parse_valid\":";
    json += hasParse ? (repair->valid ? "true" : "false") : "null";
    json += ",\"confidence\":" + std::to_string(reading.confidence);
    json += ",\"symbols\":[";
    bool firstContract = true;
    for (const LineReading& line : reading.lines) {
      for (const SymbolReading& glyph : line.glyphs) {
        if (!firstContract) json += ',';
        firstContract = false;
        json += contractGlyphJson(glyph.match, glyph.box, glyph.symbol);
      }
    }
    json += "],\"warnings\":[";
    for (size_t i = 0; i < warnings.size(); ++i) {
      if (i) json += ',';
      json += "\"" + escape(warnings[i]) + "\"";
    }
    json += "]}]";
  } else {
    StructureOptions structure;
    if (overrides != nullptr) structure.overrides = *overrides;
    std::vector<GlyphMatch> matches(segmentation.symbols.size());
    for (const LineReading& line : reading.lines) {
      for (const SymbolReading& glyph : line.glyphs) {
        if (glyph.symbol >= 0 &&
            glyph.symbol < static_cast<int>(matches.size())) {
          matches[static_cast<size_t>(glyph.symbol)] = glyph.match;
        }
      }
    }
    json += ",\"lineDetails\":[";
    for (size_t i = 0; i < reading.lines.size(); ++i) {
      if (i) json += ',';
      const ExpressionReading lineReading =
          buildLineReading(segmentation, matches, reading.lines[i].line,
                           structure);
      json += "{\"line\":" + std::to_string(i);
      json += ",\"infinit\":\"" + escape(lineReading.text) + "\"";
      json += ",\"latex\":\"" + escape(lineReading.latex) + "\"";
      json += ",\"parse_valid\":";
      if (gValidator == nullptr) {
        json += "null";
      } else {
        const char* raw = nullptr;
        json += gValidator(lineReading.text.c_str(), &raw) != 0 ? "true"
                                                               : "false";
      }
      json += ",\"confidence\":" + std::to_string(lineReading.confidence);
      json += ",\"symbols\":[";
      bool firstLineGlyph = true;
      for (const SymbolReading& glyph : reading.lines[i].glyphs) {
        if (!firstLineGlyph) json += ',';
        firstLineGlyph = false;
        json += contractGlyphJson(glyph.match, glyph.box, glyph.symbol);
      }
      json += "],\"warnings\":[";
      for (size_t w = 0; w < warnings.size(); ++w) {
        if (w) json += ',';
        json += "\"" + escape(warnings[w]) + "\"";
      }
      json += "]}";
    }
    json += "]";
  }

  json += "}";
  return json;
}

/// One reading, shared by scanner_read and scanner_read_with_overrides.
struct ReadingOutcome {
  infinit::scan::ExpressionReading reading;
  infinit::scan::RepairResult repair;
  ReadingTiming timing;
  bool hasRepair = false;
};

/// Optional second opinion over the classified symbols, used by the ink path
/// to consult the stroke prototype bank. Runs before structure analysis, so a
/// relabelled glyph changes the tree exactly as a classifier change would.
using PostClassify =
    std::function<void(const infinit::scan::Segmentation&,
                       std::vector<infinit::scan::GlyphMatch>*)>;

ReadingOutcome readOutcome(const infinit::scan::GrayImage& binary,
                           const infinit::scan::Segmentation& segmentation,
                           const std::vector<std::string>* labels,
                           const infinit::scan::StructureOptions& structure,
                           const PostClassify& postClassify = {}) {
  using namespace infinit::scan;
  ReadingOutcome out;
  double start = nowMs();
  std::vector<GlyphMatch> matches =
      classifySymbols(binary, segmentation, *gClassifier, labels);
  out.timing.classifyMs = nowMs() - start;
  if (postClassify) postClassify(segmentation, &matches);

  setStage(4);
  start = nowMs();
  out.reading = buildReading(segmentation, matches, structure);
  out.timing.structureMs = nowMs() - start;

  if (gValidator != nullptr && out.reading.kind == "expression") {
    setStage(5);
    const Validator validator = [](const std::string& expression,
                                   std::string* error) {
      const char* raw = nullptr;
      const int ok = gValidator(expression.c_str(), &raw);
      if (ok == 0 && error != nullptr && raw != nullptr) *error = raw;
      return ok != 0;
    };
    RepairOptions options;
    options.structure = structure;
    if (labels != nullptr) {
      for (size_t i = 0; i < labels->size(); ++i) {
        if (!(*labels)[i].empty()) options.pinned.push_back(static_cast<int>(i));
      }
    }
    start = nowMs();
    out.repair = repairReading(segmentation, matches, out.reading, validator,
                               options);
    out.timing.repairMs = nowMs() - start;
    out.reading = out.repair.best;
    out.hasRepair = true;
  }
  return out;
}

char* readJson(const uint8_t* data, std::size_t size,
               const std::vector<std::string>* labels) {
  using namespace infinit::scan;
  if (cancelled()) {
    return duplicate(errorJson("cancelled: before preprocessing", "cancelled"));
  }
  ReadingTiming timing;
  setStage(1);
  double start = nowMs();
  const PreprocessResult pre =
      preprocess(decodeImage(data, size), PreprocessOptions{});
  timing.preprocessMs = nowMs() - start;
  if (cancelled()) {
    return duplicate(errorJson("cancelled: after preprocessing", "cancelled"));
  }

  setStage(2);
  start = nowMs();
  const Segmentation segmentation = segment(pre.binary, SegmentOptions{});
  timing.segmentMs = nowMs() - start;
  if (cancelled()) {
    return duplicate(errorJson("cancelled: after segmentation", "cancelled"));
  }
  if (segmentation.symbols.empty()) {
    return duplicate(errorJson("no_components: no symbols found"));
  }
  if (segmentation.overSegmented) {
    return duplicate(errorJson("over_segmented: too many symbols; crop closer"));
  }

  setStage(3);
  ReadingOutcome outcome =
      readOutcome(pre.binary, segmentation, labels, infinit::scan::StructureOptions{});
  if (cancelled()) {
    return duplicate(errorJson("cancelled: after recognition", "cancelled"));
  }
  outcome.timing.preprocessMs = timing.preprocessMs;
  outcome.timing.segmentMs = timing.segmentMs;
  setStage(6);
  return duplicate(readingJson(
      outcome.reading, segmentation, pre.skewDegrees, pre.chosen,
      pre.gray.width, pre.gray.height, 0,
      outcome.hasRepair ? &outcome.repair : nullptr, &outcome.timing,
      pre.profile));
}

std::vector<std::string> labelsFor(std::size_t count, const char* json) {
  std::vector<std::string> labels(count, std::string());
  for (const auto& entry : parseOverrides(json)) {
    if (entry.first >= 0 && entry.first < static_cast<int>(labels.size())) {
      labels[entry.first] = entry.second;
    }
  }
  return labels;
}

std::vector<std::vector<int>> componentGroups(
    const infinit::scan::Segmentation& segmentation) {
  std::vector<std::vector<int>> groups;
  groups.reserve(segmentation.symbols.size());
  for (const auto& symbol : segmentation.symbols) {
    groups.push_back(symbol.components);
  }
  return groups;
}

char* sessionJson(std::uint64_t id, const std::vector<std::string>* labels) {
  if (!gSession || gSession->id != id) {
    return duplicate(errorJson("the scan session has expired"));
  }
  infinit::scan::StructureOptions structure;
  structure.overrides = gSession->structureOverrides;
  const ReadingOutcome outcome =
      readOutcome(gSession->binary, gSession->segmentation, labels, structure);
  ReadingTiming timing = outcome.timing;
  timing.preprocessMs = gSession->preprocessMs;
  timing.segmentMs = gSession->segmentMs;
  return duplicate(readingJson(
      outcome.reading, gSession->segmentation, gSession->skewDegrees,
      gSession->binarization, gSession->width, gSession->height, gSession->id,
      outcome.hasRepair ? &outcome.repair : nullptr, &timing,
      gSession->profile, &gSession->structureOverrides));
}

}  // namespace

extern "C" {

/// Injects the Core parser. Call once before reading; pass nullptr to disable
/// repair. The callback runs on the reading thread and must be fast.
void scanner_set_validator(ScannerValidator validate) {
  std::lock_guard<std::mutex> lock(gMutex);
  gValidator = validate;
}

/// Requests cancellation of the running recognition. Safe to call from another
/// thread without blocking; the flag is checked between stages. The next read
/// resets it.
void scanner_cancel() { gCancelled.store(true, std::memory_order_relaxed); }

/// Stage of the read that is running right now, so the app can name it.
/// Read from another isolate while the worker runs; the value is an atomic.
int scanner_stage() { return gStage.load(std::memory_order_relaxed); }

/// Whether the template bank is already in memory. The isolates in the app
/// call this before loading, so a second scan never rebuilds the classifier
/// and never drops the personal prototypes attached to it.
int scanner_bank_loaded() {
  std::lock_guard<std::mutex> lock(gMutex);
  return gBankLoaded ? 1 : 0;
}

/// Loads the template bank from the bytes of glyph_templates.bin. Returns 1 on
/// success. Safe to call more than once.
int scanner_load_templates(const uint8_t* data, std::size_t size) {  if (data == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  std::string error;
  gBankLoaded = gBank.loadFromMemory(data, size, &error);
  if (gBankLoaded) {
    gClassifier = std::make_unique<infinit::scan::HybridClassifier>(gBank);
  }
  return gBankLoaded ? 1 : 0;
}

/// Loads a prototype set (Tier B, kNN). Plain data, not a model: the app can
/// ship a bundled set and merge the user's own samples. Returns 1 on success.
int scanner_load_prototypes(const uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded || !gClassifier) return 0;
  std::string error;
  return gClassifier->prototypes().loadFromMemory(data, size, &error) ? 1 : 0;
}

/// Adds one labelled sample from a PNG that contains a single symbol. Used by
/// the handwriting board to grow the user's personal prototype set.
int scanner_add_prototype(const char* label, const uint8_t* png,
                          std::size_t size) {
  if (label == nullptr || png == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded || !gClassifier) return 0;
  try {
    const infinit::scan::GrayImage image =
        infinit::scan::decodeImage(png, size);
    return gClassifier->prototypes().addFromImage(image, label) ? 1 : 0;
  } catch (const std::exception&) {
    return 0;
  }
}

/// Number of loaded prototypes, so the app can show "personal samples: N".
std::size_t scanner_prototype_count() {
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gClassifier) return 0;
  return gClassifier->prototypes().size();
}

/// Handwriting mode: the personal prototypes answer before the printed
/// templates. Returns 1 on success.
int scanner_set_handwriting_mode(int enabled) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gClassifier) return 0;
  gClassifier->setPreferPrototypes(enabled != 0);
  return 1;
}

/// Serialises the current prototype set. Returns a malloc'd buffer and writes
/// its length to out_size; free it with scanner_free_bytes. nullptr on error.
uint8_t* scanner_export_prototypes(std::size_t* out_size) {
  if (out_size == nullptr) return nullptr;
  *out_size = 0;
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gClassifier) return nullptr;
  std::vector<uint8_t> bytes;
  if (!gClassifier->prototypes().serialize(&bytes) || bytes.empty()) {
    return nullptr;
  }
  auto* out = static_cast<uint8_t*>(std::malloc(bytes.size()));
  if (out == nullptr) return nullptr;
  std::memcpy(out, bytes.data(), bytes.size());
  *out_size = bytes.size();
  return out;
}

void scanner_free_bytes(uint8_t* data) { std::free(data); }

/// Reads one image. Returns a malloc'd JSON document; free it with
/// scanner_free. Never returns nullptr unless allocation fails.
char* scanner_read(const uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) {
    return duplicate(errorJson("empty image"));
  }
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded) {
    return duplicate(errorJson("template bank is not loaded"));
  }
  gCancelled.store(false, std::memory_order_relaxed);
  try {
    return readJson(data, size, nullptr);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Same as scanner_read, but every symbol may be re-labelled before the
/// structure is rebuilt. `overrides_json` is {"symbolId": "label"}.
char* scanner_read_with_overrides(const uint8_t* data, std::size_t size,
                                  const char* overrides_json) {
  using namespace infinit::scan;
  if (data == nullptr || size == 0) {
    return duplicate(errorJson("empty image"));
  }
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded) {
    return duplicate(errorJson("template bank is not loaded"));
  }
  gCancelled.store(false, std::memory_order_relaxed);
  try {
    GrayImage source = decodeImage(data, size);
    if (source.empty()) return duplicate(errorJson("decoded image is empty"));
    // Symbol ids come from a segmentation pass; labels are indexed by id.
    const PreprocessResult pre = preprocess(source, PreprocessOptions{});
    const Segmentation segmentation = segment(pre.binary, SegmentOptions{});
    const std::vector<std::string> labels =
        labelsFor(segmentation.symbols.size(), overrides_json);
    return readJson(data, size, &labels);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Everything the ink path produces, so the session can keep the rendered
/// bitmap and the segmentation and the serializer can reuse the photo shape.
struct InkOutcome {
  bool ok = false;
  std::string error;
  infinit::scan::InkSegmentation ink;
  infinit::scan::ExpressionReading reading;
  infinit::scan::RepairResult repair;
  ReadingTiming timing;
  bool hasRepair = false;
  /// "ink" for the board's strokes, "handwriting" for strokes reconstructed
  /// from a photo (plan §12A.8, experimental).
  std::string profile = "ink";
};

InkOutcome runInkDocument(const infinit::scan::InkDocument& document,
                          const char* overrides_json,
                          const char* profile) {
  using namespace infinit::scan;
  InkOutcome out;
  out.profile = profile != nullptr ? profile : "ink";
  if (document.width == 0 || document.height == 0) {
    out.error = "invalid_ink: the stroke buffer was malformed";
    return out;
  }
  if (document.strokes.empty()) {
    out.error = "empty_ink: no strokes";
    return out;
  }
  // `document` is a parameter now; the rest of the function is unchanged.
  double start = nowMs();
  out.ink = segmentInk(document, gClassifier.get());
  out.timing.preprocessMs = nowMs() - start;
  if (out.ink.segmentation.symbols.empty()) {
    out.error = "no_components: no ink groups found";
    return out;
  }
  std::vector<std::string> labels;
  const bool hasOverrides = overrides_json != nullptr;
  if (hasOverrides) {
    labels = labelsFor(out.ink.segmentation.symbols.size(), overrides_json);
  }
  // The ink path knows which strokes made each symbol, so the personal stroke
  // bank gets a real vote. A raster prototype cannot tell a two-stroke "x"
  // from a one-stroke blob; a point cloud can.
  PostClassify strokeVote;
  if (!gInkBank.empty()) {
    const InkDocument* documentPtr = &document;
    const InkSegmentation* inkPtr = &out.ink;
    strokeVote = [documentPtr, inkPtr, hasOverrides, &labels](
                     const infinit::scan::Segmentation& segmentation,
                     std::vector<infinit::scan::GlyphMatch>* matches) {
      for (std::size_t i = 0; i < matches->size() &&
                              i < inkPtr->groups.size() &&
                              i < segmentation.symbols.size();
           ++i) {
        if (hasOverrides && i < labels.size() && !labels[i].empty()) continue;
        const CloudMatch vote = gInkBank.match(documentPtr->strokes, inkPtr->groups[i]);
        if (vote.label.empty() || !vote.confident) continue;
        if (vote.score < 0.60) continue;
        GlyphMatch& match = (*matches)[i];
        if (vote.score <= match.score + 0.05) continue;
        match.label = vote.label;
        match.runnerUp = vote.runnerUp;
        match.score = vote.score;
        match.margin = vote.margin;
        match.confident = vote.confident;
        match.alternatives = vote.alternatives;
      }
    };
  }
  ReadingOutcome reading =
      readOutcome(out.ink.binary, out.ink.segmentation,
                  hasOverrides ? &labels : nullptr,
                  infinit::scan::StructureOptions{}, strokeVote);
  reading.timing.preprocessMs = out.timing.preprocessMs;
  out.timing = reading.timing;
  out.reading = std::move(reading.reading);
  out.repair = std::move(reading.repair);
  out.hasRepair = reading.hasRepair;
  out.ok = true;
  return out;
}

std::string inkJson(const InkOutcome& out) {
  return readingJson(out.reading, out.ink.segmentation, 0.0,
                     out.profile == "handwriting" ? "handwriting" : "ink",
                     out.ink.binary.width, out.ink.binary.height, 0,
                     out.hasRepair ? &out.repair : nullptr, &out.timing,
                     out.profile.c_str());
}

/// Board strokes: decode the ink buffer, then hand it to the shared path.
InkOutcome runInk(const uint8_t* data, std::size_t size,
                  const char* overrides_json) {
  InkOutcome out;
  if (data == nullptr || size == 0) {
    out.error = "empty ink";
    return out;
  }
  return runInkDocument(infinit::scan::decodeInk(data, size), overrides_json,
                        "ink");
}

/// Reads stroke input. The JSON is the same contract as a photo read.
char* scanner_read_ink(const uint8_t* data, std::size_t size) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded) {
    return duplicate(errorJson("template bank is not loaded"));
  }
  gCancelled.store(false, std::memory_order_relaxed);
  setStage(1);
  try {
    const InkOutcome out = runInk(data, size, nullptr);
    if (!out.ok) return duplicate(errorJson(out.error));
    setStage(6);
    return duplicate(inkJson(out));
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Reads stroke input with manual labels, indexed by ink symbol id.
char* scanner_read_ink_with_overrides(const uint8_t* data, std::size_t size,
                                      const char* overrides_json) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded) {
    return duplicate(errorJson("template bank is not loaded"));
  }
  gCancelled.store(false, std::memory_order_relaxed);
  try {
    const InkOutcome out = runInk(data, size, overrides_json);
    if (!out.ok) return duplicate(errorJson(out.error));
    return duplicate(inkJson(out));
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Opens a correction session for stroke input. The rendered bitmap and the
/// ink grouping are kept, so relabelling a symbol never re-runs the front end.
std::uint64_t scanner_open_ink_session(const uint8_t* data, std::size_t size) {
  std::lock_guard<std::mutex> lock(gMutex);
  gSession.reset();
  if (!gBankLoaded) return 0;
  gCancelled.store(false, std::memory_order_relaxed);
  try {
    InkOutcome out = runInk(data, size, nullptr);
    if (!out.ok) return 0;
    auto session = std::make_unique<Session>();
    session->id = gNextSession++;
    session->binary = std::move(out.ink.binary);
    session->segmentation = std::move(out.ink.segmentation);
    session->skewDegrees = 0.0;
    session->binarization = "ink";
    session->profile = "ink";
    session->width = session->binary.width;
    session->height = session->binary.height;
    session->preprocessMs = out.timing.preprocessMs;
    session->segmentMs = 0.0;
    gSession = std::move(session);
    return gSession->id;
  } catch (const std::exception&) {
    return 0;
  }
}


/// Decodes, binarises and segments once, then keeps the result for the review
/// screen. Returns 0 when the image cannot be read; scanner_read still reports
/// the detailed error in that case.
std::uint64_t scanner_open_session(const uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  gSession.reset();
  if (!gBankLoaded) return 0;
  gCancelled.store(false, std::memory_order_relaxed);
  try {
    double start = nowMs();
    const infinit::scan::PreprocessResult pre =
        infinit::scan::preprocess(infinit::scan::decodeImage(data, size),
                                  infinit::scan::PreprocessOptions{});
    const double preprocessMs = nowMs() - start;
    if (cancelled()) return 0;
    start = nowMs();
    const infinit::scan::Segmentation segmentation = infinit::scan::segment(
        pre.binary, infinit::scan::SegmentOptions{});
    const double segmentMs = nowMs() - start;
    if (cancelled() || segmentation.overSegmented) return 0;
    auto session = std::make_unique<Session>();
    session->id = gNextSession++;
    session->binary = pre.binary;
    session->segmentation = segmentation;
    session->skewDegrees = pre.skewDegrees;
    session->binarization = pre.chosen;
    session->profile = pre.profile;
    session->width = pre.gray.width;
    session->height = pre.gray.height;
    session->preprocessMs = preprocessMs;
    session->segmentMs = segmentMs;
    gSession = std::move(session);
    return gSession->id;
  } catch (const std::exception&) {
    return 0;
  }
}

char* scanner_session_read(std::uint64_t id) {
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    return sessionJson(id, nullptr);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

char* scanner_session_relabel(std::uint64_t id, const char* overrides_json) {
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    if (!gSession || gSession->id != id) {
      return duplicate(errorJson("the scan session has expired"));
    }
    const std::vector<std::string> labels =
        labelsFor(gSession->segmentation.symbols.size(), overrides_json);
    return sessionJson(id, &labels);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Forces one interpretation of an ambiguous pair: a raised glyph as a
/// superscript or inline text, a short bar as a fraction bar or a minus. `kind`
/// is "script" or "bar"; `value` is "auto" plus the two readings. Returns the
/// rebuilt reading so the review screen can redraw in one round trip.
char* scanner_session_relation(std::uint64_t id, int symbol, const char* kind,
                               const char* value) {
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    if (!gSession || gSession->id != id) {
      return duplicate(errorJson("the scan session has expired"));
    }
    const std::string what = kind != nullptr ? kind : "";
    const std::string choice = value != nullptr ? value : "auto";
    if (symbol < 0 ||
        symbol >= static_cast<int>(gSession->segmentation.symbols.size())) {
      return duplicate(errorJson("relation needs an existing symbol"));
    }
    if (what == "script") {
      auto& slot = gSession->structureOverrides.scripts[symbol];
      if (choice == "superscript") {
        slot = infinit::scan::ScriptRelation::Superscript;
      } else if (choice == "inline") {
        slot = infinit::scan::ScriptRelation::Inline;
      } else {
        slot = infinit::scan::ScriptRelation::Auto;
      }
    } else if (what == "bar") {
      auto& slot = gSession->structureOverrides.bars[symbol];
      if (choice == "fraction") {
        slot = infinit::scan::BarRelation::Fraction;
      } else if (choice == "minus") {
        slot = infinit::scan::BarRelation::Minus;
      } else {
        slot = infinit::scan::BarRelation::Auto;
      }
    } else {
      return duplicate(errorJson("relation kind is script or bar"));
    }
    return sessionJson(id, nullptr);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Joins two neighbouring symbols into one. The component unions are regrouped
/// and the reading is rebuilt; overrides are dropped because ids change.
char* scanner_session_merge(std::uint64_t id, int first, int second) {
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    if (!gSession || gSession->id != id) {
      return duplicate(errorJson("the scan session has expired"));
    }
    std::vector<std::vector<int>> groups =
        componentGroups(gSession->segmentation);
    const int count = static_cast<int>(groups.size());
    if (first == second || first < 0 || second < 0 || first >= count ||
        second >= count) {
      return duplicate(errorJson("merge needs two different symbols"));
    }
    groups[static_cast<size_t>(first)].insert(
        groups[static_cast<size_t>(first)].end(),
        groups[static_cast<size_t>(second)].begin(),
        groups[static_cast<size_t>(second)].end());
    groups.erase(groups.begin() + second);
    gSession->segmentation =
        infinit::scan::regroup(gSession->segmentation, groups);
    return sessionJson(id, nullptr);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Splits a symbol into one symbol per connected component. This is the escape
/// hatch for touching glyphs that the merge rules glued together.
char* scanner_session_split(std::uint64_t id, int symbol) {
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    if (!gSession || gSession->id != id) {
      return duplicate(errorJson("the scan session has expired"));
    }
    std::vector<std::vector<int>> groups =
        componentGroups(gSession->segmentation);
    const int count = static_cast<int>(groups.size());
    if (symbol < 0 || symbol >= count) {
      return duplicate(errorJson("split needs an existing symbol"));
    }
    std::vector<std::vector<int>> pieces;
    pieces.reserve(groups[static_cast<size_t>(symbol)].size());
    for (int component : groups[static_cast<size_t>(symbol)]) {
      pieces.push_back(std::vector<int>{component});
    }
    if (pieces.size() < 2) {
      return sessionJson(id, nullptr);
    }
    groups.erase(groups.begin() + symbol);
    groups.insert(groups.begin() + symbol, pieces.begin(), pieces.end());
    gSession->segmentation =
        infinit::scan::regroup(gSession->segmentation, groups);
    return sessionJson(id, nullptr);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

void scanner_close_session(std::uint64_t id) {
  setStage(0);
  std::lock_guard<std::mutex> lock(gMutex);
  if (gSession && gSession->id == id) gSession.reset();
}

void scanner_free(char* text) { std::free(text); }

/// One stage of the pipeline as PNG bytes, for the Scanner Lab (plan 13.4).
///
/// `stage` is one of: gray, normalized, otsu, sauvola, adaptive, binary,
/// symbols. Unknown stages return null. The caller frees the result with
/// scanner_free_bytes.
uint8_t* scanner_debug_stage(const uint8_t* data, std::size_t size,
                            const char* stage, std::size_t* outLength) {
  if (outLength != nullptr) *outLength = 0;
  if (data == nullptr || size == 0 || stage == nullptr) return nullptr;
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    using namespace infinit::scan;
    const GrayImage input = decodeImage(data, size);
    const std::string name(stage);
    std::vector<uint8_t> png;
    if (name == "gray") {
      png = encodePng(downscaleToLongSide(input, 1600));
    } else {
      const PreprocessResult pre = preprocess(input, PreprocessOptions{});
      if (name == "normalized") {
        png = encodePng(pre.normalized);
      } else if (name == "otsu") {
        png = encodePng(pre.binaryOtsu);
      } else if (name == "sauvola") {
        png = encodePng(pre.binarySauvola);
      } else if (name == "adaptive") {
        png = encodePng(pre.binaryAdaptive);
      } else if (name == "binary") {
        png = encodePng(pre.binary);
      } else if (name == "symbols") {
        const Segmentation segmentation = segment(pre.binary, SegmentOptions{});
        png = encodePng(renderSegmentation(pre.binary, segmentation));
      } else {
        return nullptr;
      }
    }
    if (png.empty()) return nullptr;
    auto* out = static_cast<uint8_t*>(std::malloc(png.size()));
    if (out == nullptr) return nullptr;
    std::memcpy(out, png.data(), png.size());
    if (outLength != nullptr) *outLength = png.size();
    return out;
  } catch (const std::exception&) {
    return nullptr;
  }
}

/// Adds one personal stroke prototype. `data` is the same ink buffer
/// scanner_read_ink accepts (width, height, strokes), so the board can send the
/// strokes it already collected without a new encoding.
int scanner_add_ink_prototype(const char* label, const uint8_t* data,
                              std::size_t size) {
  if (label == nullptr || data == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  try {
    const infinit::scan::InkDocument document = infinit::scan::decodeInk(data, size);
    if (document.strokes.empty()) return 0;
    return gInkBank.add(std::string(label), document.strokes) ? 1 : 0;
  } catch (const std::exception&) {
    return 0;
  }
}

/// Replaces the personal stroke bank with a previously exported set.
int scanner_load_ink_prototypes(const uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  std::string error;
  return gInkBank.loadFromMemory(data, size, &error) ? 1 : 0;
}

std::size_t scanner_ink_prototype_count() { return gInkBank.size(); }

void scanner_clear_ink_prototypes() { gInkBank.clear(); }

/// The personal stroke bank in its wire format, for export and backup.
uint8_t* scanner_export_ink_prototypes(std::size_t* outLength) {
  if (outLength != nullptr) *outLength = 0;
  std::lock_guard<std::mutex> lock(gMutex);
  const std::vector<uint8_t> bytes = gInkBank.serialize();
  if (bytes.empty()) return nullptr;
  auto* out = static_cast<uint8_t*>(std::malloc(bytes.size()));
  if (out == nullptr) return nullptr;
  std::memcpy(out, bytes.data(), bytes.size());
  if (outLength != nullptr) *outLength = bytes.size();
  return out;
}

/// Photo of handwriting, read through the ink pipeline (§12A.8).
///
/// The ink is thinned to a skeleton, cut at junctions and traced back into
/// strokes, which the same grouping/classify/structure/repair path then reads.
/// Crossing strokes and shared junctions make this much weaker than live ink,
/// so the reading is labelled `handwriting` and the app shows it as
/// experimental.
char* scanner_read_handwriting(const uint8_t* data, std::size_t size) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded) {
    return duplicate(errorJson("template bank is not loaded"));
  }
  if (data == nullptr || size == 0) {
    return duplicate(errorJson("cancelled: empty image", "cancelled"));
  }
  gCancelled.store(false, std::memory_order_relaxed);
  setStage(1);
  try {
    using namespace infinit::scan;
    const GrayImage input = decodeImage(data, size);
    const PreprocessResult pre = preprocess(input, PreprocessOptions{});
    if (cancelled()) {
      return duplicate(errorJson("cancelled: after preprocessing", "cancelled"));
    }
    setStage(2);
    TraceOptions options;
    // A photo has no pen width, so the join radius has to scale with the page:
    // the median symbol height is the only size the pipeline knows.
    const int longSide = std::max(pre.binary.width, pre.binary.height);
    options.joinRadius = std::max(6.0, longSide * 0.012);
    options.minLength = std::max(6, static_cast<int>(longSide * 0.004));
    InkDocument document;
    document.width = pre.binary.width;
    document.height = pre.binary.height;
    document.strokes = traceStrokes(pre.binary, options);
    if (document.strokes.empty()) {
      return duplicate(errorJson("no_components: no strokes found"));
    }
    setStage(3);
    InkOutcome out = runInkDocument(document, nullptr, "handwriting");
    if (!out.ok) return duplicate(errorJson(out.error));
    setStage(6);
    return duplicate(inkJson(out));
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
}

/// Desktop camera feed for the app's in-app preview. Android uses the Camera2
/// channel instead, so these are stubs there; on Linux/Windows they wrap the
/// OpenCV grab thread. Bytes stay in this process, like every other scan.
int scanner_camera_available() {
  return infinit::scan::cameraSupported() ? 1 : 0;
}

/// Starts the first working device. Returns 1 on success; 0 means the app
/// should show its "camera unavailable" state.
int scanner_camera_start(int max_long_side) {
  std::string error;
  return infinit::scan::cameraStart(max_long_side > 0 ? max_long_side : 1280,
                                    &error)
             ? 1
             : 0;
}

void scanner_camera_stop() { infinit::scan::cameraStop(); }

int scanner_camera_running() {
  return infinit::scan::cameraRunning() ? 1 : 0;
}

int scanner_camera_width() { return infinit::scan::cameraWidth(); }

int scanner_camera_height() { return infinit::scan::cameraHeight(); }

/// Copies the newest JPEG frame into the caller's buffer. Returns the byte
/// count, 0 before the first frame, -1 when the buffer is too small and -2 when
/// no camera has been started.
int32_t scanner_camera_frame(uint8_t* out, int32_t capacity) {
  if (out == nullptr || capacity <= 0) return -1;
  return static_cast<int32_t>(
      infinit::scan::cameraCopyFrame(out, static_cast<std::size_t>(capacity)));
}

/// Buffer size the app allocates once per preview session.
int32_t scanner_camera_frame_capacity() {
  return static_cast<int32_t>(infinit::scan::cameraFrameCapacity());
}

/// Reason the last start or grab failed, copied into a scanner_free string.
const char* scanner_camera_error() {
  const std::string message = infinit::scan::cameraError();
  return duplicate(message);
}

const char* scanner_version() { return "infinit-scanner 0.1.0"; }

}  // extern "C"
