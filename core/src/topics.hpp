// Infinit Lab — topic workbench.
//
// The curriculum pages (applications, numerical methods, transforms, systems
// and PDEs) share one request shape: a topic id plus named numeric and string
// parameters.  Keeping the dispatcher here means the JSON API stays a thin
// adapter and the Flutter side can use a single generic form.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "solver.hpp"

#include <map>
#include <string>

namespace ode {

struct TopicRequest {
    std::string topic;
    std::map<std::string, double> nums;
    std::map<std::string, std::string> strs;
    std::string lang = "en";

    double number(const std::string& key, double fallback) const {
        auto it = nums.find(key);
        return it == nums.end() ? fallback : it->second;
    }
    std::string text(const std::string& key, const std::string& fallback = "") const {
        auto it = strs.find(key);
        return it == strs.end() ? fallback : it->second;
    }
};

Result solveTopic(const TopicRequest& req);

} // namespace ode
