import 'package:flutter/material.dart';

import 'i18n/catalog.dart';
import 'l10n/ar.dart';
import 'l10n/de.dart';
import 'l10n/de_topics.dart';
import 'l10n/es.dart';
import 'l10n/fa.dart';
import 'l10n/fr.dart';
import 'l10n/hi.dart';
import 'l10n/ja.dart';
import 'l10n/pt.dart';
import 'l10n/ru.dart';
import 'l10n/tr.dart';
import 'l10n/zh.dart';

/// One selectable interface language. Adding a language means adding an entry
/// here plus a map in `l10n/`, never touching the native engine.
class AppLanguage {
  const AppLanguage(
    this.code,
    this.englishName,
    this.nativeName, {
    this.rtl = false,
  });

  final String code;
  final String englishName;
  final String nativeName;
  final bool rtl;
}

const kAppLanguages = <AppLanguage>[
  AppLanguage('en', 'English', 'English'),
  AppLanguage('fa', 'Persian', 'فارسی', rtl: true),
  AppLanguage('ar', 'Arabic', 'العربية', rtl: true),
  AppLanguage('es', 'Spanish', 'Español'),
  AppLanguage('fr', 'French', 'Français'),
  AppLanguage('de', 'German', 'Deutsch'),
  AppLanguage('ru', 'Russian', 'Русский'),
  AppLanguage('zh', 'Chinese', '中文'),
  AppLanguage('ja', 'Japanese', '日本語'),
  AppLanguage('hi', 'Hindi', 'हिन्दी'),
  AppLanguage('pt', 'Portuguese', 'Português'),
  AppLanguage('tr', 'Turkish', 'Türkçe'),
];

const _translations = <String, Map<String, String>>{
  'fa': faTranslations,
  'ar': arTranslations,
  'es': esTranslations,
  'fr': frTranslations,
  'de': deTranslations,
  'ru': ruTranslations,
  'zh': zhTranslations,
  'ja': jaTranslations,
  'hi': hiTranslations,
  'pt': ptTranslations,
  'tr': trTranslations,
};

