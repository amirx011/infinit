// Structure tests with hand-placed boxes: no image, no classifier, just the
// geometry rules that turn glyphs into fractions, powers and radicals.
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "structure.hpp"

using namespace infinit::scan;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& got,
           const std::string& want) {
  const bool ok = got == want;
  std::cout << (ok ? "PASS " : "FAIL ") << label << "  got: " << got;
  if (!ok) {
    ++failures;
    std::cout << "   want: " << want;
  }
  std::cout << "\n";
}

struct Placement {
  std::string label;
  Box box;
};

std::string build(const std::vector<Placement>& placements) {
  Segmentation segmentation;
  std::vector<GlyphMatch> matches;
  int id = 0;
  for (const Placement& placement : placements) {
    Symbol symbol;
    symbol.id = id++;
    symbol.box = placement.box;
    symbol.line = 0;
    segmentation.symbols.push_back(symbol);

    GlyphMatch match;
    match.label = placement.label;
    match.score = 0.95;
    match.margin = 0.30;
    match.confident = true;
    matches.push_back(match);
  }
  return renderStructure(buildStructure(segmentation, matches));
}

void testRow() {
  check("row", build({{"x", {0, 40, 30, 80}},
                      {"+", {40, 50, 60, 62}},
                      {"1", {70, 40, 100, 80}}}),
        "x+1");
}

void testPower() {
  check("power", build({{"x", {0, 40, 30, 80}}, {"2", {34, 8, 56, 34}}}),
        "x^2");
  check("power of sum", build({{"x", {0, 40, 30, 80}},
                               {"2", {34, 8, 56, 34}},
                               {"+", {60, 8, 82, 20}},
                               {"1", {86, 8, 108, 34}}}),
        "x^(2+1)");
}

void testFraction() {
  check("fraction", build({{"x", {10, 20, 40, 48}},
                           {"-", {0, 52, 60, 58}},
                           {"y", {10, 62, 40, 90}}}),
        "x/y");
  check("fraction in a row", build({{"3", {0, 40, 30, 80}},
                                    {"-", {36, 52, 96, 58}},
                                    {"x", {46, 20, 76, 48}},
                                    {"y", {46, 62, 76, 90}}}),
        "3*x/y");
}

void testNestedFraction() {
  check("nested fraction", build({{"1", {20, 10, 40, 38}},
                                  {"-", {12, 42, 48, 48}},
                                  {"x", {20, 52, 40, 80}},
                                  {"+", {56, 30, 78, 42}},
                                  {"1", {86, 16, 106, 44}},
                                  {"-", {4, 90, 116, 96}},
                                  {"y", {20, 100, 40, 128}},
                                  {"+", {56, 108, 78, 120}},
                                  {"2", {86, 100, 106, 128}}}),
        "(1/x+1)/(y+2)");
}

void testRadical() {
  check("radical", build({{"√", {0, 0, 160, 80}}, {"x", {40, 40, 70, 68}}}),
        "sqrt(x)");
  check("radical with power", build({{"√", {0, 0, 200, 80}},
                                     {"x", {40, 40, 70, 68}},
                                     {"2", {74, 8, 96, 34}},
                                     {"+", {100, 44, 122, 56}},
                                     {"1", {126, 34, 146, 62}}}),
        "sqrt(x^2+1)");
}

void testSum() {
  check("sum", build({{"sum", {0, 60, 120, 240}},
                      {"inf", {20, 10, 80, 40}},
                      {"n", {10, 260, 50, 300}},
                      {"=", {60, 270, 100, 285}},
                      {"1", {110, 260, 140, 300}},
                      {"1", {180, 80, 210, 120}},
                      {"-", {160, 130, 260, 138}},
                      {"n", {180, 150, 210, 190}},
                      {"2", {214, 140, 236, 166}}}),
        "sum(1/(n^2), n=1, inf)");
}

void testIntegral() {
  check("integral", build({{"integral", {0, 20, 40, 200}},
                           {"1", {50, 0, 70, 40}},
                           {"0", {50, 170, 70, 210}},
                           {"x", {90, 100, 120, 140}},
                           {"2", {124, 80, 136, 100}},
                           {"d", {150, 90, 175, 150}},
                           {"x", {178, 100, 200, 140}}}),
        "integral(x^2, x, 0, 1)");
  check("indefinite integral", build({{"integral", {0, 20, 40, 200}},
                                      {"x", {90, 100, 120, 140}},
                                      {"d", {150, 90, 175, 150}},
                                      {"x", {178, 100, 200, 140}}}),
        "integral(x, x)");
}

void testFunctionPower() {
  check("power of a function",
        build({{"s", {0, 0, 15, 40}},
               {"i", {20, 0, 30, 40}},
               {"n", {35, 0, 55, 40}},
               {"2", {58, -10, 70, 10}},
               {"x", {75, 0, 100, 40}}}),
        "sin(x)^2");
}

void testLimit() {
  check("limit", build({{"l", {0, 20, 10, 100}},
                        {"i", {20, 20, 30, 100}},
                        {"m", {40, 40, 100, 100}},
                        {"x", {0, 120, 30, 160}},
                        {"arrow", {35, 125, 70, 140}},
                        {"0", {75, 115, 100, 155}},
                        {"s", {130, 30, 145, 70}},
                        {"i", {150, 30, 160, 70}},
                        {"n", {165, 30, 180, 70}},
                        {"(", {185, 30, 190, 70}},
                        {"x", {192, 30, 210, 70}},
                        {")", {212, 30, 217, 70}},
                        {"-", {130, 80, 220, 86}},
                        {"x", {160, 95, 180, 135}}}),
        "lim(sin(x)/x, x->0)");
}

void testSystem() {
  check("system", build({{"2", {0, 0, 20, 40}},
                         {"x", {24, 5, 44, 40}},
                         {"+", {48, 15, 68, 25}},
                         {"y", {72, 5, 92, 40}},
                         {"=", {96, 15, 116, 25}},
                         {"5", {120, 0, 140, 40}},
                         {",", {144, 30, 152, 40}},
                         {"x", {160, 5, 180, 40}},
                         {"-", {184, 15, 204, 25}},
                         {"y", {208, 5, 228, 40}},
                         {"=", {232, 15, 252, 25}},
                         {"1", {256, 0, 276, 40}}}),
        "2x+y=5,x-y=1");
}

void testPrecedence() {
  check("two times a fraction", build({{"2", {0, 40, 30, 80}},
                                       {"-", {36, 52, 96, 58}},
                                       {"x", {46, 20, 76, 48}},
                                       {"y", {46, 62, 76, 90}}}),
        "2*x/y");
  check("power of a fraction", build({{"x", {10, 20, 40, 48}},
                                      {"-", {0, 52, 60, 58}},
                                      {"y", {10, 62, 40, 90}},
                                      {"2", {66, 20, 88, 46}}}),
        "(x/y)^2");
}

}  // namespace

int main() {
  testRow();
  testPower();
  testFraction();
  testNestedFraction();
  testRadical();
  testSum();
  testIntegral();
  testFunctionPower();
  testLimit();
  testSystem();
  testPrecedence();

  if (failures > 0) {
    std::cout << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "all structure tests passed\n";
  return 0;
}
