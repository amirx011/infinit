// Infinit Lab — shared special functions and quadrature.
//
// These helpers are numeric building blocks for the special-function tools
// (Bessel, Legendre, Laguerre, orthogonal series).  They are deliberately
// header-declared so that several engine modules can share one implementation
// instead of copying series code around.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>

namespace ode {

// Bessel functions of the first and second kind.  `terms` bounds the series
// used for J_nu; Y_nu reuses the same series.  Invalid arguments return NaN.
double besselJ(double nu, double x, int terms = 60);
double besselY(double nu, double x, int terms = 60);

// Derivative J_nu'(x) = (J_{nu-1}(x) - J_{nu+1}(x)) / 2.
double besselJPrime(double nu, double x);

// The n-th positive zero (n >= 1) of J_nu, found by bracketing plus bisection.
// Returns NaN when the zero cannot be isolated.
double besselZero(int n, double nu);

// Legendre polynomial P_n and its derivative.
double legendreP(int n, double x);
double legendrePrime(int n, double x);

// Laguerre polynomial L_n (weight e^{-x} on (0, inf)).
double laguerreL(int n, double x);

// Chebyshev polynomial of the first kind T_n.
double chebyshevT(int n, double x);

// Gauss-Legendre nodes and weights on [-1, 1] (n >= 1).
void gaussLegendre(int n, std::vector<double>& nodes, std::vector<double>& weights);

// Gauss-Chebyshev nodes and weights for the weight 1/sqrt(1-x^2) on [-1, 1].
void gaussChebyshev(int n, std::vector<double>& nodes, std::vector<double>& weights);

// Gauss-Laguerre nodes and weights for the weight e^{-x} on [0, inf).
void gaussLaguerre(int n, std::vector<double>& nodes, std::vector<double>& weights);

// Cyclic Jacobi diagonalisation of a symmetric matrix.  On return `values`
// holds the eigenvalues and the columns of `vectors` the eigenvectors.
void symmetricEigen(std::vector<std::vector<double>> A,
                    std::vector<double>& values,
                    std::vector<std::vector<double>>& vectors);

} // namespace ode
