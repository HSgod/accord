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

// There and back, which is the whole siren before its pause.
std::vector<std::int16_t> render_sweep()
{
    std::vector<std::int16_t> samples(static_cast<std::size_t>(kSirenFrames) * 2, 0);
    Tone tone;
    tone.render(samples, kSirenFrames);
    return samples;
}

std::size_t zero_crossings(const std::vector<std::int16_t> &samples, std::uint64_t from,
                           std::uint64_t to)
{
    std::size_t crossings = 0;
    for (std::uint64_t frame = from + 1; frame < to; ++frame)
    {
        const std::int16_t previous = samples[(frame - 1) * 2];
        const std::int16_t current = samples[frame * 2];
        if ((previous < 0 && current >= 0) || (previous >= 0 && current < 0))
            ++crossings;
    }
    return crossings;
}
} // namespace

MICRO_TEST(the_cycle_sounds_for_eight_seconds_and_rests_for_two)
{
    MICRO_CHECK(tone_playing_at(0));
    MICRO_CHECK(tone_playing_at(kSirenFrames - 1));
    MICRO_CHECK(!tone_playing_at(kSirenFrames));
    MICRO_CHECK(!tone_playing_at(kCycleFrames - 1));

    // And the next cycle starts its siren again.
    MICRO_CHECK(tone_playing_at(kCycleFrames));
    MICRO_CHECK(!tone_playing_at(2 * kCycleFrames - 1));
}

MICRO_TEST(the_sweep_goes_from_the_low_tone_up_and_back)
{
    MICRO_CHECK_EQ(static_cast<int>(siren_frequency_at(0)), static_cast<int>(kSweepLowHz));
    MICRO_CHECK_EQ(static_cast<int>(siren_frequency_at(kSweepHalfFrames)),
                   static_cast<int>(kSweepHighHz));
    MICRO_CHECK_EQ(static_cast<int>(siren_frequency_at(kSweepFrames)),
                   static_cast<int>(kSweepLowHz));

    // Rising through the first second, falling through the second.
    for (std::uint64_t frame = 1; frame < kSweepHalfFrames; ++frame)
        MICRO_CHECK(siren_frequency_at(frame) > siren_frequency_at(frame - 1));

    for (std::uint64_t frame = kSweepHalfFrames + 1; frame < kSweepFrames; ++frame)
        MICRO_CHECK(siren_frequency_at(frame) < siren_frequency_at(frame - 1));
}

MICRO_TEST(the_rendered_sweep_really_changes_frequency)
{
    const std::vector<std::int16_t> samples = render_sweep();

    // One tenth of a second at the bottom of the sweep against one tenth at the
    // top: the same span of samples has to hold far more waves up there. This
    // checks the samples, not the helper the sweep is built from.
    const std::uint64_t window = kSampleRate / 10;
    const std::size_t low = zero_crossings(samples, 0, window);
    const std::size_t high = zero_crossings(samples, kSweepHalfFrames - window, kSweepHalfFrames);

    MICRO_CHECK(low > 0);
    MICRO_CHECK(high > low * 3);
}

MICRO_TEST(the_siren_is_stereo_loud_and_free_of_clicks)
{
    const std::vector<std::int16_t> samples = render_sweep();

    std::int16_t peak = 0;
    int biggest_step = 0;
    for (std::size_t i = 0; i + 3 < samples.size(); i += 2)
    {
        MICRO_CHECK(std::abs(samples[i]) <= kSirenAmplitude);
        MICRO_CHECK_EQ(samples[i], samples[i + 1]);

        const int step = std::abs(static_cast<int>(samples[i + 2]) - static_cast<int>(samples[i]));
        if (step > biggest_step)
            biggest_step = step;
        if (std::abs(samples[i]) > peak)
            peak = static_cast<std::int16_t>(std::abs(samples[i]));
    }

    // A generator that produced silence would pass everything above, and silence
    // is the failure this tone exists to rule out.
    MICRO_CHECK(peak >= kSirenAmplitude - 1000);

    // At the top of the sweep one sample step is about a quarter of the
    // amplitude; a phase that jumped would show up as a much larger step.
    MICRO_CHECK(biggest_step < kSirenAmplitude / 2);
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
