/*
 * Accord - Test tone for the background daemon payload.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "daemon/src/tone.hpp"

#include <cmath>

namespace accord::daemon
{
namespace
{
constexpr double kTwoPi = 6.283185307179586476925286766559;

// Where inside the two-second cycle this frame sits, and whether that position
// is inside one of the three beeps.
std::uint64_t cycle_position(std::uint64_t frames) noexcept
{
    return frames % kCycleFrames;
}

bool position_is_a_beep(std::uint64_t position) noexcept
{
    return position < kBeepsFrames && (position % kBeepPeriodFrames) < kBeepFrames;
}
} // namespace

bool tone_playing_at(std::uint64_t frames) noexcept
{
    return position_is_a_beep(cycle_position(frames));
}

void Tone::render(std::span<std::int16_t> out, std::size_t frames) noexcept
{
    const double step = kTwoPi * kBeepFrequencyHz / static_cast<double>(kSampleRate);

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const std::uint64_t position = cycle_position(frames_written_);

        // Every beep starts from the same phase, so the seam between a beep and
        // the silence after it does not click.
        if (position % kBeepPeriodFrames == 0)
            phase_ = 0.0;

        std::int16_t value = 0;
        if (position_is_a_beep(position))
        {
            value = static_cast<std::int16_t>(std::lround(std::sin(phase_) * kBeepAmplitude));
            phase_ += step;
            if (phase_ >= kTwoPi)
                phase_ -= kTwoPi;
        }

        out[frame * 2] = value;
        out[frame * 2 + 1] = value;
        ++frames_written_;
    }
}

std::uint64_t Tone::frames_written() const noexcept
{
    return frames_written_;
}

bool Tone::playing() const noexcept
{
    return tone_playing_at(frames_written_);
}
} // namespace accord::daemon
