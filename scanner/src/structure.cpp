#include "structure.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace infinit::scan {
namespace {

double median(std::vector<int> values) {
  if (values.empty()) return 0.0;
  // Lower median: a few tall containers must not inflate the page scale.
  const size_t mid = (values.size() - 1) / 2;
  std::nth_element(values.begin(), values.begin() + mid, values.end());
  return static_cast<double>(values[mid]);
}

bool contains(const Box& outer, const Box& inner) {
  return inner.x0 >= outer.x0 - 1 && inner.x1 <= outer.x1 + 1 &&
         inner.y0 >= outer.y0 - 1 && inner.y1 <= outer.y1 + 1;
}

struct Context {
  const Segmentation& segmentation;
  const std::vector<GlyphMatch>& matches;
  StructureOptions options;
  double scale = 1.0;

  Box box(int id) const { return segmentation.symbols[id].box; }
  const GlyphMatch& match(int id) const { return matches[id]; }
};

bool isBarLike(const Context& ctx, int id) {
  const Box box = ctx.box(id);
  return box.height() <= ctx.options.barAspect * box.width();
}

/// Collects the symbols that sit directly above (or below) a bar and belong to
/// the same visual cluster, so a nested fraction keeps its own bar.
std::vector<int> clusterAlong(const Context& ctx, const std::vector<int>& ids,
                              const Box& bar, bool upwards, double clusterGap) {
  std::vector<int> sorted = ids;
  std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
    return upwards ? ctx.box(a).y1 > ctx.box(b).y1 : ctx.box(a).y0 < ctx.box(b).y0;
  });

  std::vector<int> kept;
  int edge = upwards ? bar.y0 : bar.y1;
  for (int id : sorted) {
    const Box box = ctx.box(id);
    const int gap = upwards ? edge - box.y1 : box.y0 - edge;
    if (gap > clusterGap) break;
    kept.push_back(id);
    edge = upwards ? std::min(edge, box.y0) : std::max(edge, box.y1);
  }
  return kept;
}

/// A fraction bar needs ink directly above and directly below it, spanning it.
bool isFractionBar(const Context& ctx, int id, const std::vector<int>& ids,
                   std::vector<int>* numerator, std::vector<int>* denominator) {
  if (ctx.options.overrides.barFor(id) == BarRelation::Minus) return false;
  const Box bar = ctx.box(id);
  if (!isBarLike(ctx, id)) return false;
  // A dash is short; a fraction bar covers its numerator and denominator. The
  // relative test below is the real one, this only rejects tiny dashes early.
  if (bar.width() < 0.60 * ctx.scale) return false;

  const double tolerance = 0.05 * bar.width();
  std::vector<int> above;
  std::vector<int> below;
  for (int other : ids) {
    if (other == id) continue;
    const Box box = ctx.box(other);
    if (box.centerX() < bar.x0 - tolerance) continue;
    if (box.centerX() > bar.x1 + tolerance) continue;
    if (box.y1 <= bar.y0 + 2) {
      above.push_back(other);
    } else if (box.y0 >= bar.y1 - 2) {
      below.push_back(other);
    }
  }
  if (above.empty() || below.empty()) return false;

  const double clusterGap = 0.60 * ctx.scale;
  above = clusterAlong(ctx, above, bar, true, clusterGap);
  below = clusterAlong(ctx, below, bar, false, clusterGap);
  if (above.empty() || below.empty()) return false;

  // The nearest numerator and denominator must touch the bar closely; a stray
  // minus under a superscript does not.
  const double reach = ctx.options.barGap * ctx.scale;
  int aboveBottom = bar.y0;
  int belowTop = bar.y1;
  for (int other : above) aboveBottom = std::max(aboveBottom, ctx.box(other).y1);
  for (int other : below) belowTop = std::min(belowTop, ctx.box(other).y0);
  if (bar.y0 - aboveBottom > reach) return false;
  if (belowTop - bar.y1 > reach) return false;

  // The bar spans the wider of the two groups.
  int groupLeft = bar.x1;
  int groupRight = bar.x0;
  for (int other : above) {
    groupLeft = std::min(groupLeft, ctx.box(other).x0);
    groupRight = std::max(groupRight, ctx.box(other).x1);
  }
  for (int other : below) {
    groupLeft = std::min(groupLeft, ctx.box(other).x0);
    groupRight = std::max(groupRight, ctx.box(other).x1);
  }
  if (bar.width() < 0.95 * (groupRight - groupLeft)) return false;

  if (numerator != nullptr) *numerator = std::move(above);
  if (denominator != nullptr) *denominator = std::move(below);
  return true;
}

/// A superscript starts at or above the base's top edge, ends in the base's
/// upper half, and sits clearly higher than the baseline.
///
/// Size is deliberately not a hard gate. In serif maths the x-height of a
/// lowercase base (about 0.45 em) is *shorter* than a superscript digit
/// (about 0.49 em), so requiring `box.height() < base.height()` turned a clean
/// `x^2` into `x2`. What separates a script from a neighbour is where the box
/// sits, not how tall it is, so the height enters only as a loose ratio.
bool isRaisedAbove(const Context& ctx, const Box& base, const Box& box) {
  // Size is a ratio, not a hard gate: the x-height of a lowercase base is
  // shorter than a superscript digit in serif faces (0.45 em vs 0.49 em), so
  // `height < base.height()` turned a clean x^2 into x2. 1.15 keeps a normal
  // neighbour out (they are level, and the lift test rejects them) while
  // accepting a real script.
  if (box.height() > base.height() * 1.15) return false;
  const double height = base.height();
  if (box.y0 > base.y0 + ctx.options.superscriptTop * height) return false;
  if (box.y1 > base.y0 + ctx.options.superscriptBottom * height) return false;
  return base.y1 - box.y1 >= ctx.options.superscriptLift * height;
}

