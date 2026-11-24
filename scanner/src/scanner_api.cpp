// C ABI for the app: one photo in, one JSON reading out.
//
// The Flutter side loads the template bank from its assets once
// (scanner_load_templates), then sends image bytes (scanner_read). Every
// returned string is allocated here and released with scanner_free.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "glyphs.hpp"
#include "image.hpp"
#include "preprocess.hpp"
#include "reader.hpp"
#include "segment.hpp"

namespace {

std::mutex gMutex;
infinit::scan::TemplateBank gBank;
bool gBankLoaded = false;

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

std::string errorJson(const std::string& message) {
  return "{\"ok\":false,\"error\":\"" + escape(message) + "\"}";
}

}  // namespace

extern "C" {

/// Loads the template bank from the bytes of glyph_templates.bin. Returns 1 on
/// success. Safe to call more than once.
int scanner_load_templates(const uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) return 0;
  std::lock_guard<std::mutex> lock(gMutex);
  std::string error;
  gBankLoaded = gBank.loadFromMemory(data, size, &error);
  return gBankLoaded ? 1 : 0;
}

/// Reads one image. Returns a malloc'd JSON document; free it with
/// scanner_free. Never returns nullptr unless allocation fails.
char* scanner_read(const uint8_t* data, std::size_t size) {
  using namespace infinit::scan;

  if (data == nullptr || size == 0) {
    return duplicate(errorJson("empty image"));
  }

  std::lock_guard<std::mutex> lock(gMutex);
  if (!gBankLoaded) {
    return duplicate(errorJson("template bank is not loaded"));
  }

  GrayImage source;
  try {
    source = decodeImage(data, size);
  } catch (const std::exception& error) {
    return duplicate(errorJson(error.what()));
  }
  if (source.empty()) return duplicate(errorJson("decoded image is empty"));

  const PreprocessResult pre = preprocess(source, PreprocessOptions{});
  const Segmentation segmentation = segment(pre.binary, SegmentOptions{});
  const ExpressionReading reading =
      readExpression(pre.binary, segmentation, gBank);

  std::string json;
  json.reserve(1024 + reading.text.size());
  json += "{\"ok\":true";
  json += ",\"expression\":\"" + escape(reading.text) + "\"";
  json += ",\"kind\":\"" + escape(reading.kind) + "\"";
  json += ",\"parts\":{";
  for (size_t i = 0; i < reading.parts.size(); ++i) {
    if (i) json += ',';
    json += "\"" + escape(reading.parts[i].name) + "\":\"" +
            escape(reading.parts[i].value) + "\"";
  }
  json += "}";
  json += ",\"confidence\":" + std::to_string(reading.confidence);
  json += ",\"skewDegrees\":" + std::to_string(pre.skewDegrees);
  json += ",\"binarization\":\"" + pre.chosen + "\"";
  json += ",\"width\":" + std::to_string(pre.gray.width);
  json += ",\"height\":" + std::to_string(pre.gray.height);
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
      json += "{\"label\":\"" + escape(glyph.match.label) + "\"";
      json += ",\"score\":" + std::to_string(glyph.match.score);
      json += ",\"margin\":" + std::to_string(glyph.match.margin);
      json += ",\"box\":[" + std::to_string(glyph.box.x0) + "," +
              std::to_string(glyph.box.y0) + "," + std::to_string(glyph.box.x1) +
              "," + std::to_string(glyph.box.y1) + "]}";
    }
  }
  json += "]}";

  return duplicate(json);
}

void scanner_free(char* text) { std::free(text); }

const char* scanner_version() { return "infinit-scanner 0.1.0"; }

}  // extern "C"
