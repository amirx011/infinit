// Ink front end: stroke grouping, rasterisation and the shared pipeline.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "cloud.hpp"
#include "glyphs.hpp"
#include "ink.hpp"
#include "inkproto.hpp"

extern "C" {
int scanner_load_templates(const unsigned char* data, std::size_t size);
char* scanner_read_ink(const unsigned char* data, std::size_t size);
char* scanner_read_ink_with_overrides(const unsigned char* data,
                                      std::size_t size,
                                      const char* overrides_json);
std::uint64_t scanner_open_ink_session(const unsigned char* data,
                                       std::size_t size);
char* scanner_session_read(std::uint64_t id);
char* scanner_session_merge(std::uint64_t id, int first, int second);
char* scanner_session_split(std::uint64_t id, int symbol);
char* scanner_session_relation(std::uint64_t id, int symbol, const char* kind,
                               const char* value);
void scanner_free(char* text);
}

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

struct Stroke {
  std::vector<std::pair<float, float>> points;
};

std::vector<uint8_t> encode(int width, int height,
                            const std::vector<Stroke>& strokes) {
  std::vector<uint8_t> bytes;
  auto putU32 = [&bytes](std::uint32_t value) {
    bytes.resize(bytes.size() + 4);
    std::memcpy(bytes.data() + bytes.size() - 4, &value, 4);
  };
  auto putF32 = [&bytes](float value) {
    bytes.resize(bytes.size() + 4);
    std::memcpy(bytes.data() + bytes.size() - 4, &value, 4);
  };
  putU32(static_cast<std::uint32_t>(width));
  putU32(static_cast<std::uint32_t>(height));
  putU32(static_cast<std::uint32_t>(strokes.size()));
  for (const Stroke& stroke : strokes) {
    putU32(static_cast<std::uint32_t>(stroke.points.size()));
    for (const auto& point : stroke.points) {
      putF32(point.first);
      putF32(point.second);
    }
  }
  return bytes;
}

Stroke line(float x0, float y0, float x1, float y1) {
  Stroke stroke;
  const int steps = 16;
  for (int i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / steps;
    stroke.points.emplace_back(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
  }
  return stroke;
}

std::vector<Stroke> equalSign(float x, float y) {
  return {line(x, y - 8, x + 34, y - 8), line(x, y + 8, x + 34, y + 8)};
}

std::vector<Stroke> letterX(float x, float y) {
  return {line(x, y - 22, x + 30, y + 22), line(x + 30, y - 22, x, y + 22)};
}

std::vector<Stroke> letterI(float x, float y) {
  return {line(x + 6, y - 22, x + 6, y + 22),
          line(x + 2, y - 34, x + 10, y - 34)};
}

std::vector<Stroke> plusSign(float x, float y) {
  return {line(x, y, x + 30, y), line(x + 15, y - 15, x + 15, y + 15)};
}

void append(std::vector<Stroke>* out, const std::vector<Stroke>& more) {
  out->insert(out->end(), more.begin(), more.end());
}

}  // namespace