/// Later members of an exponent ("x+1" in 2^(x+1)) are level with the first
/// member rather than with the base.
bool continuesScript(const Context& ctx, const Box& base, const Box& group,
                     const Box& box) {
  if (box.y0 > base.y0 + ctx.options.superscriptTop * base.height()) {
    return false;
  }
  if (box.y1 > base.y0 + ctx.options.superscriptBottom * base.height()) {
    return false;
  }
  const int overlap = group.overlapY(box);
  if (overlap < 0.50 * std::min(group.height(), box.height())) return false;
  const double gap = static_cast<double>(box.x0 - group.x1);
  return gap >= -0.30 * group.width() && gap <= 1.00 * group.height();
}

struct Item {
  Node node;
  std::vector<int> ids;
};

std::vector<Item> parseRegion(const Context& ctx, std::vector<int> ids,
                              bool allowScripts);
Node rowOf(std::vector<Item> items);
Node makeAtom(const Context& ctx, int id);

bool isLetterLabel(const std::string& label) {
  return label.size() == 1 &&
         std::isalpha(static_cast<unsigned char>(label[0])) != 0;
}

bool labelIs(const Context& ctx, int id, const char* name) {
  return ctx.match(id).label == name;
}

/// The three letters of "lim" must sit on one baseline, in order, close
/// together. Italic math typesets them as separate glyphs, never as a word.
bool findLimitWord(const Context& ctx, const std::vector<int>& ids,
                   std::vector<int>* letters) {
  for (int id : ids) {
    if (labelIs(ctx, id, "lim")) {
      letters->push_back(id);
      return true;
    }
  }
  const double slack = 0.20 * ctx.scale;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (!labelIs(ctx, ids[i], "l")) continue;
    const Box a = ctx.box(ids[i]);
    // The subscript (x -> 0) interleaves with the letters in x order, so skip
    // anything that is not on the letters' baseline.
    size_t second = ids.size();
    for (size_t j = i + 1; j < ids.size(); ++j) {
      const Box box = ctx.box(ids[j]);
      if (std::abs(box.y1 - a.y1) > slack) continue;
      if (box.x0 - a.x1 > 0.6 * ctx.scale) break;
      if (!labelIs(ctx, ids[j], "i")) continue;
      second = j;
      break;
    }
    if (second == ids.size()) continue;
    const Box b = ctx.box(ids[second]);
    size_t third = ids.size();
    for (size_t j = second + 1; j < ids.size(); ++j) {
      const Box box = ctx.box(ids[j]);
      if (std::abs(box.y1 - a.y1) > slack) continue;
      if (box.x0 - b.x1 > 0.6 * ctx.scale) break;
      if (!labelIs(ctx, ids[j], "m")) continue;
      third = j;
      break;
    }
    if (third == ids.size()) continue;
    letters->push_back(ids[i]);
    letters->push_back(ids[second]);
    letters->push_back(ids[third]);
    return true;
  }
  return false;
}

/// Builds a limit node: the subscript under "lim" is split at the arrow into
/// the variable and the point, and everything to the right is the body.
bool parseLimit(const Context& ctx, const std::vector<int>& ids,
                std::vector<int>* consumed, Node* out) {
  std::vector<int> word;
  if (!findLimitWord(ctx, ids, &word)) return false;

  Box frame;
  for (int id : word) frame.include(ctx.box(id));
  const double width = frame.width();
  const double height = frame.height();

  std::vector<int> subscript;
  std::vector<int> body;
  for (int id : ids) {
    if (std::find(word.begin(), word.end(), id) != word.end()) continue;
    const Box box = ctx.box(id);
    const bool underWord =
        box.y0 >= frame.y1 - 0.15 * height &&
        box.centerX() >= frame.x0 - 0.6 * width &&
        box.centerX() <= frame.x1 + 0.6 * width;
    if (underWord) {
      subscript.push_back(id);
    } else if (box.x0 >= frame.x1 - 0.05 * ctx.scale) {
      body.push_back(id);
    }
  }

  Node variable;
  Node point;
  int arrow = -1;
  for (int id : subscript) {
    if (labelIs(ctx, id, "arrow") || labelIs(ctx, id, "-")) arrow = id;
  }
  if (arrow >= 0) {
    std::vector<int> lhs;
    std::vector<int> rhs;
    for (int id : subscript) {
      if (id == arrow) continue;
      if (ctx.box(id).centerX() < ctx.box(arrow).centerX()) {
        lhs.push_back(id);
      } else {
        rhs.push_back(id);
      }
    }
    variable = rowOf(parseRegion(ctx, lhs, false));
    point = rowOf(parseRegion(ctx, rhs, false));
  } else {
    variable = rowOf(parseRegion(ctx, subscript, false));
  }

  Node node;
  node.kind = NodeKind::Limit;
  node.text = "lim";
  node.box = frame;
  node.confidence = ctx.match(word.front()).score;
  Node bodyNode = rowOf(parseRegion(ctx, body, true));
  if (!bodyNode.box.empty()) node.box.include(bodyNode.box);
  if (!variable.box.empty()) node.box.include(variable.box);
  if (!point.box.empty()) node.box.include(point.box);
  node.confidence = std::min(node.confidence, bodyNode.confidence);
  node.children.push_back(std::move(bodyNode));
  node.children.push_back(std::move(variable));
  node.children.push_back(std::move(point));

  consumed->push_back(word.front());
  consumed->insert(consumed->end(), word.begin(), word.end());
  consumed->insert(consumed->end(), subscript.begin(), subscript.end());
  consumed->insert(consumed->end(), body.begin(), body.end());
  *out = std::move(node);
  return true;
}

/// Splits the summation index `n=1` into the index and the start value.
void splitIndex(const Context& ctx, const std::vector<int>& lower,
                std::vector<int>* index, std::vector<int>* start) {
  int equals = -1;
  for (int id : lower) {
    if (labelIs(ctx, id, "=")) {
      equals = id;
      break;
    }
  }
  if (equals < 0) {
    *index = lower;
    return;
  }
  for (int id : lower) {
    if (id == equals) continue;
    if (ctx.box(id).centerX() < ctx.box(equals).centerX()) {
      index->push_back(id);
    } else {
      start->push_back(id);
    }
  }
}

