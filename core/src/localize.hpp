// Infinit Lab — localisation helpers for engine error messages.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace ode {

// The parser and the algebra layer throw Persian messages so that the Persian
// UI never needs a second translation table.  When a request asks for English,
// the API boundary funnels those messages through this helper.  Unknown
// Persian text falls back to a generic English sentence instead of leaking the
// original language into an English response.
inline bool containsPersian(const std::string& text) {
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0xD8 && c <= 0xDB) return true; // Arabic/Persian block
    }
    return false;
}

inline std::string localizeError(const std::string& message, bool english) {
    if (!english || message.empty()) return message;

    struct Entry {
        const char* fa;
        const char* en;
    };
    static const Entry entries[] = {
        {"عبارت نامعتبر است", "invalid expression"},
        {"عبارت بسیار پیچیده است", "the expression is too deeply nested"},
        {"عبارت بسیار طولانی است", "the expression is too long"},
        {"عبارت اضافی در ورودی", "unexpected trailing input"},
        {"دو طرف معادله لازم است", "both sides of the equation are required"},
        {"sqrt به آرگومان نیاز دارد", "sqrt requires an argument"},
        {"نماد مشتق نامعتبر است", "invalid derivative symbol"},
        {"عدد بسیار بزرگ: ", "number too large: "},
        {"کران‌ها باید عددی باشند", "the bounds must be numeric"},
        {"شکل دیفرانسیلی معادله خوانده نشد", "could not read the differential form"},
        {"تقسیم بر صفر", "division by zero"},
        {"ورودی خالی است", "empty input"},
    };

    for (const auto& e : entries) {
        const std::string fa(e.fa);
        const size_t pos = message.find(fa);
        if (pos == std::string::npos) continue;
        std::string out = message;
        out.replace(pos, fa.size(), e.en);
        return out;
    }

    if (containsPersian(message)) return "the input could not be processed";
    return message;
}

} // namespace ode