/// Short labels shared by the shell and the quick preference chips.
const _extra = <String, Map<String, String>>{
  'fa': {
    'System': 'سیستم',
    'Light': 'روشن',
    'Dark': 'تاریک',
    'Worked example': 'نمونهٔ حل',
    'Linear first-order': 'خطی مرتبهٔ اول',
    'Integrating factor': 'عامل انتگرال‌گیر',
    'Substitute back': 'جای‌گذاری در معادله',
    'Clear': 'پاک کردن',
    'Close menu': 'بستن منو',
    'Solve, apply, transform': 'حل، کاربرد، تبدیل',
    'Limits, derivatives, integrals': 'حد، مشتق، انتگرال',
    'Study notes': 'یادداشت‌های آموزشی',
  },
  'ar': {
    'System': 'النظام',
    'Light': 'فاتح',
    'Dark': 'داكن',
    'Worked example': 'مثال محلول',
    'Linear first-order': 'خطية من الرتبة الأولى',
    'Integrating factor': 'عامل التكامل',
    'Substitute back': 'التعويض في المعادلة',
    'Clear': 'مسح',
    'Close menu': 'إغلاق القائمة',
    'Solve, apply, transform': 'حل، تطبيق، تحويل',
    'Limits, derivatives, integrals': 'الحدود، المشتقات، التكاملات',
    'Study notes': 'ملاحظات دراسية',
  },
  'es': {
    'System': 'Sistema',
    'Light': 'Claro',
    'Dark': 'Oscuro',
    'Worked example': 'Ejemplo resuelto',
    'Linear first-order': 'Lineal de primer orden',
    'Integrating factor': 'Factor integrante',
    'Substitute back': 'Sustituir en la ecuación',
    'Clear': 'Borrar',
    'Close menu': 'Cerrar menú',
    'Solve, apply, transform': 'Resolver, aplicar, transformar',
    'Limits, derivatives, integrals': 'Límites, derivadas, integrales',
    'Study notes': 'Notas de estudio',
  },
  'fr': {
    'System': 'Système',
    'Light': 'Clair',
    'Dark': 'Sombre',
    'Worked example': 'Exemple résolu',
    'Linear first-order': 'Linéaire du premier ordre',
    'Integrating factor': 'Facteur intégrant',
    'Substitute back': 'Substituer dans l’équation',
    'Clear': 'Effacer',
    'Close menu': 'Fermer le menu',
    'Solve, apply, transform': 'Résoudre, appliquer, transformer',
    'Limits, derivatives, integrals': 'Limites, dérivées, intégrales',
    'Study notes': "Notes d'étude",
  },
  'de': {
    'System': 'System',
    'Light': 'Hell',
    'Dark': 'Dunkel',
    'Worked example': 'Gerechnetes Beispiel',
    'Linear first-order': 'Linear erster Ordnung',
    'Integrating factor': 'Integrierender Faktor',
    'Substitute back': 'In die Gleichung einsetzen',
    'Clear': 'Löschen',
    'Close menu': 'Menü schließen',
    'Solve, apply, transform': 'Lösen, anwenden, transformieren',
    'Limits, derivatives, integrals': 'Grenzwerte, Ableitungen, Integrale',
    'Study notes': 'Lernnotizen',
  },
  'ru': {
    'System': 'Система',
    'Light': 'Светлая',
    'Dark': 'Тёмная',
    'Worked example': 'Разобранный пример',
    'Linear first-order': 'Линейное первого порядка',
    'Integrating factor': 'Интегрирующий множитель',
    'Substitute back': 'Подстановка в уравнение',
    'Clear': 'Очистить',
    'Close menu': 'Закрыть меню',
    'Solve, apply, transform': 'Решать, применять, преобразовывать',
    'Limits, derivatives, integrals': 'Пределы, производные, интегралы',
    'Study notes': 'Учебные заметки',
  },
  'zh': {
    'System': '系统',
    'Light': '浅色',
    'Dark': '深色',
    'Worked example': '示例',
    'Linear first-order': '一阶线性',
    'Integrating factor': '积分因子',
    'Substitute back': '代回方程',
    'Clear': '清除',
    'Close menu': '关闭菜单',
    'Solve, apply, transform': '求解、应用、变换',
    'Limits, derivatives, integrals': '极限、导数、积分',
    'Study notes': '学习笔记',
  },
  'ja': {
    'System': 'システム',
    'Light': 'ライト',
    'Dark': 'ダーク',
    'Worked example': '例題',
    'Linear first-order': '1階線形',
    'Integrating factor': '積分因子',
    'Substitute back': '方程式に代入',
    'Clear': 'クリア',
    'Close menu': 'メニューを閉じる',
    'Solve, apply, transform': '解く・応用・変換',
    'Limits, derivatives, integrals': '極限・微分・積分',
    'Study notes': '学習ノート',
  },
  'hi': {
    'System': 'सिस्टम',
    'Light': 'लाइट',
    'Dark': 'डार्क',
    'Worked example': 'हल किया उदाहरण',
    'Linear first-order': 'प्रथम कोटि रैखिक',
    'Integrating factor': 'समाकलन गुणक',
    'Substitute back': 'समीकरण में प्रतिस्थापित करें',
    'Clear': 'साफ़ करें',
    'Close menu': 'मेनू बंद करें',
    'Solve, apply, transform': 'हल करें, लागू करें, रूपांतरित करें',
    'Limits, derivatives, integrals': 'सीमाएँ, अवकलज, समाकल',
    'Study notes': 'अध्ययन नोट्स',
  },
  'pt': {
    'System': 'Sistema',
    'Light': 'Claro',
    'Dark': 'Escuro',
    'Worked example': 'Exemplo resolvido',
    'Linear first-order': 'Linear de primeira ordem',
    'Integrating factor': 'Fator integrante',
    'Substitute back': 'Substituir na equação',
    'Clear': 'Limpar',
    'Close menu': 'Fechar menu',
    'Solve, apply, transform': 'Resolver, aplicar, transformar',
    'Limits, derivatives, integrals': 'Limites, derivadas, integrais',
    'Study notes': 'Notas de estudo',
  },
  'tr': {
    'System': 'Sistem',
    'Light': 'Açık',
    'Dark': 'Koyu',
    'Worked example': 'Çözümlü örnek',
    'Linear first-order': 'Birinci mertebe lineer',
    'Integrating factor': 'İntegral çarpanı',
    'Substitute back': 'Denklemde yerine koy',
    'Clear': 'Temizle',
    'Close menu': 'Menüyü kapat',
    'Solve, apply, transform': 'Çöz, uygula, dönüştür',
    'Limits, derivatives, integrals': 'Limitler, türevler, integraller',
    'Study notes': 'Çalışma notları',
  },
};