/// Builds a sum or an integral: the operator owns its bounds and the body to
/// its right. The body of an integral loses the trailing `dx`.
bool parseBigOperator(const Context& ctx, const std::vector<int>& ids,
                      std::vector<int>* consumed, Node* out) {
  int op = -1;
  double best = -1.0;
  for (int id : ids) {
    const bool isSum = labelIs(ctx, id, "sum");
    const bool isIntegral = labelIs(ctx, id, "integral");
    if (!isSum && !isIntegral) continue;
    const double area = static_cast<double>(ctx.box(id).area());
    if (area > best) {
      best = area;
      op = id;
    }
  }
  if (op < 0) return false;

  const bool isIntegral = labelIs(ctx, op, "integral");
  const Box opBox = ctx.box(op);
  std::vector<int> upper;
  std::vector<int> lower;
  std::vector<int> body;

  for (int id : ids) {
    if (id == op) continue;
    const Box box = ctx.box(id);
    if (isIntegral) {
      // Integral bounds hug the right edge of the sign: one above its middle,
      // one below. A narrow side-set integral can end before the bound starts,
      // so allow the column from the sign's right half onwards.
      const bool inColumn =
          box.x0 >= opBox.x0 + 0.35 * opBox.width() &&
          box.x0 <= opBox.x1 + 0.90 * ctx.scale;
      // Compare centres, not edges: a bound often overlaps the sign's
      // vertical extent by a pixel or two even when it is clearly above or
      // below the middle.
      const double centerY = box.centerY();
      if (inColumn && centerY <= opBox.y0 + 0.35 * opBox.height()) {
        upper.push_back(id);
        continue;
      }
      if (inColumn && centerY >= opBox.y0 + 0.65 * opBox.height()) {
        lower.push_back(id);
        continue;
      }
    } else {
      // Summation bounds are centred on the sign. The whole bound row
      // ("n = 1") is wider than the sign, so the column is generous.
      const double margin = 0.40 * opBox.width();
      const bool inColumn = box.centerX() >= opBox.x0 - margin &&
                            box.centerX() <= opBox.x1 + margin;
      if (inColumn && box.y1 <= opBox.y0 + 0.10 * opBox.height()) {
        upper.push_back(id);
        continue;
      }
      if (inColumn && box.y0 >= opBox.y1 - 0.10 * opBox.height()) {
        lower.push_back(id);
        continue;
      }
    }
    if (box.x0 >= opBox.x1 - 0.05 * ctx.scale) {
      body.push_back(id);
    }
  }

  std::sort(body.begin(), body.end(),
            [&ctx](int a, int b) { return ctx.box(a).x0 < ctx.box(b).x0; });

  int variable = -1;
  int differential = -1;
  if (isIntegral && body.size() >= 2) {
    const int last = body[body.size() - 1];
    const int previous = body[body.size() - 2];
    const Box d = ctx.box(previous);
    const Box v = ctx.box(last);
    const bool adjacent = v.x0 - d.x1 <= 0.40 * ctx.scale;
    const bool aligned = std::abs(v.y1 - d.y1) <= 0.30 * ctx.scale;
    if (labelIs(ctx, previous, "d") && isLetterLabel(ctx.match(last).label) &&
        adjacent && aligned) {
      variable = last;
      differential = previous;
      body.pop_back();
      body.pop_back();
    }
  }

  Node node;
  node.kind = isIntegral ? NodeKind::Integral : NodeKind::Sum;
  node.text = isIntegral ? "integral" : "sum";
  node.box = opBox;
  node.confidence = ctx.match(op).score;

  Node bodyNode = rowOf(parseRegion(ctx, body, true));
  node.confidence = std::min(node.confidence, bodyNode.confidence);
  if (!bodyNode.box.empty()) node.box.include(bodyNode.box);
  node.children.push_back(std::move(bodyNode));

  if (isIntegral) {
    Node varNode;
    if (variable >= 0) {
      varNode = makeAtom(ctx, variable);
      node.confidence = std::min(node.confidence, varNode.confidence);
    }
    Node lowerNode = rowOf(parseRegion(ctx, lower, true));
    Node upperNode = rowOf(parseRegion(ctx, upper, true));
    node.children.push_back(std::move(varNode));
    node.children.push_back(std::move(lowerNode));
    node.children.push_back(std::move(upperNode));
  } else {
    std::vector<int> index;
    std::vector<int> start;
    splitIndex(ctx, lower, &index, &start);
    Node indexNode = rowOf(parseRegion(ctx, index, false));
    Node startNode = rowOf(parseRegion(ctx, start, false));
    Node upperNode = rowOf(parseRegion(ctx, upper, false));
    node.children.push_back(std::move(indexNode));
    node.children.push_back(std::move(startNode));
    node.children.push_back(std::move(upperNode));
  }

  consumed->push_back(op);
  consumed->insert(consumed->end(), upper.begin(), upper.end());
  consumed->insert(consumed->end(), lower.begin(), lower.end());
  consumed->insert(consumed->end(), body.begin(), body.end());
  if (variable >= 0) consumed->push_back(variable);
  if (differential >= 0) consumed->push_back(differential);
  *out = std::move(node);
  return true;
}

Node makeAtom(const Context& ctx, int id) {
  Node node;
  node.kind = NodeKind::Atom;
  node.text = ctx.match(id).label.empty() ? "?" : ctx.match(id).label;
  node.box = ctx.box(id);
  node.confidence = ctx.match(id).score;
  return node;
}

Node makeRow(std::vector<Node> items) {
  if (items.size() == 1) return std::move(items.front());
  Node row;
  row.kind = NodeKind::Row;
  row.confidence = 1.0;
  for (const Node& item : items) {
    row.confidence = std::min(row.confidence, item.confidence);
    if (!item.box.empty()) row.box.include(item.box);
  }
  row.children = std::move(items);
  return row;
}

Node rowOf(std::vector<Item> items) {
  std::vector<Node> nodes;
  nodes.reserve(items.size());
  for (Item& item : items) nodes.push_back(std::move(item.node));
  return makeRow(std::move(nodes));
}

