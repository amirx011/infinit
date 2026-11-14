// Infinit Lab — shared special functions and quadrature.
// SPDX-License-Identifier: Apache-2.0
#include "special_functions.hpp"

#include <cmath>
#include <limits>

namespace ode {
namespace {

constexpr double kPi = 3.14159265358979323846;

double harmonic(int m) {
    if (m <= 0) return 0.0;
    double h = 0.0;
    for (int k = 1; k <= m; ++k) h += 1.0 / k;
    return h;
}

} // namespace

double besselJ(double nu, double x, int terms) {
    if (!std::isfinite(nu) || !std::isfinite(x) || x < 0)
        return std::numeric_limits<double>::quiet_NaN();
#if defined(__cpp_lib_math_special_functions)
    (void)terms;
    // The standard library uses the same series for small arguments but
    // switches to stable asymptotics for large ones.  The plain series loses
    // all significant digits once x exceeds roughly 20.
    return std::cyl_bessel_j(nu, x);
#else
    if (x == 0) return nu == 0 ? 1.0 : 0.0;
    const double half = x / 2.0;
    const int needed = std::max(terms, (int)(2.5 * x) + 40);
    double sum = std::pow(half, nu) / std::tgamma(nu + 1.0);
    double term = sum;
    for (int k = 1; k <= needed; ++k) {
        term *= -(half * half) / ((double)k * (nu + k));
        sum += term;
        if (std::fabs(term) < 1e-18 * std::fabs(sum)) break;
    }
    return sum;
#endif
}

double besselY(double nu, double x, int terms) {
    if (!std::isfinite(nu) || !std::isfinite(x) || x <= 0)
        return std::numeric_limits<double>::quiet_NaN();
#if defined(__cpp_lib_math_special_functions)
    (void)terms;
    return std::cyl_neumann(nu, x);
#else
    const double nRounded = std::round(nu);
    const bool integer = std::fabs(nu - nRounded) < 1e-12;
    if (!integer) {
        const double s = std::sin(kPi * nu);
        return (besselJ(nu, x, terms) * std::cos(kPi * nu) - besselJ(-nu, x, terms)) / s;
    }
    const int n = (int)nRounded;
    const double half = x / 2.0;
    constexpr double eulerGamma = 0.5772156649015328606;
    double sum = (2.0 / kPi) * besselJ(nu, x, terms) * std::log(half);
    for (int k = 0; k < n; ++k) {
        double numerator = 1.0;
        for (int j = 0; j < n - k; ++j) numerator *= (j + 1);
        double denominator = 1.0;
        for (int j = 1; j <= k; ++j) denominator *= j;
        sum -= (1.0 / kPi) * (numerator / denominator) * std::pow(half, 2 * k - n);
    }
    for (int k = 0; k <= terms; ++k) {
        double factK = 1.0, factNK = 1.0;
        for (int j = 1; j <= k; ++j) factK *= j;
        for (int j = 1; j <= n + k; ++j) factNK *= j;
        const double psiK = -eulerGamma + harmonic(k);
        const double psiNK = -eulerGamma + harmonic(n + k);
        const double term = (k % 2 == 0 ? 1.0 : -1.0) *
                            (psiK + psiNK) / (factK * factNK) *
                            std::pow(half, 2 * k + n);
        sum -= (1.0 / kPi) * term;
    }
    return sum;
#endif
}

double besselJPrime(double nu, double x) {
    if (x <= 0 || !std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    return 0.5 * (besselJ(nu - 1.0, x) - besselJ(nu + 1.0, x));
}

double besselZero(int n, double nu) {
    if (n < 1 || nu < 0 || !std::isfinite(nu))
        return std::numeric_limits<double>::quiet_NaN();
    // The first zero never precedes this lower bound; subsequent zeros are
    // separated by roughly pi, so a fixed scan step is safe.
    double x = std::max(1e-6, nu + 1e-6);
    double f = besselJ(nu, x);
    int found = 0;
    const double step = 0.1;
    for (int iter = 0; iter < 100000 && x < 1e6; ++iter) {
        const double next = x + step;
        const double fNext = besselJ(nu, next);
        if (std::isfinite(f) && std::isfinite(fNext) && f * fNext < 0) {
            double lo = x, hi = next;
            double flo = f;
            for (int k = 0; k < 200; ++k) {
                const double mid = 0.5 * (lo + hi);
                const double fm = besselJ(nu, mid);
                if (flo * fm <= 0) {
                    hi = mid;
                } else {
                    lo = mid;
                    flo = fm;
                }
                if (hi - lo < 1e-14 * std::max(1.0, hi)) break;
            }
            ++found;
            if (found == n) return 0.5 * (lo + hi);
        }
        x = next;
        f = fNext;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

double legendreP(int n, double x) {
    if (n < 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 0) return 1.0;
    if (n == 1) return x;
    double p0 = 1.0, p1 = x;
    for (int k = 2; k <= n; ++k) {
        const double p2 = ((2.0 * k - 1.0) * x * p1 - (k - 1.0) * p0) / (double)k;
        p0 = p1;
        p1 = p2;
    }
    return p1;
}

double legendrePrime(int n, double x) {
    if (n < 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 0) return 0.0;
    const double denom = 1.0 - x * x;
    if (std::fabs(denom) < 1e-12) {
        const double magnitude = 0.5 * n * (n + 1.0);
        if (x > 0) return magnitude;
        return (n % 2 == 1) ? magnitude : -magnitude;
    }
    return n * (legendreP(n - 1, x) - x * legendreP(n, x)) / denom;
}

double laguerreL(int n, double x) {
    if (n < 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 0) return 1.0;
    if (n == 1) return 1.0 - x;
    double l0 = 1.0, l1 = 1.0 - x;
    for (int k = 2; k <= n; ++k) {
        const double l2 = ((2.0 * k - 1.0 - x) * l1 - (k - 1.0) * l0) / (double)k;
        l0 = l1;
        l1 = l2;
    }
    return l1;
}

double chebyshevT(int n, double x) {
    if (n < 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 0) return 1.0;
    if (n == 1) return x;
    double t0 = 1.0, t1 = x;
    for (int k = 2; k <= n; ++k) {
        const double t2 = 2.0 * x * t1 - t0;
        t0 = t1;
        t1 = t2;
    }
    return t1;
}

void gaussLegendre(int n, std::vector<double>& nodes, std::vector<double>& weights) {
    nodes.assign(n, 0.0);
    weights.assign(n, 0.0);
    if (n < 1) return;
    for (int i = 0; i < n; ++i) {
        // Initial guess: the Chebyshev-like distribution of the roots.
        double x = std::cos(kPi * (i + 0.75) / (n + 0.5));
        for (int iter = 0; iter < 100; ++iter) {
            const double p = legendreP(n, x);
            const double dp = legendrePrime(n, x);
            if (std::fabs(dp) < 1e-300) break;
            const double dx = p / dp;
            x -= dx;
            if (std::fabs(dx) < 1e-15) break;
        }
        nodes[i] = x;
        const double dp = legendrePrime(n, x);
        weights[i] = 2.0 / ((1.0 - x * x) * dp * dp);
    }
    // Order the nodes so that quadrature loops run on a monotone grid.
    for (int i = 0; i < n / 2; ++i) {
        std::swap(nodes[i], nodes[n - 1 - i]);
        std::swap(weights[i], weights[n - 1 - i]);
    }
}

void gaussChebyshev(int n, std::vector<double>& nodes, std::vector<double>& weights) {
    nodes.assign(n, 0.0);
    weights.assign(n, kPi / n);
    for (int i = 0; i < n; ++i)
        nodes[i] = std::cos(kPi * (2 * i + 1) / (2 * n));
}

// Cyclic Jacobi eigenvalues of a symmetric matrix.
void gaussLaguerre(int n, std::vector<double>& nodes, std::vector<double>& weights) {
    nodes.assign(n, 0.0);
    weights.assign(n, 0.0);
    if (n < 1) return;
    // Bracket every root of L_n on [0, 200] and bisect.
    std::vector<double> roots;
    double x = 0.0;
    double fPrev = laguerreL(n, x);
    while ((int)roots.size() < n && x < 220.0) {
        // The roots cluster near the origin, so the scan is finer there.
        const double step = x < 20.0 ? 0.01 : 0.2;
        const double next = x + step;
        const double fNext = laguerreL(n, next);
        if (fPrev * fNext <= 0 && std::fabs(fPrev) > 1e-300) {
            double lo = x, hi = next, flo = fPrev;
            for (int k = 0; k < 200; ++k) {
                const double mid = 0.5 * (lo + hi);
                const double fm = laguerreL(n, mid);
                if (flo * fm <= 0) {
                    hi = mid;
                } else {
                    lo = mid;
                    flo = fm;
                }
                if (hi - lo < 1e-14 * std::max(1.0, hi)) break;
            }
            roots.push_back(0.5 * (lo + hi));
        }
        x = next;
        fPrev = fNext;
    }
    for (int i = 0; i < (int)roots.size() && i < n; ++i) {
        const double xi = roots[i];
        nodes[i] = xi;
        const double ln1 = laguerreL(n + 1, xi);
        weights[i] = xi / ((double)(n + 1) * (n + 1) * ln1 * ln1);
    }
}

void symmetricEigen(std::vector<std::vector<double>> A,
                 std::vector<double>& values,
                 std::vector<std::vector<double>>& vectors) {
    const int n = (int)A.size();
    vectors.assign(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) vectors[i][i] = 1.0;
    for (int sweep = 0; sweep < 60; ++sweep) {
        double off = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) off += A[i][j] * A[i][j];
        if (std::sqrt(off) < 1e-13) break;
        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                if (std::fabs(A[p][q]) < 1e-16) continue;
                const double theta = (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
                const double t = (theta >= 0 ? 1.0 : -1.0) /
                                 (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                for (int k = 0; k < n; ++k) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; ++k) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < n; ++k) {
                    const double vkp = vectors[k][p], vkq = vectors[k][q];
                    vectors[k][p] = c * vkp - s * vkq;
                    vectors[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    values.assign(n, 0.0);
    for (int i = 0; i < n; ++i) values[i] = A[i][i];
}

} // namespace ode
