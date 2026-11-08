import 'models.dart';

/// One numbered step in a worked study example.
class StudyStep {
  const StudyStep(this.titleEn, this.titleFa, this.detailEn, this.detailFa,
      {this.math});

  final String titleEn;
  final String titleFa;
  final String detailEn;
  final String detailFa;
  final ExView? math;
}

/// A self-contained study article.  The short answer always comes first, then
/// the theory, then a fully worked example with numbered steps.
class StudyArticle {
  const StudyArticle({
    required this.id,
    required this.titleEn,
    required this.titleFa,
    required this.subtitleEn,
    required this.subtitleFa,
    required this.shortAnswerEn,
    required this.shortAnswerFa,
    required this.shortMath,
    required this.keyIdeasEn,
    required this.keyIdeasFa,
    required this.exampleProblemEn,
    required this.exampleProblemFa,
    required this.exampleAnswerEn,
    required this.exampleAnswerFa,
    required this.exampleAnswerMath,
    required this.steps,
    required this.pitfallsEn,
    required this.pitfallsFa,
    required this.sourceEn,
    required this.sourceFa,
  });

  final String id;
  final String titleEn;
  final String titleFa;
  final String subtitleEn;
  final String subtitleFa;
  final String shortAnswerEn;
  final String shortAnswerFa;
  final ExView shortMath;
  final List<String> keyIdeasEn;
  final List<String> keyIdeasFa;
  final String exampleProblemEn;
  final String exampleProblemFa;
  final String exampleAnswerEn;
  final String exampleAnswerFa;
  final ExView exampleAnswerMath;
  final List<StudyStep> steps;
  final String pitfallsEn;
  final String pitfallsFa;
  final String sourceEn;
  final String sourceFa;
}