AppLanguage languageFor(String code) => kAppLanguages.firstWhere(
  (language) => language.code == code,
  orElse: () => kAppLanguages.first,
);

bool isSupportedLanguage(String code) =>
    kAppLanguages.any((language) => language.code == code);


/// Labels introduced by the study articles, the linear-algebra workbench and
/// the shared topic pages.  They live in one table so every language gets the
/// same set of keys without touching the older `_extra` maps.
const _study = <String, Map<String, String>>{
  'fa': {
    'Linear algebra': 'جبر خطی',
    'Matrices & eigenvalues': 'ماتریس و مقدار ویژه',
    'Educational explanations': 'توضیحات آموزشی',
    'Derivative': 'مشتق',
    'Integral': 'انتگرال',
    'Types of differential equations': 'انواع معادلات دیفرانسیل',
    'Applications of differential equations': 'کاربردهای معادلات دیفرانسیل',
    'Show the answer and steps': 'نمایش جواب و مراحل',
    'Computing…': 'در حال محاسبه…',
    'Short answer': 'جواب کوتاه',
    'Key ideas': 'ایده‌های کلیدی',
    'Simplest answer': 'ساده‌ترین جواب',
    'Step by step': 'مرحله به مرحله',
    'Common pitfalls': 'اشتباه‌های رایج',
    'Examples': 'نمونه‌ها',
    'The computation engine is unavailable.': 'موتور محاسبات در دسترس نیست.',
  },
  'ar': {
    'Linear algebra': 'الجبر الخطي',
    'Matrices & eigenvalues': 'المصفوفات والقيم الذاتية',
    'Educational explanations': 'شروحات تعليمية',
    'Derivative': 'المشتقة',
    'Integral': 'التكامل',
    'Types of differential equations': 'أنواع المعادلات التفاضلية',
    'Applications of differential equations': 'تطبيقات المعادلات التفاضلية',
    'Show the answer and steps': 'إظهار الإجابة والخطوات',
    'Computing…': 'جارٍ الحساب…',
    'Short answer': 'إجابة قصيرة',
    'Key ideas': 'الأفكار الأساسية',
    'Simplest answer': 'أبسط إجابة',
    'Step by step': 'خطوة بخطوة',
    'Common pitfalls': 'الأخطاء الشائعة',
    'Examples': 'أمثلة',
    'The computation engine is unavailable.': 'محرك الحساب غير متاح.',
  },
  'es': {
    'Linear algebra': 'Álgebra lineal',
    'Matrices & eigenvalues': 'Matrices y valores propios',
    'Educational explanations': 'Explicaciones educativas',
    'Derivative': 'Derivada',
    'Integral': 'Integral',
    'Types of differential equations': 'Tipos de ecuaciones diferenciales',
    'Applications of differential equations': 'Aplicaciones de las ecuaciones diferenciales',
    'Show the answer and steps': 'Mostrar la respuesta y los pasos',
    'Computing…': 'Calculando…',
    'Short answer': 'Respuesta breve',
    'Key ideas': 'Ideas clave',
    'Simplest answer': 'Respuesta más simple',
    'Step by step': 'Paso a paso',
    'Common pitfalls': 'Errores comunes',
    'Examples': 'Ejemplos',
    'The computation engine is unavailable.': 'El motor de cálculo no está disponible.',
  },
  'fr': {
    'Linear algebra': 'Algèbre linéaire',
    'Matrices & eigenvalues': 'Matrices et valeurs propres',
    'Educational explanations': 'Explications pédagogiques',
    'Derivative': 'Dérivée',
    'Integral': 'Intégrale',
    'Types of differential equations': 'Types d’équations différentielles',
    'Applications of differential equations': 'Applications des équations différentielles',
    'Show the answer and steps': 'Afficher la réponse et les étapes',
    'Computing…': 'Calcul en cours…',
    'Short answer': 'Réponse courte',
    'Key ideas': 'Idées clés',
    'Simplest answer': 'Réponse la plus simple',
    'Step by step': 'Étape par étape',
    'Common pitfalls': 'Erreurs fréquentes',
    'Examples': 'Exemples',
    'The computation engine is unavailable.': 'Le moteur de calcul n’est pas disponible.',
  },
  'de': {
    'Linear algebra': 'Lineare Algebra',
    'Matrices & eigenvalues': 'Matrizen und Eigenwerte',
    'Educational explanations': 'Lernerklärungen',
    'Derivative': 'Ableitung',
    'Integral': 'Integral',
    'Types of differential equations': 'Arten von Differentialgleichungen',
    'Applications of differential equations': 'Anwendungen von Differentialgleichungen',
    'Show the answer and steps': 'Antwort und Schritte anzeigen',
    'Computing…': 'Wird berechnet…',
    'Short answer': 'Kurze Antwort',
    'Key ideas': 'Kernideen',
    'Simplest answer': 'Einfachste Antwort',
    'Step by step': 'Schritt für Schritt',
    'Common pitfalls': 'Häufige Fehler',
    'Examples': 'Beispiele',
    'The computation engine is unavailable.': 'Die Rechen-Engine ist nicht verfügbar.',
  },
  'ru': {
    'Linear algebra': 'Линейная алгебра',
    'Matrices & eigenvalues': 'Матрицы и собственные значения',
    'Educational explanations': 'Учебные объяснения',
    'Derivative': 'Производная',
    'Integral': 'Интеграл',
    'Types of differential equations': 'Типы дифференциальных уравнений',
    'Applications of differential equations': 'Применения дифференциальных уравнений',
    'Show the answer and steps': 'Показать ответ и шаги',
    'Computing…': 'Вычисление…',
    'Short answer': 'Краткий ответ',
    'Key ideas': 'Ключевые идеи',
    'Simplest answer': 'Простейший ответ',
    'Step by step': 'Шаг за шагом',
    'Common pitfalls': 'Типичные ошибки',
    'Examples': 'Примеры',
    'The computation engine is unavailable.': 'Вычислительный движок недоступен.',
  },
  'zh': {
    'Linear algebra': '线性代数',
    'Matrices & eigenvalues': '矩阵与特征值',
    'Educational explanations': '教学讲解',
    'Derivative': '导数',
    'Integral': '积分',
    'Types of differential equations': '微分方程的类型',
    'Applications of differential equations': '微分方程的应用',
    'Show the answer and steps': '显示答案和步骤',
    'Computing…': '正在计算…',
    'Short answer': '简短答案',
    'Key ideas': '核心思想',
    'Simplest answer': '最简答案',
    'Step by step': '逐步解答',
    'Common pitfalls': '常见错误',
    'Examples': '示例',
    'The computation engine is unavailable.': '计算引擎不可用。',
  },
  'ja': {
    'Linear algebra': '線形代数',
    'Matrices & eigenvalues': '行列と固有値',
    'Educational explanations': '学習解説',
    'Derivative': '導関数',
    'Integral': '積分',
    'Types of differential equations': '微分方程式の種類',
    'Applications of differential equations': '微分方程式の応用',
    'Show the answer and steps': '答えと手順を表示',
    'Computing…': '計算中…',
    'Short answer': '短い答え',
    'Key ideas': '重要な考え方',
    'Simplest answer': '最も簡単な答え',
    'Step by step': 'ステップごと',
    'Common pitfalls': 'よくある間違い',
    'Examples': '例',
    'The computation engine is unavailable.': '計算エンジンが利用できません。',
  },
  'hi': {
    'Linear algebra': 'रैखिक बीजगणित',
    'Matrices & eigenvalues': 'आव्यूह और आइगेनमान',
    'Educational explanations': 'शैक्षिक व्याख्याएँ',
    'Derivative': 'अवकलज',
    'Integral': 'समाकल',
    'Types of differential equations': 'अवकल समीकरणों के प्रकार',
    'Applications of differential equations': 'अवकल समीकरणों के अनुप्रयोग',
    'Show the answer and steps': 'उत्तर और चरण दिखाएँ',
    'Computing…': 'गणना हो रही है…',
    'Short answer': 'संक्षिप्त उत्तर',
    'Key ideas': 'मुख्य विचार',
    'Simplest answer': 'सरलतम उत्तर',
    'Step by step': 'चरण दर चरण',
    'Common pitfalls': 'सामान्य गलतियाँ',
    'Examples': 'उदाहरण',
    'The computation engine is unavailable.': 'गणना इंजन उपलब्ध नहीं है।',
  },
  'pt': {
    'Linear algebra': 'Álgebra linear',
    'Matrices & eigenvalues': 'Matrizes e valores próprios',
    'Educational explanations': 'Explicações educativas',
    'Derivative': 'Derivada',
    'Integral': 'Integral',
    'Types of differential equations': 'Tipos de equações diferenciais',
    'Applications of differential equations': 'Aplicações das equações diferenciais',
    'Show the answer and steps': 'Mostrar a resposta e os passos',
    'Computing…': 'Calculando…',
    'Short answer': 'Resposta curta',
    'Key ideas': 'Ideias-chave',
    'Simplest answer': 'Resposta mais simples',
    'Step by step': 'Passo a passo',
    'Common pitfalls': 'Erros comuns',
    'Examples': 'Exemplos',
    'The computation engine is unavailable.': 'O motor de cálculo não está disponível.',
  },
  'tr': {
    'Linear algebra': 'Lineer cebir',
    'Matrices & eigenvalues': 'Matrisler ve özdeğerler',
    'Educational explanations': 'Eğitim açıklamaları',
    'Derivative': 'Türev',
    'Integral': 'İntegral',
    'Types of differential equations': 'Diferansiyel denklem türleri',
    'Applications of differential equations': 'Diferansiyel denklemlerin uygulamaları',
    'Show the answer and steps': 'Cevabı ve adımları göster',
    'Computing…': 'Hesaplanıyor…',
    'Short answer': 'Kısa cevap',
    'Key ideas': 'Temel fikirler',
    'Simplest answer': 'En basit cevap',
    'Step by step': 'Adım adım',
    'Common pitfalls': 'Yaygın hatalar',
    'Examples': 'Örnekler',
    'The computation engine is unavailable.': 'Hesaplama motoru kullanılamıyor.',
  },
};

