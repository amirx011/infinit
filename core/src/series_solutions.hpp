// Infinit Lab — series solutions and special functions.
//
// Batch 1 of the reserved curriculum slots: power series, ordinary/singular
// points, Frobenius, Cauchy–Euler, Legendre and Bessel.  Every handler is a
// pure function of a TopicRequest; the dispatcher in topics.cpp only forwards.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result seriesPowerTopic(const TopicRequest& req);
Result seriesSingularPointsTopic(const TopicRequest& req);
Result seriesFrobeniusTopic(const TopicRequest& req);
Result eulerEquationTopic(const TopicRequest& req);
Result legendreTopic(const TopicRequest& req);
Result besselTopic(const TopicRequest& req);

} // namespace ode
