import 'package:flutter/material.dart';

class L10n {
  const L10n(this.isFa);

  final bool isFa;

  String t(String en, String fa) => isFa ? fa : en;

  static L10n of(BuildContext context) =>
      L10n(Localizations.localeOf(context).languageCode == 'fa');
}