/// Joins adjacent digits (and a decimal point between them) into one number
/// atom. A gap wider than a third of the page scale starts a new number.
void mergeNumberWords(const Context& ctx, std::vector<Item>* items) {
  const auto isDigit = [](const Item& item) {
    return item.node.isAtom() && item.node.text.size() == 1 &&
           std::isdigit(static_cast<unsigned char>(item.node.text[0])) != 0;
  };
  const auto isPoint = [](const Item& item) {
    return item.node.isAtom() && item.node.text == ".";
  };

  std::vector<Item> out;
  out.reserve(items->size());
  size_t i = 0;
  while (i < items->size()) {
    if (!isDigit((*items)[i])) {
      out.push_back(std::move((*items)[i]));
      ++i;
      continue;
    }

    Item merged = std::move((*items)[i]);
    size_t j = i + 1;
    while (j < items->size()) {
      Item& next = (*items)[j];
      if (!next.node.isAtom()) break;
      const double gap = static_cast<double>(next.node.box.x0 - merged.node.box.x1);
      if (isDigit(next)) {
        if (gap > 0.30 * ctx.scale) break;
      } else if (isPoint(next)) {
        // A decimal point needs a digit right after it and must sit on the
        // number's baseline; a centred multiplication dot does not.
        if (j + 1 >= items->size() || !isDigit((*items)[j + 1])) break;
        if (std::abs(next.node.box.centerY() - merged.node.box.centerY()) >
            0.25 * ctx.scale) {
          break;
        }
        const double afterGap =
            static_cast<double>((*items)[j + 1].node.box.x0 - next.node.box.x1);
        if (afterGap > 0.30 * ctx.scale) break;
      } else {
        break;
      }
      merged.node.text += next.node.text;
      merged.node.box.include(next.node.box);
      merged.node.confidence =
          std::min(merged.node.confidence, next.node.confidence);
      merged.ids.insert(merged.ids.end(), next.ids.begin(), next.ids.end());
      ++j;
    }
    out.push_back(std::move(merged));
    i = j;
  }
  *items = std::move(out);
}

/// Joins the letters of a function printed without a single glyph (`\sin` is
/// three separate glyphs) into one atom, so the renderer can add parentheses.
void mergeFunctionWords(const Context& ctx, std::vector<Item>* items) {
  static const char* kNames[] = {
      "arcsin", "arccos", "arctan", "sinh", "cosh", "tanh", "sin",
      "cos",    "tan",    "cot",    "sec",  "csc",  "ln",   "log",
      "exp",    "det",    "rank",   "dim",  "min",  "max",  "gcd",
      "tr"};
  std::vector<Item> out;
  out.reserve(items->size());

  size_t i = 0;
  while (i < items->size()) {
    const char* best = nullptr;
    size_t bestLength = 1;
    for (const char* name : kNames) {
      const size_t length = std::strlen(name);
      if (length <= bestLength || i + length > items->size()) continue;
      const Box first = (*items)[i].node.box;
      bool ok = true;
      for (size_t k = 0; k < length; ++k) {
        const Item& item = (*items)[i + k];
        if (!item.node.isAtom() || item.node.text.size() != 1 ||
            item.node.text[0] != name[k]) {
          ok = false;
          break;
        }
        if (std::abs(item.node.box.y1 - first.y1) > 0.20 * ctx.scale) {
          ok = false;
          break;
        }
        if (k > 0) {
          const Box previous = (*items)[i + k - 1].node.box;
          if (item.node.box.x0 - previous.x1 > 0.45 * ctx.scale) {
            ok = false;
            break;
          }
        }
      }
      if (ok) {
        best = name;
        bestLength = length;
      }
    }

    if (best == nullptr) {
      out.push_back(std::move((*items)[i]));
      ++i;
      continue;
    }

    Item merged;
    merged.node.kind = NodeKind::Atom;
    merged.node.text = best;
    merged.node.confidence = 1.0;
    for (size_t k = 0; k < bestLength; ++k) {
      Item& part = (*items)[i + k];
      merged.node.box.include(part.node.box);
      merged.node.confidence =
          std::min(merged.node.confidence, part.node.confidence);
      merged.ids.insert(merged.ids.end(), part.ids.begin(), part.ids.end());
    }
    out.push_back(std::move(merged));
    i += bestLength;
  }
  *items = std::move(out);
}

/// Attaches raised glyphs to the item on their left as exponents.
void attachScripts(const Context& ctx, std::vector<Item>* items) {
  std::vector<Item> out;
  out.reserve(items->size());

  size_t i = 0;
  while (i < items->size()) {
    Item base = std::move((*items)[i]);
    std::vector<int> scriptIds;
    Box group = base.node.box;

    size_t j = i + 1;
    while (j < items->size()) {
      const Box box = (*items)[j].node.box;
      const bool first = scriptIds.empty();
      const int firstId =
          (*items)[j].ids.empty() ? -1 : (*items)[j].ids.front();
      const ScriptRelation choice =
          first && firstId >= 0 ? ctx.options.overrides.scriptFor(firstId)
                                : ScriptRelation::Auto;
      if (first && choice == ScriptRelation::Inline) break;
      bool raised =
          first ? isRaisedAbove(ctx, base.node.box, box)
                : continuesScript(ctx, base.node.box, group, box);
      if (first && choice == ScriptRelation::Superscript) {
        // The user said this glyph belongs to the base; accept it unless it is
        // clearly a separate token far to the right.
        const double gap = static_cast<double>(box.x0 - base.node.box.x1);
        raised = gap <= 0.75 * base.node.box.width();
      }
      if (!raised) break;
      if (first) {
        const double gap = static_cast<double>(box.x0 - base.node.box.x1);
        if (gap > ctx.options.superscriptGap * base.node.box.width()) break;
      }
      group.include(box);
      scriptIds.insert(scriptIds.end(), (*items)[j].ids.begin(),
                       (*items)[j].ids.end());
      ++j;
    }

    if (scriptIds.empty()) {
      out.push_back(std::move(base));
      ++i;
      continue;
    }

    // A prime after a letter is a derivative mark, not an exponent. The
    // geometric pass sees a small mark above the baseline and calls it a
    // superscript, which renders f'(x) as f^'(x); the core parser only accepts
    // the first form.
    bool allPrimes = base.node.isAtom();
    for (int id : scriptIds) {
      if (ctx.match(id).label != "'") {
        allPrimes = false;
        break;
      }
    }
    if (allPrimes) {
      for (size_t k = 0; k < scriptIds.size(); ++k) base.node.text += "'";
      base.node.box.include((*items)[j - 1].node.box);
      base.node.confidence =
          std::min(base.node.confidence, ctx.match(scriptIds.front()).score);
      for (int id : scriptIds) base.ids.push_back(id);
      out.push_back(std::move(base));
      i = j;
      continue;
    }

    Node power;
    power.kind = NodeKind::Power;
    power.box = group;
    Node exponent = rowOf(parseRegion(ctx, scriptIds, false));
    power.confidence = std::min(base.node.confidence, exponent.confidence);
    power.children.push_back(std::move(base.node));
    power.children.push_back(std::move(exponent));
    out.push_back(Item{std::move(power), std::move(scriptIds)});
    i = j;
  }
  *items = std::move(out);
}

