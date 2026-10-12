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
//
// 0 is the configuration the console accepted: the system user, the type the kit
// opens, and the volume flags that came back 0 rather than 0x80260013. The rest
// are the controls it is compared against, each one the failure it has already
// been seen to be: the kit's path without the volume call, the rejected flags,
// the other type that opens, and the user id 0x10000000 that the console refuses
// whatever else is asked for.
constexpr AudioVariant kVariants[kAudioVariantCount] = {
    {kUserSystem, 0, 3, "system, type 0, volume flags 3"},
    {kUserSystem, 0, kNoVolumeCall, "system, type 0, no volume"},
    {kUserSystem, 0, 0, "system, type 0, volume flags 0"},
    {kUserSystem, 1, 3, "system, type 1, volume flags 3"},
    {kUserAccount, 0, 3, "account, type 0, volume flags 3"},
    {kUserAccount, 0, kNoVolumeCall, "account, type 0, no volume"},
};

constexpr std::size_t kFirstWithMainType = 0;
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

    // The console refused the chat type with 0x80260011 on every user id it was
    // tried with, so no variant of this grid carries it and --audio=voice has
    // nothing to select. The name stays one the command line may pass; the
    // daemon reports it instead of obeying it.
    known = false;
    return kFirstWithMainType;
}
} // namespace accord::daemon