const kStudyArticles = <StudyArticle>[
  StudyArticle(
    id: 'derivative',
    titleEn: 'Derivative',
    titleFa: 'مشتق',
    subtitleEn:
        'Instantaneous rate of change, the tangent line and the rules that make differentiation mechanical.',
    subtitleFa:
        'آهنگ تغییر لحظه‌ای، خط مماس و قواعدی که مشتق‌گیری را مکانیکی می‌کند.',
    shortAnswerEn:
        'The derivative is the instantaneous rate of change of a function. It is the limit of the difference quotient and equals the slope of the tangent line at that point.',
    shortAnswerFa:
        'مشتق، آهنگ تغییر لحظه‌ای یک تابع است. حد خارج‌قسمت تفاضلی است و برابر شیب خط مماس در آن نقطه می‌شود.',
    shortMath: ExView(
      plain: "f'(x) = lim_{h->0} (f(x+h) - f(x)) / h",
      pretty: "f′(x) = lim(h→0) [f(x+h) − f(x)] / h",
      latex: r"f'(x)=\lim_{h\to 0}\frac{f(x+h)-f(x)}{h}",
    ),
    keyIdeasEn: [
      'Average rate of change is a secant slope; the derivative is the limit as the two points merge.',
      'Power, product, quotient and chain rules turn the limit definition into fast algebra.',
      'A derivative is a function in its own right: f′ can be differentiated again to get f″.',
      'Implicit differentiation handles curves written as F(x, y) = 0, not just y = f(x).',
      'The sign of f′ gives increasing/decreasing behaviour; the sign of f″ gives concavity.',
    ],
    keyIdeasFa: [
      'آهنگ تغییر متوسط شیب خط قاطع است؛ مشتق حد آن هنگام نزدیک شدن دو نقطه است.',
      'قواعد توان، ضرب، تقسیم و زنجیره تعریف حدی را به جبر سریع تبدیل می‌کنند.',
      'مشتق خودش یک تابع است: می‌توان از f′ دوباره مشتق گرفت و f″ را ساخت.',
      'مشتق‌گیری ضمنی منحنی‌هایی را که به شکل F(x, y) = 0 نوشته شده‌اند، نه فقط y = f(x)، حل می‌کند.',
      'علامت f′ رفتار صعودی/نزولی و علامت f″ تقعر را نشان می‌دهد.',
    ],
    exampleProblemEn: 'Find f′(x) for f(x) = x³ + 2x, then write the tangent line at x = 1.',
    exampleProblemFa:
        'برای f(x) = x³ + 2x مشتق f′(x) را بیابید، سپس معادلهٔ خط مماس در x = 1 را بنویسید.',
    exampleAnswerEn: 'f′(x) = 3x² + 2 and the tangent line is y = 5x − 2.',
    exampleAnswerFa: 'f′(x) = 3x² + 2 و خط مماس y = 5x − 2 است.',
    exampleAnswerMath: ExView(
      plain: "f'(x) = 3x^2 + 2,  y = 5x - 2",
      pretty: 'f′(x) = 3x² + 2,   y = 5x − 2',
      latex: r"f'(x)=3x^2+2,\qquad y=5x-2",
    ),
    steps: [
      StudyStep(
        'Differentiate term by term',
        'جمله‌به‌جمله مشتق بگیرید',
        'Use the power rule d/dx[xⁿ] = n xⁿ⁻¹ on each term.',
        'از قاعدهٔ توان d/dx[xⁿ] = n xⁿ⁻¹ برای هر جمله استفاده کنید.',
        math: ExView(
          plain: "d/dx[x^3] = 3x^2,  d/dx[2x] = 2",
          pretty: 'd/dx[x³] = 3x²,   d/dx[2x] = 2',
          latex: r"\frac{d}{dx}x^3=3x^2,\qquad \frac{d}{dx}2x=2",
        ),
      ),
      StudyStep(
        'Write the derivative',
        'مشتق را بنویسید',
        'Add the two results. The derivative is valid for every real x.',
        'دو نتیجه را جمع کنید. مشتق برای همهٔ xهای حقیقی معتبر است.',
        math: ExView(
          plain: "f'(x) = 3x^2 + 2",
          pretty: 'f′(x) = 3x² + 2',
          latex: r"f'(x)=3x^2+2",
        ),
      ),
      StudyStep(
        'Evaluate the slope at x = 1',
        'شیب را در x = 1 حساب کنید',
        'Substitute x = 1 into f′ to get the tangent slope.',
        'x = 1 را در f′ جای‌گذاری کنید تا شیب مماس به‌دست آید.',
        math: ExView(
          plain: "f'(1) = 3(1)^2 + 2 = 5",
          pretty: 'f′(1) = 3(1)² + 2 = 5',
          latex: r"f'(1)=3(1)^2+2=5",
        ),
      ),
      StudyStep(
        'Find the point of tangency',
        'نقطهٔ تماس را پیدا کنید',
        'Evaluate the original function at x = 1.',
        'تابع اصلی را در x = 1 حساب کنید.',
        math: ExView(
          plain: 'f(1) = 1 + 2 = 3,  point (1, 3)',
          pretty: 'f(1) = 1 + 2 = 3,   نقطه (1, 3)',
          latex: r"f(1)=1+2=3,\qquad (1,3)",
        ),
      ),
      StudyStep(
        'Write the tangent line',
        'خط مماس را بنویسید',
        'Use point–slope form y − y₀ = m(x − x₀).',
        'از فرم نقطه–شیب y − y₀ = m(x − x₀) استفاده کنید.',
        math: ExView(
          plain: 'y - 3 = 5(x - 1)  =>  y = 5x - 2',
          pretty: 'y − 3 = 5(x − 1)  ⇒  y = 5x − 2',
          latex: r"y-3=5(x-1)\;\Longrightarrow\;y=5x-2",
        ),
      ),
    ],
    pitfallsEn:
        'The usual mistakes are forgetting the chain rule inside a composite function and treating the derivative as an average slope. A derivative is local: it depends on the behaviour arbitrarily close to the point.',
    pitfallsFa:
        'اشتباه‌های رایج فراموش کردن قاعدهٔ زنجیره در توابع مرکب و اشتباه گرفتن مشتق با شیب متوسط است. مشتق محلی است: به رفتار در نزدیکی بی‌نهایت نزدیک نقطه بستگی دارد.',
    sourceEn:
        'Follows Thomas’ Calculus, chapters 3–4: the derivative as a limit, differentiation rules, implicit differentiation and applications.',
    sourceFa:
        'بر پایهٔ حساب دیفرانسیل و انتگرال توماس، فصل‌های ۳ و ۴: مشتق به‌عنوان حد، قواعد مشتق‌گیری، مشتق ضمنی و کاربردها.',
  ),
  StudyArticle(
    id: 'integral',
    titleEn: 'Integral',
    titleFa: 'انتگرال',
    subtitleEn:
        'Signed area, the Riemann sum and the Fundamental Theorem that links integration to differentiation.',
    subtitleFa:
        'مساحت علامت‌دار، جمع ریمان و قضیهٔ اساسی که انتگرال‌گیری را به مشتق‌گیری پیوند می‌دهد.',
    shortAnswerEn:
        'The definite integral is the signed area between a curve and the x-axis. The Fundamental Theorem of Calculus says that integrating a rate recovers the total change.',
    shortAnswerFa:
        'انتگرال معین، مساحت علامت‌دار بین منحنی و محور x است. قضیهٔ اساسی حساب می‌گوید انتگرال‌گیری از یک آهنگ، تغییر کل را بازمی‌گرداند.',
    shortMath: ExView(
      plain: 'integral_a^b f(x) dx = F(b) - F(a),  F\' = f',
      pretty: '∫ₐᵇ f(x) dx = F(b) − F(a),   F′ = f',
      latex: r"\int_a^b f(x)\,dx=F(b)-F(a),\qquad F'=f",
    ),
    keyIdeasEn: [
      'A Riemann sum approximates area with rectangles; the integral is the limit as the rectangles get thin.',
      'An antiderivative reverses differentiation, so every derivative rule has an integral counterpart.',
      'Substitution reverses the chain rule; integration by parts reverses the product rule.',
      'Area below the axis counts as negative, which is why the integral is called signed area.',
      'Improper integrals take a limit at an infinite endpoint or a singularity.',
    ],
    keyIdeasFa: [
      'جمع ریمان مساحت را با مستطیل‌ها تقریب می‌زند؛ انتگرال حد آن هنگام نازک شدن مستطیل‌ها است.',
      'پادمشتق مشتق‌گیری را برمی‌گرداند، پس هر قاعدهٔ مشتق یک همتای انتگرالی دارد.',
      'جایگذاری قاعدهٔ زنجیره و انتگرال‌گیری جزء‌به‌جزء قاعدهٔ ضرب را برمی‌گرداند.',
      'مساحت زیر محور منفی شمرده می‌شود؛ به همین دلیل انتگرال را مساحت علامت‌دار می‌نامند.',
      'انتگرال‌های ناسره در انتهای بی‌نهایت یا تکینگی حد می‌گیرند.',
    ],
    exampleProblemEn: 'Evaluate ∫₀² (3x² + 2x) dx.',
    exampleProblemFa: 'مقدار ∫₀² (3x² + 2x) dx را حساب کنید.',
    exampleAnswerEn: 'The integral equals 12.',
    exampleAnswerFa: 'انتگرال برابر ۱۲ است.',
    exampleAnswerMath: ExView(
      plain: 'integral_0^2 (3x^2 + 2x) dx = 12',
      pretty: '∫₀² (3x² + 2x) dx = 12',
      latex: r"\int_0^2 (3x^2+2x)\,dx=12",
    ),
    steps: [
      StudyStep(
        'Find an antiderivative',
        'یک پادمشتق پیدا کنید',
        'Reverse the power rule: the antiderivative of xⁿ is xⁿ⁺¹/(n+1).',
        'قاعدهٔ توان را برعکس کنید: پادمشتق xⁿ برابر xⁿ⁺¹/(n+1) است.',
        math: ExView(
          plain: 'F(x) = x^3 + x^2',
          pretty: 'F(x) = x³ + x²',
          latex: r"F(x)=x^3+x^2",
        ),
      ),
      StudyStep(
        'Apply the Fundamental Theorem',
        'قضیهٔ اساسی را اعمال کنید',
        'Evaluate the antiderivative at the upper and lower limits.',
        'پادمشتق را در حد بالا و پایین حساب کنید.',
        math: ExView(
          plain: 'F(2) - F(0) = (8 + 4) - (0 + 0)',
          pretty: 'F(2) − F(0) = (8 + 4) − (0 + 0)',
          latex: r"F(2)-F(0)=(8+4)-(0+0)",
        ),
      ),
      StudyStep(
        'Simplify',
        'ساده کنید',
        'The definite integral is a number, not a family of functions.',
        'انتگرال معین یک عدد است، نه خانواده‌ای از توابع.',
        math: ExView(
          plain: '12',
          pretty: '12',
          latex: r"12",
        ),
      ),
      StudyStep(
        'Check by differentiating',
        'با مشتق‌گیری بررسی کنید',
        'Differentiate the antiderivative to recover the integrand.',
        'از پادمشتق مشتق بگیرید تا تابع زیر انتگرال بازگردد.',
        math: ExView(
          plain: "d/dx[x^3 + x^2] = 3x^2 + 2x",
          pretty: 'd/dx[x³ + x²] = 3x² + 2x',
          latex: r"\frac{d}{dx}(x^3+x^2)=3x^2+2x",
        ),
      ),
    ],
    pitfallsEn:
        'Do not drop the constant of integration in an indefinite integral, and do not forget to change the limits when you use substitution. A definite integral can be negative even when the geometric area is positive.',
    pitfallsFa:
        'در انتگرال نامعین ثابت انتگرال را حذف نکنید و هنگام جایگذاری، تغییر حدود را فراموش نکنید. انتگرال معین می‌تواند منفی باشد حتی وقتی مساحت هندسی مثبت است.',
    sourceEn:
        'Follows Thomas’ Calculus, chapters 5–8: Riemann sums, the Fundamental Theorem, substitution, integration by parts and improper integrals.',
    sourceFa:
        'بر پایهٔ حساب دیفرانسیل و انتگرال توماس، فصل‌های ۵ تا ۸: جمع‌های ریمان، قضیهٔ اساسی، جایگذاری، انتگرال‌گیری جزء‌به‌جزء و انتگرال‌های ناسره.',
  ),
  StudyArticle(
    id: 'ode_types',
    titleEn: 'Types of differential equations',
    titleFa: 'انواع معادلات دیفرانسیل',
    subtitleEn:
        'Order, linearity, homogeneity and the standard first-order forms, with the vocabulary Boyce uses throughout.',
    subtitleFa:
        'مرتبه، خطی بودن، همگنی و فرم‌های استاندارد مرتبهٔ اول، با واژگانی که بواز در سراسر کتاب به کار می‌برد.',
    shortAnswerEn:
        'A differential equation relates an unknown function to its derivatives. It is classified by order (highest derivative), by linearity, and by whether the right side is zero.',
    shortAnswerFa:
        'معادلهٔ دیفرانسیل یک تابع مجهول را به مشتق‌هایش مرتبط می‌کند. بر اساس مرتبه (بالاترین مشتق)، خطی بودن و صفر بودن سمت راست طبقه‌بندی می‌شود.',
    shortMath: ExView(
      plain: 'a_n(x) y^(n) + ... + a_1(x) y\' + a_0(x) y = g(x)',
      pretty: 'aₙ(x)y⁽ⁿ⁾ + … + a₁(x)y′ + a₀(x)y = g(x)',
      latex: r"a_n(x)y^{(n)}+\cdots+a_1(x)y'+a_0(x)y=g(x)",
    ),
    keyIdeasEn: [
      'Order is the highest derivative present; it is the number of constants in the general solution.',
      'Linear means y and its derivatives appear to the first power with coefficients depending only on x.',
      'Homogeneous means g(x) = 0; nonhomogeneous equations add a particular solution to the homogeneous one.',
      'An initial value problem fixes y and enough derivatives at one point; a boundary value problem fixes values at two points.',
      'First-order standard forms include separable, linear, exact, Bernoulli and homogeneous equations.',
    ],
    keyIdeasFa: [
      'مرتبه، بالاترین مشتق موجود است و تعداد ثابت‌های جواب عمومی را می‌دهد.',
      'خطی یعنی y و مشتق‌هایش با توان یک و ضرایبی که فقط به x بستگی دارند ظاهر شوند.',
      'همگن یعنی g(x) = 0؛ معادلهٔ ناهمگن یک جواب خصوصی به جواب همگن اضافه می‌کند.',
      'مسئلهٔ مقدار اولیه y و مشتق‌های کافی را در یک نقطه ثابت می‌کند؛ مسئلهٔ مقدار مرزی مقادیر را در دو نقطه.',
      'فرم‌های استاندارد مرتبهٔ اول شامل جداشدنی، خطی، کامل، برنولی و همگن است.',
    ],
    exampleProblemEn:
        'Classify y″ + 3y′ + 2y = eˣ and write its general solution.',
    exampleProblemFa:
        'معادلهٔ y″ + 3y′ + 2y = eˣ را طبقه‌بندی کنید و جواب عمومی آن را بنویسید.',
    exampleAnswerEn:
        'Second-order, linear, nonhomogeneous, constant-coefficient ODE. The general solution is y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6.',
    exampleAnswerFa:
        'معادلهٔ دیفرانسیل معمولی مرتبهٔ دوم، خطی، ناهمگن با ضرایب ثابت. جواب عمومی y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6 است.',
    exampleAnswerMath: ExView(
      plain: 'y = C_1 e^(-x) + C_2 e^(-2x) + e^x/6',
      pretty: 'y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6',
      latex: r"y=C_1e^{-x}+C_2e^{-2x}+\frac{e^x}{6}",
    ),
    steps: [
      StudyStep(
        'Identify the order',
        'مرتبه را مشخص کنید',
        'The highest derivative is y″, so the equation is second order.',
        'بالاترین مشتق y″ است، پس معادله مرتبهٔ دوم است.',
        math: ExView(
          plain: 'order = 2',
          pretty: 'order = 2',
          latex: r"\text{order}=2",
        ),
      ),
      StudyStep(
        'Check linearity and homogeneity',
        'خطی بودن و همگنی را بررسی کنید',
        'y, y′ and y″ appear to the first power with constant coefficients. The right side eˣ is nonzero, so it is nonhomogeneous.',
        'y، y′ و y″ با توان یک و ضرایب ثابت ظاهر شده‌اند. سمت راست eˣ ناصفر است، پس ناهمگن است.',
        math: ExView(
          plain: 'linear, nonhomogeneous, constant coefficients',
          pretty: 'linear, nonhomogeneous, constant coefficients',
          latex: r"\text{linear, nonhomogeneous, constant coefficients}",
        ),
      ),
      StudyStep(
        'Solve the homogeneous equation',
        'معادلهٔ همگن را حل کنید',
        'The characteristic equation is r² + 3r + 2 = 0, whose roots are −1 and −2.',
        'معادلهٔ مشخصه r² + 3r + 2 = 0 است که ریشه‌هایش −1 و −2 هستند.',
        math: ExView(
          plain: 'r = -1, -2  =>  y_h = C_1 e^(-x) + C_2 e^(-2x)',
          pretty: 'r = −1, −2  ⇒  yₕ = C₁e⁻ˣ + C₂e⁻²ˣ',
          latex: r"r=-1,-2\;\Rightarrow\;y_h=C_1e^{-x}+C_2e^{-2x}",
        ),
      ),
      StudyStep(
        'Find a particular solution',
        'یک جواب خصوصی پیدا کنید',
        'Guess yₚ = Aeˣ and substitute: A eˣ + 3A eˣ + 2A eˣ = eˣ, so 6A = 1.',
        'yₚ = Aeˣ را حدس بزنید و جای‌گذاری کنید: A eˣ + 3A eˣ + 2A eˣ = eˣ، پس 6A = 1.',
        math: ExView(
          plain: 'A = 1/6  =>  y_p = e^x/6',
          pretty: 'A = 1/6  ⇒  yₚ = eˣ/6',
          latex: r"A=\frac16\;\Rightarrow\;y_p=\frac{e^x}{6}",
        ),
      ),
      StudyStep(
        'Add the two parts',
        'دو بخش را جمع کنید',
        'The general solution is the homogeneous solution plus the particular solution.',
        'جواب عمومی برابر جواب همگن به‌علاوهٔ جواب خصوصی است.',
        math: ExView(
          plain: 'y = C_1 e^(-x) + C_2 e^(-2x) + e^x/6',
          pretty: 'y = C₁e⁻ˣ + C₂e⁻²ˣ + eˣ/6',
          latex: r"y=C_1e^{-x}+C_2e^{-2x}+\frac{e^x}{6}",
        ),
      ),
    ],
    pitfallsEn:
        'Order is not the same as degree. A nonlinear term such as y·y′ or sin(y) immediately makes the equation nonlinear. For a nonhomogeneous equation, never forget the particular solution.',
    pitfallsFa:
        'مرتبه با درجه یکی نیست. جملهٔ غیرخطی مانند y·y′ یا sin(y) فوراً معادله را غیرخطی می‌کند. در معادلهٔ ناهمگن هرگز جواب خصوصی را فراموش نکنید.',
    sourceEn:
        'Follows Boyce & DiPrima, Differential Equations and Boundary Value Problems, chapters 1–3: classification, first-order methods and linear equations.',
    sourceFa:
        'بر پایهٔ بواز و دی‌پریما، معادلات دیفرانسیل و مسائل مقدار مرزی، فصل‌های ۱ تا ۳: طبقه‌بندی، روش‌های مرتبهٔ اول و معادلات خطی.',
  ),
  StudyArticle(
    id: 'ode_applications',
    titleEn: 'Applications of differential equations',
    titleFa: 'کاربردهای معادلات دیفرانسیل',
    subtitleEn:
        'Turn a physical statement into an equation, solve it, then read the prediction back in the original units.',
    subtitleFa:
        'یک بیان فیزیکی را به معادله تبدیل کنید، حل کنید، سپس پیش‌بینی را با واحدهای اصلی بخوانید.',
    shortAnswerEn:
        'A differential equation models how a quantity changes. The solution predicts the future state from an initial condition, and the constants in the model come from the physical data.',
    shortAnswerFa:
        'معادلهٔ دیفرانسیل نحوهٔ تغییر یک کمیت را مدل می‌کند. جواب حالت آینده را از یک شرط اولیه پیش‌بینی می‌کند و ثابت‌های مدل از داده‌های فیزیکی می‌آیند.',
    shortMath: ExView(
      plain: "model -> solve -> apply initial condition -> interpret",
      pretty: 'model → solve → apply initial condition → interpret',
      latex: r"\text{model}\to\text{solve}\to\text{initial condition}\to\text{interpret}",
    ),
    keyIdeasEn: [
      'State the assumptions and define every variable with its units before writing the equation.',
      'Exponential growth/decay: y′ = ky, so y = y₀e^{kt}.',
      'Newton cooling: T′ = −k(T − Tₘ), so the temperature difference decays exponentially.',
      'Logistic growth: P′ = rP(1 − P/K) gives an S-curve that levels off at K.',
      'Spring–mass and series LRC circuits share m y″ + c y′ + k y = F(t).',
    ],
    keyIdeasFa: [
      'پیش از نوشتن معادله، فرض‌ها را بیان و هر متغیر را با واحدش تعریف کنید.',
      'رشد/واپاشی نمایی: y′ = ky، پس y = y₀e^{kt}.',
      'سرد شدن نیوتن: T′ = −k(T − Tₘ)، پس اختلاف دما به‌صورت نمایی کاهش می‌یابد.',
      'رشد لجستیک: P′ = rP(1 − P/K) منحنی S می‌دهد که در K مسطح می‌شود.',
      'سیستم جرم و فنر و مدار سری LRC هر دو معادلهٔ m y″ + c y′ + k y = F(t) را دارند.',
    ],
    exampleProblemEn:
        'A cup at 90 °C sits in a 20 °C room with cooling constant k = 0.1 min⁻¹. Find its temperature after 10 minutes.',
    exampleProblemFa:
        'فنجانی با دمای ۹۰ درجه در اتاقی با دمای ۲۰ درجه و ثابت سرد شدن k = 0.1 بر دقیقه قرار دارد. دمای آن پس از ۱۰ دقیقه چقدر است؟',
    exampleAnswerEn: 'T(10) = 20 + 70e⁻¹ ≈ 45.8 °C.',
    exampleAnswerFa: 'T(10) = 20 + 70e⁻¹ ≈ ۴۵٫۸ درجه.',
    exampleAnswerMath: ExView(
      plain: 'T(10) = 20 + 70 e^(-1) ~= 45.8',
      pretty: 'T(10) = 20 + 70e⁻¹ ≈ 45.8',
      latex: r"T(10)=20+70e^{-1}\approx 45.8",
    ),
    steps: [
      StudyStep(
        'Write the balance law',
        'قانون موازنه را بنویسید',
        'The rate of cooling is proportional to the difference from room temperature.',
        'آهنگ سرد شدن با اختلاف دما با محیط متناسب است.',
        math: ExView(
          plain: "T' = -k(T - T_m)",
          pretty: 'T′ = −k(T − Tₘ)',
          latex: r"T'=-k(T-T_m)",
        ),
      ),
      StudyStep(
        'Solve the linear equation',
        'معادلهٔ خطی را حل کنید',
        'Separate or use an integrating factor. The general solution has one constant.',
        'جدا کنید یا از عامل انتگرال‌گیر استفاده کنید. جواب عمومی یک ثابت دارد.',
        math: ExView(
          plain: 'T(t) = T_m + C e^(-kt)',
          pretty: 'T(t) = Tₘ + C e⁻ᵏᵗ',
          latex: r"T(t)=T_m+Ce^{-kt}",
        ),
      ),
      StudyStep(
        'Apply the initial temperature',
        'دمای اولیه را اعمال کنید',
        'At t = 0, T = 90, so C = 90 − 20 = 70.',
        'در t = 0 داریم T = 90، پس C = 90 − 20 = 70.',
        math: ExView(
          plain: 'T(t) = 20 + 70 e^(-0.1 t)',
          pretty: 'T(t) = 20 + 70e⁻⁰·¹ᵗ',
          latex: r"T(t)=20+70e^{-0.1t}",
        ),
      ),
      StudyStep(
        'Evaluate at t = 10',
        'در t = 10 حساب کنید',
        'Substitute the requested time and keep the units.',
        'زمان خواسته‌شده را جای‌گذاری کنید و واحدها را نگه دارید.',
        math: ExView(
          plain: 'T(10) = 20 + 70 e^(-1) ~= 45.8',
          pretty: 'T(10) = 20 + 70e⁻¹ ≈ 45.8',
          latex: r"T(10)=20+70e^{-1}\approx 45.8",
        ),
      ),
      StudyStep(
        'Interpret the answer',
        'جواب را تفسیر کنید',
        'The cup is still above room temperature and will approach 20 °C as t grows.',
        'فنجان هنوز بالاتر از دمای اتاق است و با بزرگ شدن t به ۲۰ درجه نزدیک می‌شود.',
        math: ExView(
          plain: 'T -> 20 as t -> infinity',
          pretty: 'T → 20 وقتی t → ∞',
          latex: r"T\to 20\ \text{as}\ t\to\infty",
        ),
      ),
    ],
    pitfallsEn:
        'Keep the units consistent and check that the answer is physically sensible. A negative temperature or a population above the carrying capacity signals an algebra or sign error, not a new phenomenon.',
    pitfallsFa:
        'واحدها را سازگار نگه دارید و بررسی کنید جواب از نظر فیزیکی معقول باشد. دمای منفی یا جمعیت بالاتر از ظرفیت حمل نشانهٔ خطای جبری یا علامت است، نه پدیدهٔ جدید.',
    sourceEn:
        'Follows Boyce & DiPrima, chapters 2–3: exponential models, cooling, mixing, logistic populations and mechanical/electrical oscillations.',
    sourceFa:
        'بر پایهٔ بواز و دی‌پریما، فصل‌های ۲ و ۳: مدل‌های نمایی، سرد شدن، اختلاط، جمعیت لجستیک و نوسان‌های مکانیکی/الکتریکی.',
  ),
];
