#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cloud.hpp"
#include "ink.hpp"

namespace infinit::scan {

/// One labelled handwriting sample kept as **strokes**, not as a raster.
///
/// The plan (§12A.5) asks for personal prototypes that match the way the user
/// actually writes, and a 16x16 bitmap throws away stroke order, stroke count
/// and the difference between a dot and a tiny circle. A point cloud keeps
/// exactly the information the $P matcher needs, and it is plain data: no
/// model file, no training.
struct InkPrototype {
  std::string label;
  std::vector<InkStroke> strokes;
  PointCloud cloud;
};

/// The personal stroke bank. Wire format, little endian:
///
///   "IINK" | u32 version=1 | u32 count
///   count * ( u8 labelLength | label
///             u32 strokeCount
///             strokeCount * ( u32 pointCount | pointCount * (f32 x, f32 y) ) )
///
/// Storage is capped per class and near-duplicates are pruned, so matching
/// stays fast however long the user keeps correcting.
class InkPrototypeBank {
 public:
  bool loadFromMemory(const uint8_t* data, std::size_t size,
                      std::string* error = nullptr);
  std::vector<uint8_t> serialize() const;

  /// Adds one sample. Returns false for an empty label or empty strokes.
  bool add(const std::string& label, const std::vector<InkStroke>& strokes);

  /// kNN vote over the $P cloud distance for one stroke group.
  /// `ids` index into `strokes`; an empty label means "no opinion".
  CloudMatch match(const std::vector<InkStroke>& strokes,
                   const std::vector<int>& ids, std::size_t k = 5) const;

  void clear() { entries_.clear(); }
  bool empty() const { return entries_.empty(); }
  std::size_t size() const { return entries_.size(); }

 private:
  /// Drops the worst samples of `label` beyond the cap and near-duplicates, so
  /// one impatient user cannot fill the bank with the same "x" twenty times.
  void prune(const std::string& label);

  std::vector<InkPrototype> entries_;
};

/// Maximum samples kept per class.
constexpr std::size_t kInkPrototypesPerClass = 12;

}  // namespace infinit::scan