/// A symbol that encloses other symbols is a radical: the radical sign and its
/// overbar are one connected blob.
int findContainer(const Context& ctx, const std::vector<int>& ids) {
  int best = -1;
  double bestArea = 0.0;
  for (int id : ids) {
    const Box box = ctx.box(id);
    if (box.height() < 1.2 * ctx.scale) continue;
    int inside = 0;
    for (int other : ids) {
      if (other == id) continue;
      if (contains(box, ctx.box(other))) ++inside;
    }
    if (inside == 0) continue;
    const double area = static_cast<double>(box.width()) * box.height();
    if (area > bestArea) {
      bestArea = area;
      best = id;
    }
  }
  return best;
}

bool isOpenBracket(const std::string& label) {
  return label == "[" || label == "(" || label == "{";
}

bool isCloseBracket(const std::string& label) {
  return label == "]" || label == ")" || label == "}";
}

bool bracketsMatch(const std::string& open, const std::string& close) {
  return (open == "[" && close == "]") || (open == "(" && close == ")") ||
         (open == "{" && close == "}");
}

/// Clusters values (centres) into groups that are closer than `tolerance`.
/// Returns group indices in input order and the group centres.
struct Clusters {
  std::vector<int> group;
  std::vector<double> centre;
  std::vector<int> size;
};

Clusters clusterValues(const std::vector<double>& values, double tolerance) {
  Clusters out;
  out.group.assign(values.size(), -1);
  std::vector<size_t> order(values.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(),
            [&](size_t a, size_t b) { return values[a] < values[b]; });

  double last = 0.0;
  for (size_t k = 0; k < order.size(); ++k) {
    const size_t index = order[k];
    if (out.centre.empty() || values[index] - last > tolerance) {
      out.centre.push_back(values[index]);
      out.size.push_back(0);
    }
    const int group = static_cast<int>(out.centre.size()) - 1;
    out.group[index] = group;
    last = values[index];
    ++out.size[group];
  }
  return out;
}

/// A matrix reads as a tall bracketed grid: at least two rows and two columns
/// of cells inside one pair of brackets. The bracket itself is just a glyph;
/// what makes it a matrix is the geometry of its contents.
bool parseMatrix(const Context& ctx, const std::vector<int>& ids,
                 std::vector<int>* consumed, Node* out) {
  int bestOpen = -1;
  int bestClose = -1;
  std::vector<int> bestInner;
  double bestArea = 0.0;

  for (int open : ids) {
    if (!isOpenBracket(ctx.match(open).label)) continue;
    const Box outer = ctx.box(open);
    if (outer.height() < 1.4 * ctx.scale ||
        outer.height() < 1.5 * outer.width()) {
      continue;
    }
    for (int close : ids) {
      if (!bracketsMatch(ctx.match(open).label, ctx.match(close).label)) {
        continue;
      }
      const Box right = ctx.box(close);
      if (right.x0 <= outer.x1) continue;
      const int vertical = outer.overlapY(right);
      if (vertical < 0.60 * std::min(outer.height(), right.height())) continue;
      if (right.height() < 1.4 * ctx.scale) continue;

      std::vector<int> inner;
      for (int id : ids) {
        if (id == open || id == close) continue;
        const Box box = ctx.box(id);
        if (box.centerX() <= outer.x0 || box.centerX() >= right.x1) continue;
        if (box.centerY() <= std::min(outer.y0, right.y0)) continue;
        if (box.centerY() >= std::max(outer.y1, right.y1)) continue;
        inner.push_back(id);
      }
      if (inner.size() < 4) continue;

      // Rows by vertical centre, columns by horizontal centre. Both use the
      // page scale, never a pixel constant.
      std::vector<double> ys;
      std::vector<double> xs;
      ys.reserve(inner.size());
      xs.reserve(inner.size());
      for (int id : inner) {
        ys.push_back(ctx.box(id).centerY());
        xs.push_back(ctx.box(id).centerX());
      }
      const Clusters rows = clusterValues(ys, 0.65 * ctx.scale);
      const Clusters columns = clusterValues(xs, 0.75 * ctx.scale);
      if (rows.centre.size() < 2 || columns.centre.size() < 2) continue;
      // A brace spanning several equations is a system, not a matrix.
      if (ctx.match(open).label == "{") continue;
      // Ragged rows are allowed (a cell may be missing), but a matrix where
      // one row holds all the ink is a parenthesised expression instead.
      size_t filledRows = 0;
      for (size_t r = 0; r < rows.centre.size(); ++r) {
        for (size_t i = 0; i < inner.size(); ++i) {
          if (rows.group[i] == static_cast<int>(r)) {
            ++filledRows;
            break;
          }
        }
      }
      if (filledRows < 2) continue;

      const double area =
          static_cast<double>(right.x1 - outer.x0) * outer.height();
      if (area > bestArea) {
        bestArea = area;
        bestOpen = open;
        bestClose = close;
        bestInner = std::move(inner);
      }
    }
  }

  if (bestOpen < 0) return false;

  std::vector<double> ys;
  std::vector<double> xs;
  ys.reserve(bestInner.size());
  xs.reserve(bestInner.size());
  for (int id : bestInner) {
    ys.push_back(ctx.box(id).centerY());
    xs.push_back(ctx.box(id).centerX());
  }
  const Clusters rows = clusterValues(ys, 0.65 * ctx.scale);
  const Clusters columns = clusterValues(xs, 0.75 * ctx.scale);

  Node matrix;
  matrix.kind = NodeKind::Matrix;
  matrix.text = "matrix";
  matrix.confidence = 1.0;
  const Box openBox = ctx.box(bestOpen);
  const Box closeBox = ctx.box(bestClose);
  matrix.box = openBox;
  matrix.box.include(closeBox);
  matrix.confidence =
      std::min(ctx.match(bestOpen).score, ctx.match(bestClose).score);

  for (size_t r = 0; r < rows.centre.size(); ++r) {
    Node row;
    row.kind = NodeKind::Row;
    row.confidence = 1.0;
    for (size_t c = 0; c < columns.centre.size(); ++c) {
      std::vector<int> cell;
      for (size_t i = 0; i < bestInner.size(); ++i) {
        if (rows.group[i] != static_cast<int>(r)) continue;
        if (columns.group[i] != static_cast<int>(c)) continue;
        cell.push_back(bestInner[i]);
      }
      Node cellNode;
      if (cell.empty()) {
        // A hole in the grid is reported, not invented: keep an empty cell so
        // the row/column shape survives, and lower the confidence.
        cellNode.kind = NodeKind::Row;
        cellNode.confidence = 0.30;
      } else {
        cellNode = rowOf(parseRegion(ctx, cell, true));
        matrix.box.include(cellNode.box);
        matrix.confidence = std::min(matrix.confidence, cellNode.confidence);
      }
      row.confidence = std::min(row.confidence, cellNode.confidence);
      row.children.push_back(std::move(cellNode));
    }
    matrix.children.push_back(std::move(row));
  }

  consumed->push_back(bestOpen);
  consumed->push_back(bestClose);
  for (int id : bestInner) consumed->push_back(id);
  *out = std::move(matrix);
  return true;
}

