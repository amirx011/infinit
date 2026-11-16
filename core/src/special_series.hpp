// Infinit Lab — singular Sturm–Liouville problems and orthogonal series.
//
// Batch 4 of the reserved curriculum slots: 93, 94 and 95.  Everything is
// evaluated numerically from special functions and Gauss quadrature, and the
// result says so.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result sturmLiouvilleSingularTopic(const TopicRequest& req);
Result besselSeriesTopic(const TopicRequest& req);
Result orthogonalSeriesTopic(const TopicRequest& req);

} // namespace ode
