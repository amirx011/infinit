// Infinit Lab — elementary linear algebra.
//
// Entries are exact expressions: rational numbers or symbols.  Every routine
// works over the field of rational functions, so a symbolic matrix gets a
// generic answer (with an explicit assumption note) instead of a numeric
// guess.  Numeric approximations are reported next to exact results, never
// instead of them.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

// Rank, row/column/null spaces and the dimension theorem.
Result linalgRankTopic(const TopicRequest& req);
// Determinant by row reduction, verified against cofactor expansion.
Result linalgDetTopic(const TopicRequest& req);
// Inverse by Gauss–Jordan, verified by A·A^{-1} = I.
Result linalgInverseTopic(const TopicRequest& req);
// PA = LU with partial pivoting, verified by reconstructing A.
Result linalgLuTopic(const TopicRequest& req);
// A x = b for a rectangular symbolic system, by Gauss or Cramer.
Result linalgSolveTopic(const TopicRequest& req);
// Span membership, with a basis and the dimension of the span.
Result linalgSpanTopic(const TopicRequest& req);
// Linear independence, with the dependence relation when the set is dependent.
Result linalgIndependenceTopic(const TopicRequest& req);
// A basis extracted from a spanning set, plus the dimension.
Result linalgBasisTopic(const TopicRequest& req);
// Coordinates of a vector in an ordered basis.
Result linalgCoordinatesTopic(const TopicRequest& req);
// Transition matrix between two bases, verified by C P = B.
Result linalgChangeBasisTopic(const TopicRequest& req);
// The three subspace axioms for a span, plus its basis and dimension.
Result linalgSubspaceTopic(const TopicRequest& req);
// Orthogonal projection onto a span, with the residual and its norm.
Result linalgProjectionTopic(const TopicRequest& req);
// Gram-Schmidt: an orthogonal and an orthonormal basis.
Result linalgGramSchmidtTopic(const TopicRequest& req);
// A = Q R with orthonormal Q and upper-triangular R.
Result linalgQrTopic(const TopicRequest& req);
// Least squares by the normal equations, with the residual certificate.
Result linalgLeastSquaresTopic(const TopicRequest& req);
// A = P D P^{-1} over the rationals.
Result linalgDiagonalizeTopic(const TopicRequest& req);
// A^k by repeated multiplication, cross-checked with P D^k P^{-1}.
Result linalgPowerTopic(const TopicRequest& req);

// The orthogonal complement of the column space, from the left null space.
Result linalgOrthogonalComplementTopic(const TopicRequest& req);
// Incidence matrix of a graph: components, potentials and cycles.
Result linalgGraphTopic(const TopicRequest& req);
// The DFT/FFT of a length-4 or length-8 signal.
Result linalgFftTopic(const TopicRequest& req);
// B = P^{-1} A P, with the trace and determinant invariants checked.
Result linalgSimilarityTopic(const TopicRequest& req);
// e^{At} by diagonalization, with the closed 2x2 formula as a fallback.
Result linalgMatrixExpTopic(const TopicRequest& req);
// Sylvester's criterion and the Cholesky factor A = L L^T.
Result linalgPositiveDefiniteTopic(const TopicRequest& req);
// The SVD A = U S V^T and the Moore-Penrose pseudoinverse.
Result linalgSvdTopic(const TopicRequest& req);
// 1, infinity, Frobenius and spectral norms, plus the condition number.
Result linalgNormTopic(const TopicRequest& req);
// The dominant eigenpair by power iteration.
Result linalgPowerIterationTopic(const TopicRequest& req);
// Eigenvalues of a symmetric matrix by the unshifted QR algorithm.
Result linalgQrAlgorithmTopic(const TopicRequest& req);
// Jacobi and Gauss-Seidel iterations for A x = b.
Result linalgIterativeTopic(const TopicRequest& req);
// A small linear program by the simplex method.
Result linalgLpTopic(const TopicRequest& req);
// A 2x2 zero-sum game: saddle point or mixed strategies.
Result linalgGameTopic(const TopicRequest& req);
// The spectral theorem A = Q D Q^T for a symmetric matrix.
Result linalgSymmetricTopic(const TopicRequest& req);

} // namespace ode