std::vector<Item> parseRegion(const Context& ctx, std::vector<int> ids,
                              bool allowScripts) {
  if (ids.empty()) return {};
  std::sort(ids.begin(), ids.end(),
            [&ctx](int a, int b) { return ctx.box(a).x0 < ctx.box(b).x0; });

  std::vector<Item> items;
  std::vector<int> consumed;
  Node child;
  bool structured = false;

  // Big operators own the glyphs around them, so they are recognised before a
  // fraction bar inside their body steals the numerator and denominator.
  if (parseMatrix(ctx, ids, &consumed, &child)) {
    structured = true;
  } else if (parseLimit(ctx, ids, &consumed, &child)) {
    structured = true;
  } else if (parseBigOperator(ctx, ids, &consumed, &child)) {
    structured = true;
  } else {
    int bar = -1;
    double barWidth = 0.0;
    std::vector<int> numerator;
    std::vector<int> denominator;
    for (int id : ids) {
      std::vector<int> above;
      std::vector<int> below;
      if (!isFractionBar(ctx, id, ids, &above, &below)) continue;
      const double width = ctx.box(id).width();
      if (width > barWidth) {
        barWidth = width;
        bar = id;
        numerator = std::move(above);
        denominator = std::move(below);
      }
    }

    const int container = bar < 0 ? findContainer(ctx, ids) : -1;

    if (bar >= 0) {
      consumed.push_back(bar);
      consumed.insert(consumed.end(), numerator.begin(), numerator.end());
      consumed.insert(consumed.end(), denominator.begin(), denominator.end());

      Node frac;
      frac.kind = NodeKind::Fraction;
      frac.children.push_back(rowOf(parseRegion(ctx, numerator, true)));
      frac.children.push_back(rowOf(parseRegion(ctx, denominator, true)));
      frac.confidence =
          std::min(frac.children[0].confidence, frac.children[1].confidence);
      Box frame = ctx.box(bar);
      for (const Node& part : frac.children) {
        if (!part.box.empty()) frame.include(part.box);
      }
      frac.box = frame;
      child = std::move(frac);
      structured = true;
    } else if (container >= 0) {
      consumed.push_back(container);
      const Box box = ctx.box(container);
      std::vector<int> inner;
      // The radicand lives under the overbar, not strictly inside the sign's
      // bounding box: a tall glyph can stick out a few pixels below it. Accept
      // anything whose centre is horizontally under the bar and whose top is
      // in the bar's vertical band.
      for (int id : ids) {
        if (id == container) continue;
        const Box part = ctx.box(id);
        const bool horizontallyInside =
            part.centerX() > box.x0 && part.centerX() < box.x1;
        const bool underBar = part.y0 >= box.y0 - 0.10 * ctx.scale &&
                              part.y0 <= box.y1 + 0.35 * ctx.scale;
        if (horizontallyInside && underBar) inner.push_back(id);
      }
      consumed.insert(consumed.end(), inner.begin(), inner.end());

      Node radical;
      radical.kind = NodeKind::Radical;
      radical.box = box;
      Node body = rowOf(parseRegion(ctx, inner, true));
      radical.confidence = std::min(ctx.match(container).score, body.confidence);
      radical.children.push_back(std::move(body));
      child = std::move(radical);
      structured = true;
    }
  }

  if (structured) {
    const Box frame = child.box;
    std::vector<int> left;
    std::vector<int> right;
    for (int id : ids) {
      if (std::find(consumed.begin(), consumed.end(), id) != consumed.end()) {
        continue;
      }
      if (ctx.box(id).centerX() < frame.centerX()) {
        left.push_back(id);
      } else {
        right.push_back(id);
      }
    }

    std::vector<Item> leftItems = parseRegion(ctx, left, true);
    for (Item& item : leftItems) items.push_back(std::move(item));

    std::vector<int> childIds = consumed;
    items.push_back(Item{std::move(child), std::move(childIds)});

    std::vector<Item> rightItems = parseRegion(ctx, right, true);
    for (Item& item : rightItems) items.push_back(std::move(item));
  } else {
    for (int id : ids) items.push_back(Item{makeAtom(ctx, id), {id}});
  }

  mergeNumberWords(ctx, &items);
  mergeFunctionWords(ctx, &items);
  if (allowScripts) attachScripts(ctx, &items);
  return items;
}

