// Infinit Lab — linear systems of ODEs.
//
// Batch 2 of the reserved curriculum slots: 75, 76, 77, 78, 79 and 80.
// The handlers are self-contained; they only borrow the public AST,
// polynomial and numeric helpers.  topics.cpp stays a dispatcher.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result systemTheoryTopic(const TopicRequest& req);
Result systemComplexEigenTopic(const TopicRequest& req);
Result systemFundamentalTopic(const TopicRequest& req);
Result systemRepeatedTopic(const TopicRequest& req);
Result systemNonhomogeneousTopic(const TopicRequest& req);
Result systemRk4Topic(const TopicRequest& req);

} // namespace ode
