/*
 * Accord - The tone the title plays for the T2.3b control run.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T2.3 left one question open. The daemon's audio port opens, takes every block
 * at 48 kHz and cannot be heard, while a game and a homebrew title on the same
 * console, through the same output, can. The console-side control run plays the
 * same kind of sound from this title, which has an application id, and the only
 * thing this file decides is when the next sweep starts.
 *
 * Pure arithmetic, so the host tests cover the schedule and `make app
 * TITLE_TONE=1` is the only thing that has to build.
 */

#pragma once

namespace accord::ui
{
// The sweep the title plays: the shape of the daemon's siren, so the two are
// comparable by ear -- 400 Hz up to 2000 Hz over one second, a pause, again.
inline constexpr float kTitleToneStartHz = 400.0f;
inline constexpr float kTitleToneEndHz = 2000.0f;
inline constexpr double kTitleToneSweepSeconds = 1.0;
inline constexpr double kTitleTonePauseSeconds = 0.4;
inline constexpr double kTitleTonePeriodSeconds = kTitleToneSweepSeconds + kTitleTonePauseSeconds;
inline constexpr float kTitleToneAttackSeconds = 0.01f;
inline constexpr float kTitleToneReleaseSeconds = 0.05f;
// Not full scale: this one is heard next to a game, not instead of one.
inline constexpr float kTitleToneGain = 0.8f;

// True when the next sweep is due, which is once the period has passed since the
// last one was posted. The caller starts with `last_posted_seconds` below zero,
// so the first frame posts and the title sounds as soon as it is up. A frame
// that arrives late does not owe a burst of sweeps: it posts one and the caller
// moves the mark to now.
bool title_tone_due(double now_seconds, double last_posted_seconds) noexcept;
} // namespace accord::ui
