/*
 * Accord - SceAudioOut port for the test tone.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "daemon/src/audio_ps5.hpp"

namespace accord::daemon
{
namespace
{
// Taken from the kit's own audio port (ui/kit/platform/ps5/audio_out.cpp), which
// is proven to open on this console and firmware: user 0xff, type 0 (main),
// index 0, grain 256, 48 kHz, s16 stereo. The daemon's first attempt on the
// console used user 0x10000000 with the chat type and came back 0x80260011, so
// the values below are the ones that have a working counterpart on this exact
// machine. The two variables (user id, port type) differed at once there, so
// that failure does not yet say which of them the console rejected.
constexpr unsigned kSystemUser = 0xffu;
constexpr int kPortIndex = 0;
// The chat port of the documented enumeration, kept for --audio=voice. It is the
// one this console has not opened yet, so it stays marked unverified here.
constexpr int kTypeVoice = 2;
constexpr int kTypeMain = 0;
constexpr unsigned kRate = 48000;
constexpr unsigned kFormatS16Stereo = 1;
constexpr int kVolumeFlagAll = 0;
constexpr int kVolumeFull = 0x8000;
} // namespace

std::string_view name_of(AudioPort port) noexcept
{
    return port == AudioPort::voice ? "voice" : "main";
}

AudioPort audio_port_from_name(std::string_view name, bool &known) noexcept
{
    known = true;
    if (name == "voice")
        return AudioPort::voice;
    if (name == "main")
        return AudioPort::main;
    known = false;
    return AudioPort::voice;
}

AudioOut::~AudioOut()
{
    close();
}

bool AudioOut::open(AudioPort port) noexcept
{
    if (handle_ >= 0)
        return true;

    int code = sceAudioOutInit();
    if (code != 0)
    {
        error_.remember(AudioCall::init, code);
        return false;
    }

    const int type = port == AudioPort::voice ? kTypeVoice : kTypeMain;
    handle_ = sceAudioOutOpen(kSystemUser, type, kPortIndex, kGrainFrames, kRate,
                              kFormatS16Stereo);
    if (handle_ < 0)
    {
        error_.remember(AudioCall::open, handle_);
        handle_ = -1;
        return false;
    }

    const int volumes[2] = {kVolumeFull, kVolumeFull};
    code = sceAudioOutSetVolume(handle_, kVolumeFlagAll, volumes);
    if (code != 0)
    {
        // Not fatal: the port stays open at whatever volume it came up with.
        error_.remember(AudioCall::set_volume, code);
    }
    return true;
}

void AudioOut::submit(std::span<const std::int16_t> frames) noexcept
{
    if (handle_ < 0)
        return;

    const int code = sceAudioOutOutput(handle_, frames.data());
    if (code != 0)
        error_.remember(AudioCall::output, code);
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
