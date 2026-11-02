// Reference catalogue of the theorems of the derivative.
//
// The content is data, not logic: the theorems page renders these entries and
// each one carries its own bilingual statement, LaTeX and pitfalls. Keeping it
// here means adding a theorem never touches a widget.
import 'l10n.dart';

/// One theorem of the derivative, complete in both languages.
class TheoremEntry {
  const TheoremEntry({
    required this.id,
    required this.titleEn,
    required this.titleFa,
    required this.statementEn,
    required this.statementFa,
    required this.latex,
    required this.hypothesesEn,
    required this.hypothesesFa,
    required this.whyEn,
    required this.whyFa,
    this.pitfallsEn = const [],
    this.pitfallsFa = const [],
  });

  final String id;
  final String titleEn;
  final String titleFa;
  final String statementEn;
  final String statementFa;

  /// Formal statement in LaTeX, rendered with flutter_math_fork.
  final String latex;
  final List<String> hypothesesEn;
  final List<String> hypothesesFa;

  /// Geometric meaning or intuition.
  final String whyEn;
  final String whyFa;
  final List<String> pitfallsEn;
  final List<String> pitfallsFa;

  String title(L10n l) => l.isFa ? titleFa : titleEn;
  String statement(L10n l) => l.isFa ? statementFa : statementEn;
  String why(L10n l) => l.isFa ? whyFa : whyEn;
  List<String> hypotheses(L10n l) => l.isFa ? hypothesesFa : hypothesesEn;
  List<String> pitfalls(L10n l) => l.isFa ? pitfallsFa : pitfallsEn;
}

