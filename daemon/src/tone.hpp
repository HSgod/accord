/*
 * Accord - Test tone for the background daemon payload.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Three short beeps at 1000 Hz, then a pause: 150 ms of tone, 150 ms of silence,
 * three times, and the rest of the two-second cycle silent. The first version of
 * this tone was one second of 440 Hz at about -12 dBFS alternating with one
 * second of silence, which the console played into a port that reported itself
 * open and consumed every block while nothing could be heard. A signal nobody
 * can miss is worth more than a pleasant one while that question is open, so the
 * amplitude is close to full scale and the beeps are short enough to stand out.
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
inline constexpr double kBeepFrequencyHz = 1000.0;
inline constexpr std::int16_t kBeepAmplitude = 30000;                    // about -0.8 dBFS
inline constexpr std::uint64_t kBeepFrames = kSampleRate / 1000 * 150;   // 150 ms of tone
inline constexpr std::uint64_t kBeepGapFrames = kSampleRate / 1000 * 150; // 150 ms of silence
inline constexpr unsigned kBeepsPerCycle = 3;
inline constexpr std::uint64_t kBeepPeriodFrames = kBeepFrames + kBeepGapFrames;
inline constexpr std::uint64_t kBeepsFrames = kBeepsPerCycle * kBeepPeriodFrames;
inline constexpr std::uint64_t kCycleFrames = 2 * kSampleRate;

// Whether the tone sounds at `frames` of the two-second cycle. Free function, so
// a status read never has to touch the generator the audio path owns.
bool tone_playing_at(std::uint64_t frames) noexcept;

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
