// Infinit Lab — nonlinear planar systems and chaotic flows.
//
// Batch 3 of the reserved curriculum slots: 81, 82, 83, 84 and 112.
// The symbolic parts (equilibria, Jacobians, Lyapunov functions) are exact
// where possible; every simulation is labelled as numerical evidence and
// never as a proof.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result competingSpeciesTopic(const TopicRequest& req);
Result liapunovTopic(const TopicRequest& req);
Result limitCycleTopic(const TopicRequest& req);
Result chaosAttractorTopic(const TopicRequest& req);
Result localLinearizationTopic(const TopicRequest& req);

} // namespace ode
