/*
 * Accord - The audio port variants the console session compares.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The daemon plays the same tone on one port at a time, so the one question a
 * console run answers is which variant is audible. The grid is the two variables
 * the first two runs changed at once: the type sceAudioOutOpen asks for, and
 * whether sceAudioOutSetVolume is called afterwards. The kit never calls it --
 * that is the only difference between its audio path and this daemon's -- and
 * the research does not document its flags.
 *
 * Pure, so the host tests cover the table and the index handling the audio
 * thread relies on while the HTTP thread is free to be wrong about them.
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace accord::daemon
{
struct AudioVariant
{
    int type;        // the `type` argument of sceAudioOutOpen
    bool set_volume; // whether sceAudioOutSetVolume is called after the open
    std::string_view name;
};

// Three types, each with and without the volume call, in pairs: a variant and
// the one after it differ in the volume call alone.
inline constexpr std::size_t kAudioVariantCount = 6;

// Index 0 is the type and the volume call the kit's own port uses, so it is the
// variant the daemon starts on. Anything past the end of the grid answers with
// variant 0 rather than reading out of bounds.
const AudioVariant &audio_variant_at(std::size_t index) noexcept;

// Parses the `n` of POST /variant?n=2. `ok` is false for anything that is not a
// number or is past the end of the grid.
std::size_t audio_variant_from_text(std::string_view text, bool &ok) noexcept;

// Maps the --audio= name of the command line onto the variant that carries it:
// "main" and "voice" pick the type, and the volume call stays on.
std::size_t audio_variant_from_name(std::string_view name, bool &known) noexcept;
} // namespace accord::daemon
