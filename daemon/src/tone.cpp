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
} // namespace

double siren_frequency_at(std::uint64_t position) noexcept
{
    const std::uint64_t sweep = position % kSweepFrames;
    const double span = kSweepHighHz - kSweepLowHz;
    const double half = static_cast<double>(kSweepHalfFrames);

    if (sweep < kSweepHalfFrames)
        return kSweepLowHz + span * (static_cast<double>(sweep) / half);

    return kSweepHighHz - span * (static_cast<double>(sweep - kSweepHalfFrames) / half);
}

bool tone_playing_at(std::uint64_t frames) noexcept
{
    return frames % kCycleFrames < kSirenFrames;
}

void Tone::render(std::span<std::int16_t> out, std::size_t frames) noexcept
{
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const std::uint64_t position = frames_written_ % kCycleFrames;

        std::int16_t value = 0;
        if (position < kSirenFrames)
        {
            // The phase carries across the whole eight seconds, so the sweep is
            // one continuous sound rather than a run of separate tones.
            value = static_cast<std::int16_t>(std::lround(std::sin(phase_) * kSirenAmplitude));
            phase_ += kTwoPi * siren_frequency_at(position) / static_cast<double>(kSampleRate);
            if (phase_ >= kTwoPi)
                phase_ -= kTwoPi;
        }
        else if (position == kSirenFrames)
        {
            // The pause starts from a known phase, so the next siren does not
            // begin where the last one happened to stop.
            phase_ = 0.0;
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
