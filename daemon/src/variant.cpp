/*
 * Accord - The audio port variants the console session compares.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "daemon/src/variant.hpp"

namespace accord::daemon
{
namespace
{
// The names travel into the log, into /status and into the report, so they stay
// short and free of quotes: no JSON escaping is needed for them.
constexpr AudioVariant kVariants[kAudioVariantCount] = {
    {0, true, "type 0 main, volume"},
    {0, false, "type 0 main, no volume"},
    {1, true, "type 1, volume"},
    {1, false, "type 1, no volume"},
    {2, true, "type 2 chat, volume"},
    {2, false, "type 2 chat, no volume"},
};

constexpr std::size_t kFirstWithMainType = 0;
constexpr std::size_t kFirstWithChatType = 4;
constexpr std::size_t kMaxDigits = 2;
} // namespace

const AudioVariant &audio_variant_at(std::size_t index) noexcept
{
    return kVariants[index < kAudioVariantCount ? index : 0];
}

std::size_t audio_variant_from_text(std::string_view text, bool &ok) noexcept
{
    ok = false;
    if (text.empty() || text.size() > kMaxDigits)
        return 0;

    std::size_t value = 0;
    for (const char digit : text)
    {
        if (digit < '0' || digit > '9')
            return 0;
        value = value * 10 + static_cast<std::size_t>(digit - '0');
    }

    if (value >= kAudioVariantCount)
        return 0;

    ok = true;
    return value;
}

std::size_t audio_variant_from_name(std::string_view name, bool &known) noexcept
{
    known = true;
    if (name == "main")
        return kFirstWithMainType;
    if (name == "voice")
        return kFirstWithChatType;

    known = false;
    return 0;
}
} // namespace accord::daemon
