// Infinit Lab — Fourier convergence, even/odd splits and the heat/wave
// derivations (slots 86, 87, 88, 89, 113).
//
// The existing `fourier_series`, `heat` and `wave` handlers in topics.cpp keep
// their article-level output.  These handlers add the convergence evidence,
// the even/odd decomposition and the general boundary-condition switch, all
// evaluated numerically from the same coefficients.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result fourierConvergenceTopic(const TopicRequest& req);
Result fourierEvenOddTopic(const TopicRequest& req);
Result heatDerivationTopic(const TopicRequest& req);
Result waveDerivationTopic(const TopicRequest& req);
Result heatOtherBcTopic(const TopicRequest& req);

} // namespace ode
