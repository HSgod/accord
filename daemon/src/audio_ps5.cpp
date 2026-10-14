/*
 * Accord - SceAudioOut port for the test tone.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "daemon/src/audio_ps5.hpp"

namespace accord::daemon
{
AudioOut::~AudioOut()
{
    close();
}

bool AudioOut::open_variant(const AudioVariant &variant) noexcept
{
    close();

    const int init = sceAudioOutInit();
    if (init != 0 && init != kAlreadyInitialized)
    {
        open_code_.store(init, std::memory_order_relaxed);
        volume_flags_.store(kNoVolumeCall, std::memory_order_relaxed);
        volume_code_.store(0, std::memory_order_relaxed);
        error_.remember(AudioCall::init, init);
        return false;
    }

    handle_ = sceAudioOutOpen(variant.user, variant.type, kPortIndex, kGrainFrames, kRate,
                              kFormatS16Stereo);
    open_code_.store(handle_, std::memory_order_relaxed);
    if (handle_ < 0)
    {
        volume_flags_.store(kNoVolumeCall, std::memory_order_relaxed);
        volume_code_.store(0, std::memory_order_relaxed);
        error_.remember(AudioCall::open, handle_);
        handle_ = -1;
        return false;
    }

    // The port is open, so whatever an earlier variant failed with is history:
    // /status describes this port from here on, and a stale code would report a
    // daemon that is working as one that is broken.
    error_.clear();

    volume_flags_.store(variant.volume_flags, std::memory_order_relaxed);
    volume_code_.store(0, std::memory_order_relaxed);
    if (variant.volume_flags != kNoVolumeCall)
    {
        const int volumes[2] = {kVolumeFull, kVolumeFull};
        const int code = sceAudioOutSetVolume(handle_, variant.volume_flags, volumes);
        volume_code_.store(code, std::memory_order_relaxed);
        if (code != 0)
        {
            // Not fatal: the port stays open at whatever volume it came up with.
            error_.remember(AudioCall::set_volume, code);
        }
    }
    return true;
}

int AudioOut::open_code() const noexcept
{
    return open_code_.load(std::memory_order_relaxed);
}

int AudioOut::volume_code() const noexcept
{
    return volume_code_.load(std::memory_order_relaxed);
}

int AudioOut::volume_flags() const noexcept
{
    return volume_flags_.load(std::memory_order_relaxed);
}

int AudioOut::submit(std::span<const std::int16_t> frames) noexcept
{
    if (handle_ < 0)
        return 0;

    const int code = sceAudioOutOutput(handle_, frames.data());
    if (code < 0)
        error_.remember(AudioCall::output, code);
    return code;
}

void AudioOut::drain() noexcept
{
    if (handle_ < 0)
        return;
    sceAudioOutOutput(handle_, nullptr);
}

void AudioOut::close() noexcept
{
    if (handle_ < 0)
        return;

    drain();
    sceAudioOutClose(handle_);
    handle_ = -1;
}

bool AudioOut::is_open() const noexcept
{
    return handle_ >= 0;
}

std::string AudioOut::last_error() const
{
    return error_.text();
}
} // namespace accord::daemon
