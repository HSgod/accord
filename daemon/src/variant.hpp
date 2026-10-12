/*
 * Accord - The audio port variants the console session compares.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The daemon plays the same tone on one port at a time, so the one question a
 * console run answers is which variant is audible. The grid carries the three
 * things the first two runs changed or guessed: the user the port opens for, the
 * type sceAudioOutOpen asks for, and the flags of the sceAudioOutSetVolume call
 * that follows it. The kit never makes that call -- which is the only difference
 * between its audio path and this daemon's -- and it asks with 0xff, the system
 * user, where the first run asked with 0x10000000.
 *
 * Pure, so the host tests cover the table and the index handling the audio
 * thread relies on while the HTTP thread is free to be wrong about them.
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace accord::daemon
{
// The two user ids the grid compares. SceUser.System is what the kit's own port
// opens with and what this console accepted; 0x10000000 is the first real user
// id, which the daemon's first run asked with. Whether the console routes a
// payload's audio to one of them is one of the questions the grid is for.
inline constexpr unsigned kUserSystem = 0xffu;
inline constexpr unsigned kUserAccount = 0x10000000u;

// No volume call at all, which is what the kit does.
inline constexpr int kNoVolumeCall = -1;

struct AudioVariant
{
    unsigned user;        // the `user_id` argument of sceAudioOutOpen
    int type;             // its `type` argument
    int volume_flags;     // sceAudioOutSetVolume's flags, or kNoVolumeCall
    std::string_view name;
};

// Six variants, and the endpoint stays POST /variant?n=0..5 whichever grid is
// compiled in.
inline constexpr std::size_t kAudioVariantCount = 6;

// Index 0 is the port the kit opens, so it is the variant the daemon starts on.
// Anything past the end of the grid answers with variant 0 rather than reading
// out of bounds.
const AudioVariant &audio_variant_at(std::size_t index) noexcept;

// Parses the `n` of POST /variant?n=2. `ok` is false for anything that is not a
// number or is past the end of the grid.
std::size_t audio_variant_from_text(std::string_view text, bool &ok) noexcept;

// Maps the --audio= name of the command line onto a variant: "main" and "voice"
// pick the type of the system-user port the kit uses.
std::size_t audio_variant_from_name(std::string_view name, bool &known) noexcept;
} // namespace accord::daemon