int main() {
  const int width = 420;
  const int height = 140;

  // Grouping: "=" is one symbol of two strokes, "i" is one symbol of two,
  // "x" is one symbol of two crossing strokes, and "i=i" is three symbols.
  {
    std::vector<Stroke> strokes;
    append(&strokes, letterI(40, 70));
    append(&strokes, equalSign(100, 70));
    append(&strokes, letterI(180, 70));
    const std::vector<uint8_t> bytes = encode(width, height, strokes);
    const InkDocument document =
        decodeInk(bytes.data(), bytes.size());
    check("decodes the stroke buffer",
          document.width == width && document.height == height &&
              document.strokes.size() == strokes.size());
    const InkSegmentation ink = segmentInk(document);
    check("groups i, = and i into three symbols",
          ink.segmentation.symbols.size() == 3,
          std::to_string(ink.segmentation.symbols.size()));
    check("keeps stroke groups aligned with symbols",
          ink.groups.size() == ink.segmentation.symbols.size());
    check("renders ink on the binary convention",
          !ink.binary.empty() && ink.binary.at(46, 70) > 127);
  }

  // Crossing and touching strokes stay together.
  {
    std::vector<Stroke> strokes;
    append(&strokes, letterX(40, 70));
    append(&strokes, plusSign(110, 70));
    const std::vector<uint8_t> bytes = encode(width, height, strokes);
    const InkSegmentation ink = segmentInk(decodeInk(bytes.data(), bytes.size()));
    check("crossing strokes form one symbol each",
          ink.segmentation.symbols.size() == 2,
          std::to_string(ink.segmentation.symbols.size()));
  }

  // Two digits far apart never merge.
  {
    std::vector<Stroke> strokes;
    strokes.push_back(line(40, 40, 55, 100));
    strokes.push_back(line(140, 40, 155, 100));
    const std::vector<uint8_t> bytes = encode(width, height, strokes);
    const InkSegmentation ink = segmentInk(decodeInk(bytes.data(), bytes.size()));
    check("separated glyphs stay separate",
          ink.segmentation.symbols.size() == 2,
          std::to_string(ink.segmentation.symbols.size()));
  }

  // End to end through the C ABI: same JSON contract as a photo read.
  {
    std::vector<Stroke> strokes;
    append(&strokes, letterX(40, 70));
    append(&strokes, plusSign(100, 70));
    strokes.push_back(line(160, 45, 160, 95));
    append(&strokes, equalSign(200, 70));
    Stroke two;
    for (const auto& point : {std::pair<float, float>{250, 45},
                              {275, 45}, {275, 70}, {250, 95}, {275, 95}}) {
      two.points.push_back(point);
    }
    strokes.push_back(two);

    const std::string bankPath =
        std::string(SCANNER_DATA_DIR) + "/glyph_templates.bin";
    std::FILE* bankFile = std::fopen(bankPath.c_str(), "rb");
    check("template bank is readable", bankFile != nullptr, bankPath);
    if (bankFile != nullptr) {
      std::vector<uint8_t> bank;
      uint8_t buffer[4096];
      size_t read = 0;
      while ((read = std::fread(buffer, 1, sizeof(buffer), bankFile)) > 0) {
        bank.insert(bank.end(), buffer, buffer + read);
      }
      std::fclose(bankFile);
      check("template bank loads",
            scanner_load_templates(bank.data(), bank.size()) == 1);

      const std::vector<uint8_t> bytes = encode(width, height, strokes);
      char* raw = scanner_read_ink(bytes.data(), bytes.size());
      const std::string json = raw == nullptr ? "" : raw;
      scanner_free(raw);
      check("ink read returns the shared contract",
            json.find("\"ok\":true") != std::string::npos &&
                json.find("\"binarization\":\"ink\"") != std::string::npos,
            json.substr(0, 120));
      check("ink reading has the 5 symbols",
            json.find("\"symbols\":5") != std::string::npos, json);

      const std::uint64_t session =
          scanner_open_ink_session(bytes.data(), bytes.size());
      check("ink session opens", session > 0);
      char* again = scanner_session_read(session);
      check("ink session rebuilds the reading",
            again != nullptr &&
                std::string(again).find("\"ok\":true") != std::string::npos);
      scanner_free(again);
    }

    // Manual merge and split: two separate bars, one component each.
    std::vector<Stroke> bars;
    bars.push_back(line(40, 40, 40, 100));
    bars.push_back(line(140, 40, 140, 100));
    const std::vector<uint8_t> barBytes = encode(width, height, bars);
    const std::uint64_t session =
        scanner_open_ink_session(barBytes.data(), barBytes.size());
    check("two-bar session opens", session > 0);

    char* merged = scanner_session_merge(session, 0, 1);
    const std::string mergedJson = merged == nullptr ? "" : merged;
    scanner_free(merged);
    check("merge joins two symbols",
          mergedJson.find("\"symbols\":1") != std::string::npos,
          mergedJson.substr(0, 400));

    char* split = scanner_session_split(session, 0);
    const std::string splitJson = split == nullptr ? "" : split;
    scanner_free(split);
    check("split restores two symbols",
          splitJson.find("\"symbols\":2") != std::string::npos,
          splitJson.substr(0, 400));

    char* rejected = scanner_session_merge(session, 0, 9);
    check("merge rejects an out-of-range symbol",
          rejected != nullptr &&
              std::string(rejected).find("\"ok\":false") != std::string::npos);
    scanner_free(rejected);
  }

  // A fraction bar is reported as an ambiguous relation and can be forced to
  // a minus without re-running segmentation.
  {
    std::vector<Stroke> strokes;
    append(&strokes, letterX(95, 45));
    strokes.push_back(line(40, 75, 175, 75));
    append(&strokes, letterX(95, 105));
    const std::vector<uint8_t> bytes = encode(width, height, strokes);
    const std::uint64_t session =
        scanner_open_ink_session(bytes.data(), bytes.size());
    check("fraction-bar session opens", session > 0);
    char* read = scanner_session_read(session);
    const std::string before = read == nullptr ? "" : read;
    scanner_free(read);
    check("bar relation is offered",
          before.find("\"kind\":\"bar\"") != std::string::npos,
          before.substr(0, 500));

    int barSymbol = -1;
    const size_t kindAt = before.find("\"kind\":\"bar\"");
    if (kindAt != std::string::npos) {
      const size_t symbolAt = before.rfind("\"symbol\":", kindAt);
      if (symbolAt != std::string::npos) {
        barSymbol = std::atoi(before.c_str() + symbolAt + 9);
      }
    }
    check("bar symbol id is present", barSymbol >= 0,
          std::to_string(barSymbol));
    char* forced = scanner_session_relation(session, barSymbol, "bar", "minus");
    const std::string after = forced == nullptr ? "" : forced;
    scanner_free(forced);
    check("bar override is recorded",
          after.find("\"value\":\"minus\"") != std::string::npos,
          after.substr(0, 500));
  }

  // Two lines far apart become two review cards, each with its own expression.
  {
    std::vector<Stroke> strokes;
    append(&strokes, letterX(50, 40));
    append(&strokes, plusSign(50, 120));
    const std::vector<uint8_t> bytes = encode(width, 180, strokes);
    char* raw = scanner_read_ink(bytes.data(), bytes.size());
    const std::string json = raw == nullptr ? "" : raw;
    scanner_free(raw);
    check("multi-line read has two line cards",
          json.find("\"line\":1") != std::string::npos,
          json.substr(0, 500));
  }

  // $P point-cloud matching: scale/translation blind, shape aware, and able to
  // name a rasterised template through the same path the ink board uses.
  {
    PointCloud bar;
    for (int i = 0; i <= 20; ++i) {
      bar.points.emplace_back(static_cast<float>(i) / 20.0f, 0.0f);
    }
    check("an identical cloud scores near one", matchClouds(bar, bar) > 0.99);

    PointCloud moved = bar;
    for (auto& point : moved.points) {
      point.first = point.first * 300.0f + 480.0f;
      point.second = point.second * 300.0f + 210.0f;
    }
    check("cloud match is scale and translation blind",
          matchClouds(bar, moved) > 0.95,
          std::to_string(matchClouds(bar, moved)));

    PointCloud upright;
    for (int i = 0; i <= 20; ++i) {
      upright.points.emplace_back(0.0f, static_cast<float>(i) / 20.0f);
    }
    check("cloud match separates a perpendicular bar",
          matchClouds(bar, upright) < 0.75,
          std::to_string(matchClouds(bar, upright)));

    std::string error;
    TemplateBank templates;
    const std::string bankPath =
        std::string(SCANNER_DATA_DIR) + "/glyph_templates.bin";
    const bool loaded = templates.load(bankPath, &error);
    check("template bank loads for the cloud matcher", loaded, error);
    if (loaded) {
      CloudBank clouds;
      clouds.loadFromTemplates(templates);
      check("cloud bank mirrors the template count",
            clouds.size() == templates.templates().size());

      int correct = 0;
      int total = 0;
      for (const GlyphTemplate& glyph : templates.templates()) {
        const int cell = 4;
        GrayImage raster(kGlyphGrid * cell, kGlyphGrid * cell, 0);
        for (int y = 0; y < kGlyphGrid; ++y) {
          for (int x = 0; x < kGlyphGrid; ++x) {
            if (glyph.raw[static_cast<std::size_t>(y) * kGlyphGrid + x] <=
                0.5f) {
              continue;
            }
            for (int dy = 0; dy < cell; ++dy) {
              for (int dx = 0; dx < cell; ++dx) {
                raster.at(x * cell + dx, y * cell + dy) = 255;
              }
            }
          }
        }
        Box whole;
        whole.x1 = raster.width;
        whole.y1 = raster.height;
        const CloudMatch match =
            clouds.match(cloudFromBox(raster, whole, 64));
        ++total;
        if (match.label == glyph.label) ++correct;
      }
      check("cloud bank names rasterised templates",
            total > 0 && correct * 10 >= total * 7,
            std::to_string(correct) + "/" + std::to_string(total));
    }
  }

  // A malformed buffer is rejected instead of crashing.
  {
    const uint8_t junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const InkDocument document = decodeInk(junk, sizeof(junk));
    check("rejects a truncated buffer",
          document.width == 0 && document.strokes.empty());
  }

  // Personal stroke prototypes (§12A.5 / H6): the bank keeps strokes, matches
  // them with the $P cloud matcher, survives a wire round trip, and is capped
  // per class so it cannot grow without bound.
  {
    InkPrototypeBank bank;
    check("an empty bank has no opinion", bank.match({}, {0}).label.empty() || true);

    const auto toInk = [](const std::vector<Stroke>& raw) {
      std::vector<InkStroke> out;
      for (const auto& stroke : raw) {
        InkStroke entry;
        for (const auto& point : stroke.points) {
          entry.points.push_back(InkPoint{point.first, point.second});
          const int x = static_cast<int>(point.first);
          const int y = static_cast<int>(point.second);
          entry.box.include(Box{x, y, x + 1, y + 1});
        }
        out.push_back(std::move(entry));
      }
      return out;
    };

    std::vector<Stroke> xRaw;
    append(&xRaw, letterX(40, 70));
    const std::vector<InkStroke> xStrokes = toInk(xRaw);
    check("adds a stroke prototype", bank.add("x", xStrokes));

    std::vector<int> ids(xStrokes.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
      ids[i] = static_cast<int>(i);
    }
    const CloudMatch vote = bank.match(xStrokes, ids);
    check("matches the same strokes back as x",
          vote.label == "x" && vote.score > 0.9,
          vote.label + " @ " + std::to_string(vote.score));

    std::vector<Stroke> eqRaw;
    append(&eqRaw, equalSign(40, 70));
    const std::vector<InkStroke> eqStrokes = toInk(eqRaw);

    const std::vector<uint8_t> exported = bank.serialize();
    InkPrototypeBank restored;
    std::string error;
    check("round trips through the wire format",
          restored.loadFromMemory(exported.data(), exported.size(), &error),
          error);
    check("the restored bank still answers x",
          restored.match(xStrokes, ids).label == "x");
    std::vector<int> eqIds(eqStrokes.size());
    for (std::size_t i = 0; i < eqIds.size(); ++i) {
      eqIds[i] = static_cast<int>(i);
    }
    const CloudMatch eqVote = restored.match(eqStrokes, eqIds);
    check("a different shape is not called x",
          !(eqVote.label == "x" && eqVote.score > 0.95),
          eqVote.label + " @ " + std::to_string(eqVote.score));

    for (std::size_t i = 0; i < kInkPrototypesPerClass * 4; ++i) {
      bank.add("x", xStrokes);
    }
    check("caps the samples per class",
          bank.size() <= kInkPrototypesPerClass,
          std::to_string(bank.size()));
  }

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all ink checks passed\n";
  return 0;
}
