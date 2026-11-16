/// A link from an explanation to the workbench for its topic.
///
/// `seed` pre-fills the destination when the page accepts an equation; pages
/// with their own structured inputs open with their defaults instead.
class ToolLink {
  const ToolLink({required this.labelKey, required this.page, this.seed});

  /// Catalogue key of the tool name, usually a drawer label.
  final String labelKey;
  final int page;
  final String? seed;
}
