/*
 * Accord - Tests for the daemon's test tone.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tests/micro_test.hpp"

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "daemon/src/tone.hpp"

namespace
{
using namespace accord::daemon;

// One full cycle, as the payload would have submitted it.
std::vector<std::int16_t> render_cycle()
{
    std::vector<std::int16_t> samples(static_cast<std::size_t>(kCycleFrames) * 2, 0);
    Tone tone;
    tone.render(samples, kCycleFrames);
    return samples;
}

bool sounds_between(const std::vector<std::int16_t> &samples, std::uint64_t from, std::uint64_t to)
{
    for (std::uint64_t frame = from; frame < to; ++frame)
    {
        if (samples[frame * 2] != 0 || samples[frame * 2 + 1] != 0)
            return true;
    }
    return false;
}

bool silent_between(const std::vector<std::int16_t> &samples, std::uint64_t from, std::uint64_t to)
{
    for (std::uint64_t frame = from; frame < to; ++frame)
    {
        if (samples[frame * 2] != 0 || samples[frame * 2 + 1] != 0)
            return false;
    }
    return true;
}
} // namespace

MICRO_TEST(the_cycle_sounds_in_three_beeps_and_rests_between_them)
{
    const std::vector<std::int16_t> samples = render_cycle();

    for (unsigned beep = 0; beep < kBeepsPerCycle; ++beep)
    {
        const std::uint64_t start = beep * kBeepPeriodFrames;
        MICRO_CHECK(sounds_between(samples, start, start + kBeepFrames));
        MICRO_CHECK(silent_between(samples, start + kBeepFrames, start + kBeepPeriodFrames));
    }

    // The rest of the two-second cycle is the pause that separates one set of
    // beeps from the next.
    MICRO_CHECK(silent_between(samples, kBeepsFrames, kCycleFrames));
}

MICRO_TEST(every_beep_starts_from_the_same_phase)
{
    const std::vector<std::int16_t> samples = render_cycle();

    for (unsigned beep = 1; beep < kBeepsPerCycle; ++beep)
    {
        const std::uint64_t start = beep * kBeepPeriodFrames;
        for (std::uint64_t frame = 0; frame < 1000; ++frame)
        {
            MICRO_CHECK_EQ(samples[frame * 2], samples[(start + frame) * 2]);
            MICRO_CHECK_EQ(samples[frame * 2 + 1], samples[(start + frame) * 2 + 1]);
        }
    }
}

MICRO_TEST(the_beeps_are_stereo_and_close_to_full_scale)
{
    const std::vector<std::int16_t> samples = render_cycle();

    std::int16_t peak = 0;
    for (std::size_t i = 0; i + 1 < samples.size(); i += 2)
    {
        MICRO_CHECK(std::abs(samples[i]) <= kBeepAmplitude);
        MICRO_CHECK_EQ(samples[i], samples[i + 1]);
        if (std::abs(samples[i]) > peak)
            peak = static_cast<std::int16_t>(std::abs(samples[i]));
    }

    // A generator that produced silence would pass every check above, and
    // silence is the failure this tone exists to rule out.
    MICRO_CHECK(peak >= kBeepAmplitude - 1000);
}

MICRO_TEST(tone_playing_at_follows_the_beep_pattern)
{
    MICRO_CHECK(tone_playing_at(0));
    MICRO_CHECK(tone_playing_at(kBeepFrames - 1));
    MICRO_CHECK(!tone_playing_at(kBeepFrames));
    MICRO_CHECK(!tone_playing_at(kBeepPeriodFrames - 1));
    MICRO_CHECK(tone_playing_at(kBeepPeriodFrames));

    // The last beep ends where its own gap begins, and the long pause of the
    // cycle follows that gap.
    MICRO_CHECK(tone_playing_at(kBeepsFrames - kBeepGapFrames - 1));
    MICRO_CHECK(!tone_playing_at(kBeepsFrames - kBeepGapFrames));
    MICRO_CHECK(!tone_playing_at(kBeepsFrames));
    MICRO_CHECK(!tone_playing_at(kCycleFrames - 1));

    // And the next cycle starts its first beep again.
    MICRO_CHECK(tone_playing_at(kCycleFrames));
    MICRO_CHECK(tone_playing_at(kCycleFrames + kBeepFrames - 1));
    MICRO_CHECK(!tone_playing_at(kCycleFrames + kBeepFrames));
}

MICRO_TEST(tone_counts_the_frames_it_renders)
{
    Tone tone;
    std::vector<std::int16_t> block(512, 0);

    tone.render(block, 256);
    MICRO_CHECK_EQ(tone.frames_written(), 256ull);
    MICRO_CHECK(tone.playing());

    tone.render(block, 256);
    MICRO_CHECK_EQ(tone.frames_written(), 512ull);
    MICRO_CHECK(tone.playing());
}
