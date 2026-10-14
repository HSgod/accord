/*
 * Accord - Tests for the tone the title plays in the T2.3b control run.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the console has to answer is whether it can hear this tone, so what is
 * checked here is only that the title posts it on a schedule a listener can
 * recognise: sound as soon as the shell is up, then one sweep per period, never
 * two at once and never a burst owed after a slow frame.
 */

#include "tests/micro_test.hpp"

#include "ui/title_tone.hpp"

using namespace accord::ui;

namespace
{
// What the title hands in on its first frame, so that the first frame sounds.
constexpr double kNothingPostedYet = -kTitleTonePeriodSeconds;
} // namespace

MICRO_TEST(the_title_sounds_on_its_first_frame)
{
    MICRO_CHECK(title_tone_due(0.0, kNothingPostedYet));
    // Even if the shell took a while to get there.
    MICRO_CHECK(title_tone_due(3.5, kNothingPostedYet));
}

MICRO_TEST(a_sweep_is_not_posted_again_before_the_period_is_over)
{
    constexpr double kPostedAt = 10.0;

    MICRO_CHECK(!title_tone_due(kPostedAt, kPostedAt));
    MICRO_CHECK(!title_tone_due(kPostedAt + kTitleToneSweepSeconds, kPostedAt));
    MICRO_CHECK(!title_tone_due(kPostedAt + kTitleTonePeriodSeconds - 0.001, kPostedAt));

    // And it is due exactly on the period, not a frame later.
    MICRO_CHECK(title_tone_due(kPostedAt + kTitleTonePeriodSeconds, kPostedAt));
}

MICRO_TEST(a_late_frame_does_not_owe_a_burst_of_sweeps)
{
    // A frame that arrived a whole second late, and one that arrived ten
    // seconds late, both post exactly one sweep: the mark moves to now.
    constexpr double kPostedAt = 2.0;

    const double late = kPostedAt + 10.0;
    MICRO_CHECK(title_tone_due(late, kPostedAt));
    MICRO_CHECK(!title_tone_due(late, late));
}

MICRO_TEST(the_schedule_is_the_siren_the_daemon_plays)
{
    // Comparable by ear with the daemon's tone: a rising sweep of about a
    // second, a pause short enough to hear as a rhythm, and not full scale.
    MICRO_CHECK_EQ(kTitleToneStartHz, 400.0f);
    MICRO_CHECK_EQ(kTitleToneEndHz, 2000.0f);
    MICRO_CHECK(kTitleToneSweepSeconds >= 0.5);
    MICRO_CHECK(kTitleToneSweepSeconds <= 2.0);
    MICRO_CHECK(kTitleTonePauseSeconds > 0.0);
    MICRO_CHECK(kTitleTonePauseSeconds < kTitleToneSweepSeconds);
    MICRO_CHECK(kTitleToneGain > 0.0f);
    MICRO_CHECK(kTitleToneGain <= 1.0f);
}
