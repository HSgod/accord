/*
 * Accord - Test tone for the background daemon payload.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Eight seconds of a siren -- a 400 Hz to 2000 Hz sweep and back, one second
 * each way -- then two seconds of silence, on repeat. Two earlier signals left
 * room for doubt: one second of 440 Hz at about -12 dBFS, and three 150 ms beeps
 * of 1000 Hz at about -0.8 dBFS, both of which played into a port that reported
 * itself open and consumed every block. A sound nothing else on the console
 * makes is what removes the doubt, and a sweep is what a game's own audio cannot
 * be mistaken for.
 *
 * Pure arithmetic, so the host tests exercise exactly what the console plays.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace accord::daemon
{
// Interleaved signed 16-bit stereo: one frame is two samples.
inline constexpr std::uint32_t kSampleRate = 48000;
inline constexpr double kSweepLowHz = 400.0;
inline constexpr double kSweepHighHz = 2000.0;
inline constexpr std::int16_t kSirenAmplitude = 30000;              // about -0.8 dBFS
inline constexpr std::uint64_t kSweepHalfFrames = kSampleRate;      // one second each way
inline constexpr std::uint64_t kSweepFrames = 2 * kSweepHalfFrames; // up, then down
inline constexpr std::uint64_t kSirenFrames = 8 * kSampleRate;      // eight seconds of it
inline constexpr std::uint64_t kSirenPauseFrames = 2 * kSampleRate; // then two seconds off
inline constexpr std::uint64_t kCycleFrames = kSirenFrames + kSirenPauseFrames;

// Whether the siren sounds at `frames` of the ten-second cycle. Free function, so
// a status read never has to touch the generator the audio path owns.
bool tone_playing_at(std::uint64_t frames) noexcept;

// The frequency the sweep is at, `position` frames into the siren. Exposed so the
// tests can check the sweep by its frequency rather than by counting samples.
double siren_frequency_at(std::uint64_t position) noexcept;

class Tone
{
  public:
    // Writes `frames` interleaved stereo frames into `out`.
    void render(std::span<std::int16_t> out, std::size_t frames) noexcept;

    std::uint64_t frames_written() const noexcept;
    bool playing() const noexcept;

  private:
    std::uint64_t frames_written_ = 0;
    double phase_ = 0.0;
};
} // namespace accord::daemon
