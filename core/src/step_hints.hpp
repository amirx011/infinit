#pragma once

#include <string>

namespace ode {

// Returns the hint key for a step title, or nullptr when the title has no
// generic teaching note.
const char* stepHintKey(const std::string& titleKey);

} // namespace ode