std::string renderBare(const Node& node);

bool isBlank(const Node& node) {
  return node.kind == NodeKind::Row && node.children.empty();
}

bool isOperatorChar(char c) {
  return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || c == '(' ||
         c == '[' || c == '{' || c == ',' || c == '^' || c == '<' || c == '>';
}

/// Function names printed without parentheses (`sin x`, `ln x`) must gain them,
/// otherwise the parser reads `sinx` as one symbol.
bool isFunctionName(const std::string& text) {
  return text == "sin" || text == "cos" || text == "tan" || text == "cot" ||
         text == "sec" || text == "csc" || text == "sinh" || text == "cosh" ||
         text == "tanh" || text == "ln" || text == "log" || text == "exp" ||
         text == "arcsin" || text == "arccos" || text == "arctan" ||
         text == "det" || text == "tr" || text == "rank" || text == "dim" ||
         text == "min" || text == "max" || text == "gcd";
}

std::string renderOperand(const Node& node) {
  if (node.isAtom()) return node.text;
  if (node.kind == NodeKind::Row && node.children.size() == 1) {
    return renderOperand(node.children.front());
  }
  // `sin x` already renders as a parenthesised call; a second pair of brackets
  // around the whole numerator only adds noise.
  if (node.kind == NodeKind::Row && node.children.size() > 1 &&
      node.children.front().isAtom() &&
      isFunctionName(node.children.front().text)) {
    return renderBare(node);
  }
  return "(" + renderBare(node) + ")";
}

std::string renderBare(const Node& node) {
  switch (node.kind) {
    case NodeKind::Atom:
      return node.text;
    case NodeKind::Matrix: {
      // The app's matrix fields accept "1, 2; 3, 4"; the bracket form keeps
      // the row/column shape readable when the reading is copied as a whole.
      std::string out = "[";
      for (size_t r = 0; r < node.children.size(); ++r) {
        if (r) out += ",";
        out += "[";
        const Node& row = node.children[r];
        for (size_t c = 0; c < row.children.size(); ++c) {
          if (c) out += ",";
          out += isBlank(row.children[c]) ? "?"
                                          : renderBare(row.children[c]);
        }
        out += "]";
      }
      return out + "]";
    }
    case NodeKind::Fraction:
      return renderOperand(node.children[0]) + "/" +
             renderOperand(node.children[1]);
    case NodeKind::Power: {
      const Node& base = node.children[0];
      const Node& exponent = node.children[1];
      const std::string baseText = base.isAtom() ? base.text : renderOperand(base);
      const std::string exponentText =
          exponent.isAtom() ? exponent.text : "(" + renderBare(exponent) + ")";
      return baseText + "^" + exponentText;
    }
    case NodeKind::Radical:
      return "sqrt(" + renderBare(node.children[0]) + ")";
    case NodeKind::Sum: {
      const Node& body = node.children[0];
      const Node& index = node.children[1];
      const Node& start = node.children[2];
      const Node& upper = node.children[3];
      std::string out = "sum(" + renderBare(body);
      if (!isBlank(index)) {
        out += ", " + renderBare(index);
        if (!isBlank(start)) out += "=" + renderBare(start);
      }
      if (!isBlank(upper)) out += ", " + renderBare(upper);
      return out + ")";
    }
    case NodeKind::Integral: {
      const Node& body = node.children[0];
      const Node& variable = node.children[1];
      const Node& lower = node.children[2];
      const Node& upper = node.children[3];
      std::string out = "integral(" + renderBare(body);
      if (!isBlank(variable)) out += ", " + renderBare(variable);
      if (!isBlank(lower)) out += ", " + renderBare(lower);
      if (!isBlank(upper)) out += ", " + renderBare(upper);
      return out + ")";
    }
    case NodeKind::Limit: {
      const Node& body = node.children[0];
      const Node& variable = node.children[1];
      const Node& point = node.children[2];
      std::string out = "lim(" + renderBare(body);
      if (!isBlank(variable)) out += ", " + renderBare(variable);
      if (!isBlank(point)) out += "->" + renderBare(point);
      return out + ")";
    }
    case NodeKind::Row:
      break;
  }

  std::string out;
  for (size_t i = 0; i < node.children.size(); ++i) {
    const Node& child = node.children[i];
    std::string piece = renderBare(child);
    const bool compound =
        child.kind == NodeKind::Fraction || child.kind == NodeKind::Radical;
    // Juxtaposition means multiplication, but "x1/3" would read as one name.
    if (compound && !out.empty() && !isOperatorChar(out.back())) {
      piece = "*" + piece;
    }
    // `sin x` is written without parentheses; the parser needs `sin(x)`. The
    // same applies to `sin^2 x`, where the exponent must follow the call.
    const Node* callName = nullptr;
    if (child.isAtom() && isFunctionName(child.text)) {
      callName = &child;
    } else if (child.kind == NodeKind::Power && child.children.size() == 2 &&
               child.children[0].isAtom() &&
               isFunctionName(child.children[0].text)) {
      callName = &child.children[0];
    }
    if (callName != nullptr && i + 1 < node.children.size()) {
      const Node& next = node.children[i + 1];
      const bool alreadyCalled = next.isAtom() && next.text == "(";
      if (!alreadyCalled) {
        std::string args;
        size_t end = i + 1;
        for (; end < node.children.size(); ++end) {
          const Node& arg = node.children[end];
          const bool boundary =
              arg.isAtom() &&
              (arg.text.empty() || isOperatorChar(arg.text[0]));
          if (boundary) break;
          if (arg.kind != NodeKind::Atom && arg.kind != NodeKind::Power) break;
          args += renderBare(arg);
        }
        if (!args.empty()) {
          piece = callName->text + "(" + args + ")";
          if (child.kind == NodeKind::Power) {
            const Node& exponent = child.children[1];
            const std::string exponentText =
                exponent.isAtom()
                    ? exponent.text
                    : "(" + renderBare(exponent) + ")";
            piece += "^" + exponentText;
          }
          i = end - 1;
        }
      }
    }
    out += piece;
  }
  return out;
}

}  // namespace

