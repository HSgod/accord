/*
 * Accord - SceAudioOut port for the test tone.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Only this file knows about libSceAudioOut, so the host build and the tests
 * stay free of the SDK. The prototypes are written out here because the payload
 * SDK ships the SCE libraries as stubs without public headers.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <span>
#include <string>

#include "daemon/src/audio_error.hpp"
#include "daemon/src/variant.hpp"

// libSceAudioOut, as the SDK's stub library exports it.
extern "C" int sceAudioOutInit();
extern "C" int sceAudioOutOpen(unsigned user_id, int type, int index, unsigned grain,
                               unsigned frequency, unsigned format);
extern "C" int sceAudioOutSetVolume(int handle, int flags, const int *volumes);
extern "C" int sceAudioOutOutput(int handle, const void *samples);
extern "C" int sceAudioOutClose(int handle);

namespace accord::daemon
{
// The payload submits one complete block and blocks until the queue has room,
// so the block size is a property of this module.
inline constexpr unsigned kGrainFrames = 256;

// The values the kit's own port opens with on this console and firmware: user
// 0xff, index 0, grain 256, 48 kHz, s16 stereo. The daemon's first console run
// asked with user 0x10000000 and the chat type and came back 0x80260011; with
// these it opened. The type and the volume call are the sweep's two variables,
// so they live in the variant.
inline constexpr unsigned kSystemUser = 0xffu;
inline constexpr int kPortIndex = 0;
inline constexpr unsigned kRate = 48000;
inline constexpr unsigned kFormatS16Stereo = 1;

// The volumes the research documents: 0x8000 is 0 dB. The meaning of the flags
// word is not in it, so 0 stays as the value the first console run used.
inline constexpr int kVolumeFlagAll = 0;
inline constexpr int kVolumeFull = 0x8000;

// sceAudioOutInit answers this when the library is already up, which the kit
// treats as success. A payload that starts after any other audio user must not
// read it as a failure of its own.
inline constexpr int kAlreadyInitialized = static_cast<int>(0x8026000eu);

class AudioOut
{
  public:
    AudioOut() = default;
    AudioOut(const AudioOut &) = delete;
    AudioOut &operator=(const AudioOut &) = delete;
    ~AudioOut();

    // Closes whatever is open and opens the port the variant names. The volume
    // call is skipped when the variant says so. The codes the console answered
    // with are kept here for the log: the failure record below keeps only the
    // last one, and the audio thread overwrites it with every block.
    bool open_variant(const AudioVariant &variant) noexcept;

    // The handle sceAudioOutOpen returned, the code of the volume call, and
    // whether that call was made at all. The audio thread writes them once per
    // open, the HTTP thread reads them for /status.
    int open_code() const noexcept;
    int volume_code() const noexcept;
    bool volume_called() const noexcept;

    // Returns the code sceAudioOutOutput answered with. Only a negative one is a
    // failure: on this firmware the call answers 256, the grain it has just
    // taken, and the kit's own port checks for < 0 for the same reason.
    int submit(std::span<const std::int16_t> frames) noexcept;

    void drain() noexcept;
    void close() noexcept;

    bool is_open() const noexcept;

    // Composed from the failure record on every call, because the audio thread
    // is the one that writes it and the HTTP thread is the one that reads it.
    // Empty while nothing has failed. A failed volume call shows up here only
    // until the next failing block, which is why the log lines carry the codes.
    std::string last_error() const;

  private:
    int handle_ = -1;
    std::atomic<int> open_code_{0};
    std::atomic<int> volume_code_{0};
    std::atomic<bool> volume_called_{false};
    AudioError error_;
};
} // namespace accord::daemon
