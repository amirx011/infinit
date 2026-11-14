// Infinit Lab — transcendental functions and integration techniques
// (slots 96 and 97).
//
// These two slots are dedicated tools rather than article-only pages: the
// derivative, antiderivative and Taylor machinery is shared with the calculus
// engine, and the handlers add the identity table, the technique
// classification and the cross-checks.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "topics.hpp"

namespace ode {

Result transcendentalTopic(const TopicRequest& req);
Result integrationTechniquesTopic(const TopicRequest& req);

} // namespace ode