Node buildStructure(const Segmentation& segmentation,
                    const std::vector<GlyphMatch>& matches,
                    const StructureOptions& options) {
  Context ctx{segmentation, matches, options, 1.0};

  std::vector<int> heights;
  heights.reserve(segmentation.symbols.size());
  for (const Symbol& symbol : segmentation.symbols) {
    if (symbol.box.height() > 0) heights.push_back(symbol.box.height());
  }
  ctx.scale = std::max(1.0, median(std::move(heights)));

  std::vector<int> all(segmentation.symbols.size());
  for (size_t i = 0; i < all.size(); ++i) all[i] = static_cast<int>(i);
  return rowOf(parseRegion(ctx, std::move(all), true));
}

std::string renderStructure(const Node& node) { return renderBare(node); }

bool containsMatrix(const Node& node) {
  if (node.kind == NodeKind::Matrix) return true;
  for (const Node& child : node.children) {
    if (containsMatrix(child)) return true;
  }
  return false;
}

const Node* firstMatrix(const Node& node) {
  if (node.kind == NodeKind::Matrix) return &node;
  for (const Node& child : node.children) {
    if (const Node* found = firstMatrix(child)) return found;
  }
  return nullptr;
}

namespace {

std::string latexAtom(const std::string& label) {
  if (label == "pi") return "\\pi";
  if (label == "inf") return "\\infty";
  if (label == "*") return "\\cdot";
  if (label == "arrow") return "\\to";
  if (label == "sum") return "\\sum";
  if (label == "integral") return "\\int";
  if (label == "<") return "<";
  if (label == ">") return ">";
  if (label == "sin" || label == "cos" || label == "tan" || label == "cot" ||
      label == "sec" || label == "csc" || label == "sinh" ||
      label == "cosh" || label == "tanh" || label == "ln" ||
      label == "log" || label == "exp" || label == "arcsin" ||
      label == "arccos" || label == "arctan" || label == "det" ||
      label == "dim" || label == "min" || label == "max" ||
      label == "gcd") {
    return "\\" + label;
  }
  if (label == "rank" || label == "tr") {
    return "\\operatorname{" + label + "}";
  }
  return label;
}

std::string latexNode(const Node& node);

std::string latexOperand(const Node& node) {
  if (node.isAtom()) return latexAtom(node.text);
  if (node.kind == NodeKind::Row && node.children.size() == 1) {
    return latexOperand(node.children.front());
  }
  return "\\left(" + latexNode(node) + "\\right)";
}

std::string latexNode(const Node& node) {
  switch (node.kind) {
    case NodeKind::Atom:
      return latexAtom(node.text);
    case NodeKind::Fraction:
      return "\\frac{" + latexNode(node.children[0]) + "}{" +
             latexNode(node.children[1]) + "}";
    case NodeKind::Power: {
      const Node& exponent = node.children[1];
      const std::string exp = exponent.isAtom() ? latexAtom(exponent.text)
                                                : "{" + latexNode(exponent) + "}";
      return latexOperand(node.children[0]) + "^{" + exp + "}";
    }
    case NodeKind::Radical:
      return "\\sqrt{" + latexNode(node.children[0]) + "}";
    case NodeKind::Sum: {
      const Node& index = node.children[1];
      const Node& start = node.children[2];
      const Node& upper = node.children[3];
      std::string out = "\\sum";
      if (!isBlank(index)) {
        out += "_{" + latexNode(index);
        if (!isBlank(start)) out += "=" + latexNode(start);
        out += "}";
      }
      if (!isBlank(upper)) out += "^{" + latexNode(upper) + "}";
      return out + " " + latexNode(node.children[0]);
    }
    case NodeKind::Integral: {
      const Node& variable = node.children[1];
      const Node& lower = node.children[2];
      const Node& upper = node.children[3];
      std::string out = "\\int";
      if (!isBlank(lower)) out += "_{" + latexNode(lower) + "}";
      if (!isBlank(upper)) out += "^{" + latexNode(upper) + "}";
      out += " " + latexNode(node.children[0]);
      if (!isBlank(variable)) out += "\\,d" + latexNode(variable);
      return out;
    }
    case NodeKind::Limit: {
      const Node& variable = node.children[1];
      const Node& point = node.children[2];
      std::string out = "\\lim";
      if (!isBlank(variable) || !isBlank(point)) {
        out += "_{";
        if (!isBlank(variable)) out += latexNode(variable);
        if (!isBlank(point)) out += "\\to " + latexNode(point);
        out += "}";
      }
      return out + " " + latexNode(node.children[0]);
    }
    case NodeKind::Matrix: {
      std::string out = "\\begin{bmatrix}";
      for (size_t r = 0; r < node.children.size(); ++r) {
        if (r) out += " \\\\ ";
        const Node& row = node.children[r];
        for (size_t c = 0; c < row.children.size(); ++c) {
          if (c) out += " & ";
          if (!isBlank(row.children[c])) out += latexNode(row.children[c]);
        }
      }
      return out + "\\end{bmatrix}";
    }
    case NodeKind::Row:
      break;
  }

  std::string out;
  for (size_t i = 0; i < node.children.size(); ++i) {
    if (i) {
      const Node& previous = node.children[i - 1];
      const Node& current = node.children[i];
      // Keep digits and letters from colliding with the next atom.
      if (previous.isAtom() && current.isAtom()) {
        const bool leftWord = std::isalnum(
            static_cast<unsigned char>(previous.text.back()));
        const bool rightWord = std::isalnum(
            static_cast<unsigned char>(current.text.front()));
        if (leftWord && rightWord) out += "\\,";
      }
    }
    out += latexNode(node.children[i]);
  }
  return out;
}

}  // namespace

std::string renderLatex(const Node& node) { return latexNode(node); }

}  // namespace infinit::scan