/// The derivative theorems, in teaching order.
const theoremCatalog = <TheoremEntry>[
  TheoremEntry(
    id: 'fermat',
    titleEn: "Fermat's theorem",
    titleFa: 'قضیهٔ فرما',
    statementEn:
        'If f has a local maximum or minimum at an interior point c of its domain and f is '
        'differentiable there, then f\'(c) = 0.',
    statementFa:
        'اگر تابع f در نقطهٔ داخلی c از دامنه‌اش اکسترمم محلی داشته باشد و در آن نقطه مشتق‌پذیر '
        'باشد، آنگاه f′(c) = 0.',
    latex:
        r"f'(c) = 0 \quad \text{at a local extremum where } f \text{ is differentiable}",
    hypothesesEn: [
      'c is an interior point of the domain.',
      'f has a local extremum at c.',
      'f is differentiable at c.',
    ],
    hypothesesFa: [
      'نقطهٔ c داخلی دامنه باشد.',
      'تابع در c اکسترمم محلی داشته باشد.',
      'تابع در c مشتق‌پذیر باشد.',
    ],
    whyEn:
        'At a smooth peak or trough the tangent flattens out, so the instantaneous rate of '
        'change must be zero.',
    whyFa:
        'در یک قله یا درهٔ هموار، مماس افقی می‌شود؛ پس آهنگ تغییر لحظه‌ای باید صفر باشد.',
    pitfallsEn: [
      'The converse is false: f\'(c) = 0 does not guarantee an extremum (consider f(x) = x³ at 0).',
      'The point must be interior; an endpoint can be an extremum without f\' being zero.',
    ],
    pitfallsFa: [
      'عکس قضیه درست نیست: f′(c) = 0 تضمین‌کنندهٔ اکسترمم نیست (مثلاً f(x) = x³ در نقطهٔ ۰).',
      'نقطه باید داخلی باشد؛ دو سر بازه می‌توانند بدون صفر بودن مشتق، اکسترمم باشند.',
    ],
  ),
  TheoremEntry(
    id: 'rolle',
    titleEn: "Rolle's theorem",
    titleFa: 'قضیهٔ رول',
    statementEn:
        'If f is continuous on [a, b], differentiable on (a, b), and f(a) = f(b), then there '
        'exists at least one c in (a, b) with f\'(c) = 0.',
    statementFa:
        'اگر f روی [a, b] پیوسته و روی (a, b) مشتق‌پذیر باشد و f(a) = f(b)، آنگاه حداقل یک نقطهٔ '
        'c در (a, b) وجود دارد که f′(c) = 0.',
    latex:
        r"f(a)=f(b) \;\Rightarrow\; \exists\, c \in (a,b):\; f'(c)=0",
    hypothesesEn: [
      'f is continuous on the closed interval [a, b].',
      'f is differentiable on the open interval (a, b).',
      'f(a) = f(b).',
    ],
    hypothesesFa: [
      'f روی بازهٔ بستهٔ [a, b] پیوسته باشد.',
      'f روی بازهٔ باز (a, b) مشتق‌پذیر باشد.',
      'f(a) = f(b).',
    ],
    whyEn:
        'If the graph starts and ends at the same height, it must turn around somewhere inside '
        'and that turning point has a horizontal tangent.',
    whyFa:
        'اگر نمودار از یک ارتفاع شروع و به همان ارتفاع تمام شود، باید جایی در میان راه برگردد و '
        'در آن نقطهٔ برگشت مماس افقی است.',
    pitfallsEn: [
      'Without f(a) = f(b) the conclusion can fail completely.',
      'Rolle is the special horizontal case of the Mean Value Theorem.',
    ],
    pitfallsFa: [
      'بدون شرط f(a) = f(b) نتیجه می‌تواند کاملاً برقرار نباشد.',
      'قضیهٔ رول حالت خاص و افقی قضیهٔ مقدار میانگین است.',
    ],
  ),
  TheoremEntry(
    id: 'mvt',
    titleEn: 'Mean Value Theorem (Lagrange)',
    titleFa: 'قضیهٔ مقدار میانگین (لاگرانژ)',
    statementEn:
        'If f is continuous on [a, b] and differentiable on (a, b), then there exists at least '
        'one c in (a, b) such that f\'(c) equals the secant slope (f(b) − f(a)) / (b − a).',
    statementFa:
        'اگر f روی [a, b] پیوسته و روی (a, b) مشتق‌پذیر باشد، حداقل یک نقطهٔ c در (a, b) وجود '
        'دارد که f′(c) برابر شیب خط قاطع (f(b) − f(a)) / (b − a) است.',
    latex:
        r"f'(c) = \frac{f(b)-f(a)}{b-a} \quad \text{for some } c \in (a,b)",
    hypothesesEn: [
      'f is continuous on [a, b].',
      'f is differentiable on (a, b).',
      'a < b.',
    ],
    hypothesesFa: [
      'f روی [a, b] پیوسته باشد.',
      'f روی (a, b) مشتق‌پذیر باشد.',
      'a < b باشد.',
    ],
    whyEn:
        'Somewhere the instantaneous rate of change must equal the average rate of change over '
        'the whole interval, so some tangent is parallel to the secant.',
    whyFa:
        'جایی آهنگ تغییر لحظه‌ای باید با آهنگ تغییر میانگین روی کل بازه برابر شود؛ پس مماسی '
        'موازی خط قاطع وجود دارد.',
    pitfallsEn: [
      'Continuity is required on the CLOSED interval and differentiability on the OPEN one.',
      'The theorem promises existence; it does not tell you where until you solve for c.',
      'f(x) = |x| on [−1, 1] shows differentiability cannot be dropped.',
    ],
    pitfallsFa: [
      'پیوستگی روی بازهٔ بسته و مشتق‌پذیری روی بازهٔ باز لازم است.',
      'قضیه فقط وجود را تضمین می‌کند؛ محل دقیق c را باید با حل معادله پیدا کرد.',
      'تابع f(x) = |x| روی [−1, 1] نشان می‌دهد شرط مشتق‌پذیری را نمی‌توان حذف کرد.',
    ],
  ),
  TheoremEntry(
    id: 'cauchy',
    titleEn: "Cauchy's Mean Value Theorem",
    titleFa: 'قضیهٔ کوشی (مقدار میانگین تعمیم‌یافته)',
    statementEn:
        'If f and g are continuous on [a, b] and differentiable on (a, b), and g\'(x) ≠ 0 on '
        '(a, b), then there exists c in (a, b) with f\'(c)/g\'(c) equal to '
        '(f(b) − f(a)) / (g(b) − g(a)).',
    statementFa:
        'اگر f و g روی [a, b] پیوسته و روی (a, b) مشتق‌پذیر باشند و روی (a, b) داشته باشیم '
        'g′(x) ≠ 0، آنگاه نقطهٔ c در (a, b) وجود دارد که f′(c)/g′(c) برابر '
        '(f(b) − f(a)) / (g(b) − g(a)) است.',
    latex:
        r"\frac{f'(c)}{g'(c)} = \frac{f(b)-f(a)}{g(b)-g(a)} \quad \text{for some } c \in (a,b)",
    hypothesesEn: [
      'Both f and g are continuous on [a, b].',
      'Both are differentiable on (a, b).',
      "g'(x) ≠ 0 on (a, b), which also gives g(b) ≠ g(a).",
    ],
    hypothesesFa: [
      'هر دو تابع f و g روی [a, b] پیوسته باشند.',
      'هر دو روی (a, b) مشتق‌پذیر باشند.',
      'روی (a, b) داشته باشیم g′(x) ≠ 0 که نتیجه می‌دهد g(b) ≠ g(a).',
    ],
    whyEn:
        'It compares the two functions through a common parameter: each is a curve, and the '
        'theorem locates a point where their tangent directions match the overall change.',
    whyFa:
        'این قضیه دو تابع را با یک پارامتر مشترک مقایسه می‌کند: هر کدام یک منحنی است و قضیه '
        'نقطه‌ای را می‌یابد که جهت مماس آن‌ها با تغییر کلی هم‌خوان است.',
    pitfallsEn: [
      'Taking g(x) = x recovers the ordinary Mean Value Theorem.',
      'The condition on g is what prevents division by zero in the conclusion.',
      "Cauchy's theorem is the tool behind L'Hôpital's rule.",
    ],
    pitfallsFa: [
      'با انتخاب g(x) = x همان قضیهٔ مقدار میانگین معمولی به دست می‌آید.',
      'شرط روی g همان چیزی است که تقسیم بر صفر را در نتیجه ناممکن می‌کند.',
      'قضیهٔ کوشی ابزار پشت قاعدهٔ هوپیتال است.',
    ],
  ),
  TheoremEntry(
    id: 'lhopital',
    titleEn: "L'Hôpital's rule",
    titleFa: 'قاعدهٔ هوپیتال',
    statementEn:
        'If f and g are differentiable near a and both tend to 0 or both tend to ±∞ at a, and '
        'the limit of f\'(x)/g\'(x) exists, then the limit of f(x)/g(x) equals that same limit.',
    statementFa:
        'اگر f و g نزدیک a مشتق‌پذیر باشند و هر دو در a به ۰ یا هر دو به ±∞ میل کنند و حد '
        'f′(x)/g′(x) موجود باشد، آنگاه حد f(x)/g(x) با همان حد برابر است.',
    latex:
        r"\lim_{x\to a}\frac{f(x)}{g(x)} = \lim_{x\to a}\frac{f'(x)}{g'(x)}",
    hypothesesEn: [
      'The quotient is an indeterminate form 0/0 or ±∞/±∞ at a.',
      "f and g are differentiable near a, with g'(x) ≠ 0 there.",
      "The limit of f'/g' exists (or is infinite).",
    ],
    hypothesesFa: [
      'کسر در نقطهٔ a شکل مبهم 0/0 یا ±∞/±∞ داشته باشد.',
      'f و g نزدیک a مشتق‌پذیر باشند و در آنجا g′(x) ≠ 0 باشد.',
      'حد f′/g′ موجود باشد (یا نامتناهی باشد).',
    ],
    whyEn:
        'Near the point the two functions are dominated by their linear behaviour, so comparing '
        'their slopes replaces comparing their values.',
    whyFa:
        'نزدیک نقطه، رفتار دو تابع را جملهٔ خطی آن‌ها تعیین می‌کند؛ پس مقایسهٔ شیب‌ها جای مقایسهٔ '
        'مقادیر را می‌گیرد.',
    pitfallsEn: [
      'Never apply it before verifying the form really is indeterminate.',
      'The rule applies to the quotient as a whole, not to products or sums term by term.',
      'For 0·∞, ∞−∞, 1^∞, 0^0 and ∞^0, rewrite the expression into a 0/0 or ∞/∞ quotient first.',
      "If the new limit is worse, stop: the rule gave no information.",
    ],
    pitfallsFa: [
      'پیش از تأیید مبهم بودن شکل، هرگز آن را اعمال نکنید.',
      'قاعده به کل کسر اعمال می‌شود، نه جمله‌به‌جمله به ضرب یا جمع.',
      'برای حالت‌های 0·∞، ∞−∞، 1^∞، 0^0 و ∞^0 ابتدا عبارت را به کسر 0/0 یا ∞/∞ بازنویسی کنید.',
      'اگر حد جدید پیچیده‌تر شد، متوقف شوید؛ قاعده اطلاعاتی نداده است.',
    ],
  ),
  TheoremEntry(
    id: 'zero_derivative',
    titleEn: 'Zero derivative implies constant',
    titleFa: 'مشتق صفر روی بازه یعنی تابع ثابت',
    statementEn:
        'If f\'(x) = 0 for every x in an interval, then f is constant on that interval.',
    statementFa:
        'اگر برای هر x در یک بازه f′(x) = 0 باشد، آنگاه f روی آن بازه ثابت است.',
    latex: r"f'(x)=0 \;\forall x \in I \;\Rightarrow\; f \text{ is constant on } I",
    hypothesesEn: [
      'The set is a single interval, not a union of separate pieces.',
      'f is differentiable with zero derivative throughout it.',
    ],
    hypothesesFa: [
      'مجموعه یک بازهٔ واحد باشد، نه اجتماع چند تکهٔ جدا.',
      'f روی تمام آن مشتق‌پذیر و مشتقش صفر باشد.',
    ],
    whyEn:
        'Two functions with the same derivative differ by a constant, which is why an '
        'antiderivative always carries a + C.',
    whyFa:
        'دو تابع با مشتق یکسان فقط با یک ثابت فرق دارند؛ به همین دلیل پادمشتق همیشه یک + C دارد.',
    pitfallsEn: [
      'On a disconnected domain the conclusion can fail piecewise.',
      'This is what justifies writing one arbitrary constant in an indefinite integral.',
    ],
    pitfallsFa: [
      'روی دامنهٔ ناهمبند، نتیجه می‌تواند تکه‌تکه برقرار نباشد.',
      'همین قضیه نوشتن یک ثابت دلخواه در انتگرال نامعین را توجیه می‌کند.',
    ],
  ),
  TheoremEntry(
    id: 'monotonicity',
    titleEn: 'Monotonicity test',
    titleFa: 'آزمون یکنوایی',
    statementEn:
        "If f'(x) > 0 on an interval then f is strictly increasing there; if f'(x) < 0 then f is "
        'strictly decreasing there.',
    statementFa:
        'اگر روی بازه‌ای f′(x) > 0 باشد، f روی آن اکیداً صعودی است؛ و اگر f′(x) < 0 باشد، f روی '
        'آن اکیداً نزولی است.',
    latex:
        r"f'(x) > 0 \Rightarrow f \nearrow \qquad f'(x) < 0 \Rightarrow f \searrow",
    hypothesesEn: [
      'f is continuous on the interval and differentiable on its interior.',
      'The sign of f\' is constant on the interval.',
    ],
    hypothesesFa: [
      'f روی بازه پیوسته و در داخل آن مشتق‌پذیر باشد.',
      'علامت f′ روی آن بازه ثابت باشد.',
    ],
    whyEn:
        'A positive slope means the graph climbs as you move right, so the sign of the '
        'derivative reads directly as the direction of the function.',
    whyFa:
        'شیب مثبت یعنی با حرکت به راست نمودار بالا می‌رود؛ پس علامت مشتق مستقیماً جهت تابع را '
        'نشان می‌دهد.',
    pitfallsEn: [
      'The intervals of monotonicity are separated by the critical points of f.',
      "A single critical point does not by itself determine the sign on either side.",
    ],
    pitfallsFa: [
      'بازه‌های یکنوایی با نقطه‌های بحرانی f از هم جدا می‌شوند.',
      'یک نقطهٔ بحرانی به‌تنهایی علامت دو طرف را مشخص نمی‌کند.',
    ],
  ),
  TheoremEntry(
    id: 'first_derivative_test',
    titleEn: 'First derivative test',
    titleFa: 'آزمون مشتق اول',
    statementEn:
        "At a critical point c: if f' changes from positive to negative, c is a local maximum; "
        "if it changes from negative to positive, c is a local minimum; if it keeps its sign, c "
        'is neither.',
    statementFa:
        'در نقطهٔ بحرانی c: اگر f′ از مثبت به منفی تغییر کند، c بیشینهٔ محلی است؛ اگر از منفی به '
        'مثبت تغییر کند، c کمینهٔ محلی است؛ و اگر علامتش تغییر نکند، هیچ‌کدام نیست.',
    latex:
        r"f': + \to - \Rightarrow \text{max} \qquad f': - \to + \Rightarrow \text{min}",
    hypothesesEn: [
      'c is a critical point of f.',
      'f is continuous at c and differentiable on a punctured neighbourhood of it.',
    ],
    hypothesesFa: [
      'c نقطهٔ بحرانی f باشد.',
      'f در c پیوسته و روی همسایگی سوراخ‌شدهٔ آن مشتق‌پذیر باشد.',
    ],
    whyEn:
        'The function switches from climbing to falling exactly at a maximum and the reverse at '
        'a minimum, so the sign change is the decisive evidence.',
    whyFa:
        'تابع دقیقاً در بیشینه از صعود به نزول و در کمینه از نزول به صعود تغییر می‌کند؛ پس '
        'تغییر علامت شاهد قاطع است.',
    pitfallsEn: [
      'The test needs a genuine sign change on both sides, not just f\'(c) = 0.',
      'If f\' does not change sign, the point is a stationary turning point rather than an extremum.',
    ],
    pitfallsFa: [
      'آزمون به تغییر علامت واقعی در دو طرف نیاز دارد، نه فقط f′(c) = 0.',
      'اگر علامت f′ تغییر نکند، آن نقطه یک نقطهٔ تخت است نه اکسترمم.',
    ],
  ),
  TheoremEntry(
    id: 'second_derivative_test',
    titleEn: 'Second derivative test',
    titleFa: 'آزمون مشتق دوم',
    statementEn:
        "If f'(c) = 0 and f''(c) > 0 then c is a local minimum; if f'(c) = 0 and f''(c) < 0 then "
        'c is a local maximum. If f\'\'(c) = 0 the test is inconclusive.',
    statementFa:
        'اگر f′(c) = 0 و f″(c) > 0 باشد، c کمینهٔ محلی است؛ اگر f′(c) = 0 و f″(c) < 0 باشد، c '
        'بیشینهٔ محلی است. اگر f″(c) = 0 باشد، آزمون نتیجه‌ای نمی‌دهد.',
    latex:
        r"f'(c)=0,\; f''(c)>0 \Rightarrow \text{min} \qquad f'(c)=0,\; f''(c)<0 \Rightarrow \text{max}",
    hypothesesEn: [
      "f'(c) = 0.",
      "f'' exists and is continuous near c.",
      "f''(c) ≠ 0 for a definite conclusion.",
    ],
    hypothesesFa: [
      'داشته باشیم f′(c) = 0.',
      'مشتق دوم نزدیک c موجود و پیوسته باشد.',
      'برای نتیجهٔ قطعی، f″(c) ≠ 0 باشد.',
    ],
    whyEn:
        'Concavity is the tiebreaker: an upward-opening curve at a flat point is a valley, and a '
        'downward-opening one is a peak.',
    whyFa:
        'تقعر داور نهایی است: نمودار رو به بالا در نقطه‌ای تخت، دره و نمودار رو به پایین، قله است.',
    pitfallsEn: [
      'When f\'\'(c) = 0 nothing is decided; fall back to the first derivative test.',
      'This test only applies at stationary points, not at endpoints.',
    ],
    pitfallsFa: [
      'وقتی f″(c) = 0 باشد هیچ چیز تعیین نمی‌شود؛ به آزمون مشتق اول برگردید.',
      'این آزمون فقط در نقطه‌های سکون کاربرد دارد، نه در دو سر بازه.',
    ],
  ),
  TheoremEntry(
    id: 'inflection',
    titleEn: 'Inflection point criterion',
    titleFa: 'معیار نقطهٔ عطف',
    statementEn:
        "If f'' exists and changes sign at c, then (c, f(c)) is an inflection point and the "
        'concavity of f flips there.',
    statementFa:
        'اگر f″ موجود باشد و در نقطهٔ c علامت عوض کند، آنگاه (c, f(c)) نقطهٔ عطف است و تقعر f در '
        'آن تغییر می‌کند.',
    latex: r"f'' \text{ changes sign at } c \;\Rightarrow\; (c, f(c)) \text{ is an inflection point}",
    hypothesesEn: [
      "f'' exists on a neighbourhood of c.",
      "f'' changes sign across c.",
    ],
    hypothesesFa: [
      'مشتق دوم روی همسایگی c موجود باشد.',
      'f″ در دو طرف c علامت عوض کند.',
    ],
    whyEn:
        'Concavity describes which way the curve bends; when the bending direction flips, the '
        'curve has an inflection point.',
    whyFa:
        'تقعر نشان می‌دهد نمودار به کدام سمت خم می‌شود؛ وقتی جهت خم‌شدن عوض شود، منحنی نقطهٔ عطف '
        'دارد.',
    pitfallsEn: [
      "f''(c) = 0 alone is not enough; an actual sign change is required.",
      'f(x) = x⁴ has f\'\'(0) = 0 but no inflection at the origin.',
    ],
    pitfallsFa: [
      'صفر بودن f″(c) به‌تنهایی کافی نیست؛ تغییر علامت واقعی لازم است.',
      'تابع f(x) = x⁴ در مبدأ f″(0) = 0 دارد ولی نقطهٔ عطف ندارد.',
    ],
  ),
  TheoremEntry(
    id: 'evt',
    titleEn: 'Extreme Value Theorem (Weierstrass)',
    titleFa: 'قضیهٔ مقدار اکسترمم (وایرشتراس)',
    statementEn:
        'If f is continuous on a closed bounded interval [a, b], then f attains both an absolute '
        'maximum and an absolute minimum on that interval.',
    statementFa:
        'اگر f روی بازهٔ بستهٔ کراندار [a, b] پیوسته باشد، آنگاه f روی آن بازه هم بیشینهٔ مطلق و '
        'هم کمینهٔ مطلق را اختیار می‌کند.',
    latex:
        r"f \in C[a,b] \;\Rightarrow\; \exists\, x_1,x_2 \in [a,b]:\; f(x_1) \le f(x) \le f(x_2)",
    hypothesesEn: [
      'The interval is closed and bounded.',
      'f is continuous on the whole interval.',
    ],
    hypothesesFa: [
      'بازه بسته و کراندار باشد.',
      'f روی تمام بازه پیوسته باشد.',
    ],
    whyEn:
        'A continuous curve on a finite closed interval cannot escape to infinity or sneak past '
        'an endpoint, so it must reach its highest and lowest values.',
    whyFa:
        'یک منحنی پیوسته روی بازهٔ بستهٔ متناهی نمی‌تواند به بی‌نهایت بگریزد یا از دو سر بازه '
        'بگذرد؛ پس ناچار به بیشترین و کمترین مقدار خود می‌رسد.',
    pitfallsEn: [
      'Both assumptions matter: f(x) = 1/x on (0, 1] and f(x) = x on (0, 1) both fail.',
      'It guarantees the extremes exist; finding them still needs the critical-point work.',
    ],
    pitfallsFa: [
      'هر دو فرض مهم‌اند: f(x) = 1/x روی (0, 1] و f(x) = x روی (0, 1) هر دو نقض می‌شوند.',
      'این قضیه وجود اکسترمم را تضمین می‌کند؛ یافتن آن همچنان به بررسی نقطه‌های بحرانی نیاز دارد.',
    ],
  ),
  TheoremEntry(
    id: 'differentiable_implies_continuous',
    titleEn: 'Differentiable implies continuous',
    titleFa: 'مشتق‌پذیری نتیجه می‌دهد پیوستگی',
    statementEn:
        'If f is differentiable at c, then f is continuous at c. The converse fails: continuity '
        'does not imply differentiability.',
    statementFa:
        'اگر f در c مشتق‌پذیر باشد، آنگاه f در c پیوسته است. عکس آن درست نیست: پیوستگی '
        'مشتق‌پذیری را نتیجه نمی‌دهد.',
    latex:
        r"f \text{ differentiable at } c \;\Rightarrow\; f \text{ continuous at } c",
    hypothesesEn: [
      'f is differentiable at c.',
    ],
    hypothesesFa: [
      'f در c مشتق‌پذیر باشد.',
    ],
    whyEn:
        'A finite slope forces the graph to close up into a single unbroken point, so a jump or a '
        'hole is impossible wherever a derivative exists.',
    whyFa:
        'شیب متناهی باعث می‌شود نمودار به یک نقطهٔ به‌هم‌پیوسته برسد؛ پس هرجا مشتق موجود باشد، '
        'پرش یا حفره ناممکن است.',
    pitfallsEn: [
      'f(x) = |x| is continuous but not differentiable at 0.',
      'A vertical tangent gives an infinite slope and therefore no derivative.',
    ],
    pitfallsFa: [
      'تابع f(x) = |x| پیوسته است ولی در ۰ مشتق‌پذیر نیست.',
      'مماس قائم شیب نامتناهی و در نتیجه مشتق ندارد.',
    ],
  ),
  TheoremEntry(
    id: 'darboux',
    titleEn: "Darboux's theorem",
    titleFa: 'قضیهٔ داربوکس',
    statementEn:
        'If f is differentiable on an interval, then f\' has the intermediate value property '
        'there: it takes every value between any two of its values.',
    statementFa:
        'اگر f روی بازه‌ای مشتق‌پذیر باشد، آنگاه f′ روی آن خاصیت مقدار میانی دارد: هر مقداری '
        'میان دو مقدار خود را اختیار می‌کند.',
    latex:
        r"f'(a) < k < f'(b) \;\Rightarrow\; \exists\, c:\; f'(c)=k",
    hypothesesEn: [
      'f is differentiable on the interval.',
      "The value k lies between f'(a) and f'(b).",
    ],
    hypothesesFa: [
      'f روی بازه مشتق‌پذیر باشد.',
      'مقدار k میان f′(a) و f′(b) قرار داشته باشد.',
    ],
    whyEn:
        'Derivatives cannot jump, even though they need not be continuous; this is a surprising '
        'and useful rigidity property.',
    whyFa:
        'مشتق‌ها نمی‌توانند جهش کنند، هرچند لازم نیست پیوسته باشند؛ این خاصیت صلبیت شگفت‌آور و '
        'سودمندی است.',
    pitfallsEn: [
      'The derivative need not be continuous, so the ordinary IVT does not apply to it directly.',
    ],
    pitfallsFa: [
      'مشتق لازم نیست پیوسته باشد، پس قضیهٔ مقدار میانی معمولی مستقیماً بر آن اعمال نمی‌شود.',
    ],
  ),
  TheoremEntry(
    id: 'taylor',
    titleEn: "Taylor's theorem with Lagrange remainder",
    titleFa: 'قضیهٔ تیلور با باقی‌ماندهٔ لاگرانژ',
    statementEn:
        'If f has n + 1 derivatives on an interval containing a and x, then f(x) equals its '
        'degree n Taylor polynomial at a plus a remainder involving f⁽ⁿ⁺¹⁾ at some point between '
        'a and x.',
    statementFa:
        'اگر f روی بازه‌ای شامل a و x دارای n + 1 مشتق باشد، آنگاه f(x) برابر چندجمله‌ای تیلور '
        'درجهٔ n حول a به‌علاوهٔ باقی‌مانده‌ای است که مشتق مرتبهٔ n + 1 را در نقطه‌ای میان a و x '
        'در بر می‌گیرد.',
    latex:
        r"f(x)=\sum_{k=0}^{n}\frac{f^{(k)}(a)}{k!}(x-a)^k+\frac{f^{(n+1)}(\xi)}{(n+1)!}(x-a)^{n+1}",
    hypothesesEn: [
      'f has n + 1 continuous derivatives on the interval.',
      'ξ lies strictly between a and x.',
    ],
    hypothesesFa: [
      'f روی بازه n + 1 مشتق پیوسته داشته باشد.',
      'نقطهٔ ξ اکیداً میان a و x قرار داشته باشد.',
    ],
    whyEn:
        'A smooth function is locally indistinguishable from a polynomial, and the remainder '
        'measures exactly how good that approximation is.',
    whyFa:
        'یک تابع هموار به‌طور موضعی از یک چندجمله‌ای قابل تشخیص نیست، و باقی‌مانده دقیقاً میزان '
        'خطای این تقریب را می‌سنجد.',
    pitfallsEn: [
      'The remainder gives an error bound, not the exact error.',
      'The point ξ is guaranteed to exist but is not identified.',
    ],
    pitfallsFa: [
      'باقی‌مانده یک کران خطا می‌دهد، نه خطای دقیق.',
      'وجود نقطهٔ ξ تضمین می‌شود ولی خود آن مشخص نمی‌شود.',
    ],
  ),
  TheoremEntry(
    id: 'linear_approximation',
    titleEn: 'Linear approximation and the differential',
    titleFa: 'تقریب خطی و دیفرانسیل',
    statementEn:
        'If f is differentiable at a, then for x near a we have f(x) ≈ f(a) + f\'(a)(x − a), and '
        'the error is small compared with (x − a).',
    statementFa:
        'اگر f در a مشتق‌پذیر باشد، برای x نزدیک a داریم f(x) ≈ f(a) + f′(a)(x − a) و خطا در '
        'مقایسه با (x − a) ناچیز است.',
    latex:
        r"f(x) \approx f(a) + f'(a)(x-a) \qquad \Delta y \approx f'(a)\,\Delta x",
    hypothesesEn: [
      'f is differentiable at a.',
      'x is close enough to a for the error to be acceptable.',
    ],
    hypothesesFa: [
      'f در a مشتق‌پذیر باشد.',
      'x به اندازهٔ کافی به a نزدیک باشد تا خطا قابل قبول بماند.',
    ],
    whyEn:
        'Zooming in far enough makes any smooth curve look like its tangent line, which is the '
        'practical meaning of the derivative.',
    whyFa:
        'با بزرگ‌نمایی کافی، هر منحنی هموار شبیه خط مماسش دیده می‌شود؛ این معنای کاربردی مشتق است.',
    pitfallsEn: [
      'The approximation degrades quickly away from a.',
      'It is only first order: the error grows like (x − a)².',
    ],
    pitfallsFa: [
      'دقت تقریب با دور شدن از a به‌سرعت کاهش می‌یابد.',
      'این تقریب از مرتبهٔ اول است: خطا مانند (x − a)² رشد می‌کند.',
    ],
  ),
  TheoremEntry(
    id: 'differentiation_rules',
    titleEn: 'The differentiation rules',
    titleFa: 'قواعد مشتق‌گیری',
    statementEn:
        'Product, quotient, chain and inverse function rules, stated as the algebra that turns '
        'the derivatives of the pieces into the derivative of the whole.',
    statementFa:
        'قواعد ضرب، تقسیم، زنجیره‌ای و تابع وارون، به‌صورت جبری که مشتق اجزا را به مشتق کل تبدیل '
        'می‌کند.',
    latex:
        r"(fg)'=f'g+fg' \qquad \left(\frac{f}{g}\right)'=\frac{f'g-fg'}{g^{2}} \qquad \frac{dy}{dx}=\frac{dy}{du}\frac{du}{dx}",
    hypothesesEn: [
      'Each rule needs the relevant derivatives to exist.',
      'The quotient rule needs g(x) ≠ 0.',
      'The inverse rule needs f\'(a) ≠ 0 at the point.',
    ],
    hypothesesFa: [
      'هر قاعده به وجود مشتق‌های مربوط نیاز دارد.',
      'قاعدهٔ تقسیم به g(x) ≠ 0 نیاز دارد.',
      'قاعدهٔ تابع وارون در نقطه به f′(a) ≠ 0 نیاز دارد.',
    ],
    whyEn:
        'Differentiation is linear plus a small set of structural rules; every derivative is '
        'built by combining them.',
    whyFa:
        'مشتق‌گیری خطی است به‌علاوهٔ چند قاعدهٔ ساختاری کوچک؛ هر مشتق از ترکیب همین‌ها ساخته می‌شود.',
    pitfallsEn: [
      'The derivative of a product is not the product of the derivatives.',
      'The chain rule leaves the inner function intact as a factor.',
      'The inverse rule gives the reciprocal slope, not the inverse of the function value.',
    ],
    pitfallsFa: [
      'مشتق ضرب برابر ضرب مشتق‌ها نیست.',
      'قاعدهٔ زنجیره‌ای تابع داخلی را به‌عنوان یک عامل دست‌نخورده نگه می‌دارد.',
      'قاعدهٔ تابع وارون وارون شیب را می‌دهد، نه وارون مقدار تابع را.',
    ],
  ),
];