class L10n {
  const L10n(this.locale);

  final Locale locale;

  String get code => locale.languageCode;

  /// Persian still drives the native engine and the Persian-only study notes.
  bool get isFa => code == 'fa';

  bool get isRtl => languageFor(code).rtl;

  /// The single source of sentence direction for the UI layer.
  TextDirection get direction => isRtl ? TextDirection.rtl : TextDirection.ltr;

  /// Looks up a stable dotted key in the JSON catalogue.  Until M7 removes the
  /// legacy tables, an English source string keeps resolving through them, so
  /// the second argument is either the Persian source or a parameter map.
  String t(String en, [Object? faOrParams]) {
    final key = en.trim();
    if (I18n.instance.hasKey(key)) {
      final params = faOrParams is Map<String, Object?> ? faOrParams : null;
      // Languages still backed by the legacy Dart tables keep their wording:
      // resolve the key to its English source and look that up first.
      if (code != 'fa' && code != 'en') {
        final source = I18n.instance.english(key);
        if (source != null) {
          final legacy =
              _study[code]?[source] ??
              _extra[code]?[source] ??
              deTopicTranslations[code]?[source] ??
              _translations[code]?[source];
          if (legacy != null) return legacy;
        }
      }
      return I18n.instance.text(code, key, params);
    }
    final hit =
        _study[code]?[key] ??
        _extra[code]?[key] ??
        deTopicTranslations[code]?[key] ??
        _translations[code]?[key];
    if (hit != null) return hit;
    final fa = faOrParams is String ? faOrParams : null;
    if (code == 'fa' && fa != null) return fa;
    return en;
  }

  static L10n of(BuildContext context) => L10n(Localizations.localeOf(context));
}
