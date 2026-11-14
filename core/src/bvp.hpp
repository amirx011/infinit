// Infinit Lab — boundary value problems and Sturm–Liouville problems.
//
// Batch 3 of the reserved curriculum slots: 85, 90 and 91.  The
// finite-difference solvers are numerical and say so; the classification
// tool (90) never invents a numeric answer.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result bvpTwoPointTopic(const TopicRequest& req);
Result bvpOccurrenceTopic(const TopicRequest& req);
Result sturmLiouvilleTopic(const TopicRequest& req);
Result bvpNonhomogeneousTopic(const TopicRequest& req);

} // namespace ode
