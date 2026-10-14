/*
 * Accord - payload probe: the port types the earlier runs never tried.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The daemon and the title have no audible output as a payload, on the main and
 * the music port, in the menu and during a game: the renderer never carries a
 * port opened by a process that is not an application. PortState says so in one
 * number (outputs == 0), and the same reading taken from a title that plays says
 * 0x0081. What is left untested is the rest of the port list, and this probe
 * walks it:
 *
 *   classic port types: main 0, music 1, voice 2, personal 3, pad speaker 4
 *   AudioOut2 types:    main 0, music 1, voice 2, pad speaker 3, personal 4
 *
 * Every type is opened, its PortState is read before and after, the siren of the
 * earlier runs plays through it, and every code goes to klog. Nothing here is a
 * guessed call: the type numbers come from the published bindings and emulator
 * implementations, and the pad's user comes from sceUserServiceGetInitialUser,
 * the call the kit's own pad code makes.
 *
 * The AudioOut2 leg keeps the format the emulator implementations document and
 * the earlier probe got wrong: Gain is one float per channel, Pcm is a pointer,
 * and a frame is the context's num_grains rather than 256.
 *
 * The acceptance test is on the console, not in this file: the siren has to be
 * audible in the same headphones while somebody else's game holds the foreground,
 * and PortState has to stop reading zero.
 */

#include <ps5/klog.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "daemon/src/tone.hpp"

extern "C" int sceAudioOutInit();
extern "C" int sceAudioOutOpen(unsigned user_id, int type, int index, unsigned grain,
                               unsigned frequency, unsigned format);
extern "C" int sceAudioOutSetVolume(int handle, int flags, const int *volumes);
extern "C" int sceAudioOutOutput(int handle, const void *samples);
extern "C" int sceAudioOutGetPortState(int handle, void *state);
extern "C" int sceAudioOutClose(int handle);

extern "C" int sceAudioOut2Initialize();
extern "C" int sceAudioOut2ContextResetParam(void *param);
extern "C" int sceAudioOut2ContextQueryMemory(const void *param, std::size_t *memory_size);
extern "C" int sceAudioOut2ContextCreate(const void *param, void *buffer, std::size_t buffer_size,
                                         std::uint64_t *context);
extern "C" int sceAudioOut2ContextDestroy(std::uint64_t context);
extern "C" int sceAudioOut2ContextAdvance(std::uint64_t context);
extern "C" int sceAudioOut2ContextPush(std::uint64_t context, std::uint32_t blocking);
extern "C" int sceAudioOut2ContextGetQueueLevel(std::uint64_t context, std::uint32_t *queue_level,
                                                std::uint32_t *available);
extern "C" int sceAudioOut2PortCreate(std::uint64_t context, const void *param,
                                      std::uint64_t *port);
extern "C" int sceAudioOut2PortDestroy(std::uint64_t port);
extern "C" int sceAudioOut2PortSetAttributes(std::uint64_t port, const void *attributes,
                                             std::uint32_t count);
extern "C" int sceAudioOut2PortGetState(std::uint64_t port, void *state);
extern "C" int sceAudioOut2UserCreate(std::uint32_t user_id, std::size_t *handle);
extern "C" int sceAudioOut2UserDestroy(std::size_t handle);

extern "C" int sceUserServiceInitialize(const void *params);
extern "C" int sceUserServiceGetInitialUser(int *user);

namespace
{
using accord::daemon::Tone;

constexpr unsigned kUserSystem = 0xffu;
constexpr unsigned kLegacyGrainFrames = 256;
constexpr unsigned kRate = 48000;
constexpr unsigned kFormatS16Stereo = 1;
constexpr int kVolumeFlags = 3;
constexpr int kVolumeFull = 0x8000;

// As long as it takes to hear a sweep and a pause, and short enough that five
// types fit in one session with somebody waiting in a game.
constexpr int kLegacySecondsPerType = 15;

// AudioOut2 grains: the context's num_grains is also the frame this API takes,
// and 93 of them make a second at 48 kHz.
constexpr std::uint32_t kAo2NumGrains = 512;
constexpr int kAo2GrainsPerType = 93 * 15;

// The published port types. The numbers are the ones the bindings and the
// emulator implementations agree on, for the classic API and for AudioOut2,
// where the order of the last two is swapped.
struct LegacyType
{
    int type;
    const char *name;
    bool needs_pad_user;
};
constexpr LegacyType kLegacyTypes[] = {
    {0, "main", false},   {1, "music", false},       {2, "voice", false},
    {3, "personal", false}, {4, "pad-speaker", true},
};

struct Ao2Type
{
    std::uint16_t type;
    const char *name;
    bool needs_pad_user;
};
constexpr Ao2Type kAo2Types[] = {
    {0, "main", false},   {1, "music", false},       {2, "voice", false},
    {3, "pad-speaker", true}, {4, "personal", false},
};

// Classic PortState: 32 bytes, the same layout the title-side probe reads.
struct LegacyPortState
{
    std::uint16_t output;
    std::uint8_t channel;
    std::uint8_t reserved;
    std::int16_t volume;
    std::uint16_t reroute_counter;
    std::uint64_t flag;
    std::uint64_t reserved64[2];
};
static_assert(sizeof(LegacyPortState) == 32, "LegacyPortState ma miec 32 bajty");

struct ContextParam
{
    std::uint32_t max_ports;
    std::uint32_t max_object_ports;
    std::uint32_t guarantee_object_ports;
    std::uint32_t queue_depth;
    std::uint32_t num_grains;
    std::uint32_t flags;
    std::uint32_t reserved[10];
};
static_assert(sizeof(ContextParam) == 0x40, "ContextParam ma miec 0x40 bajtow");

struct PortParam
{
    std::uint16_t port_type;
    std::uint16_t pad;
    std::uint32_t data_format;
    std::uint32_t sampling_freq;
    std::uint32_t flags;
    std::size_t user_handle;
    std::uint32_t reserved[10];
};
static_assert(sizeof(PortParam) == 0x40, "PortParam ma miec 0x40 bajtow");

struct Attribute
{
    std::uint32_t attribute_id;
    std::uint32_t pad;
    void *value;
    std::size_t value_size;
};
static_assert(sizeof(Attribute) == 0x18, "Attribute ma miec 0x18 bajtow");

struct Pcm
{
    void *data;
};
static_assert(sizeof(Pcm) == 8, "Pcm ma byc wskaznikiem");

struct PortState2
{
    std::uint16_t output;
    std::uint8_t num_channels;
    std::uint8_t pad1;
    std::int16_t volume;
    std::uint16_t reroute_counter;
    std::uint32_t flags;
    std::uint32_t pad2;
    std::uint64_t reserved[6];
};
static_assert(sizeof(PortState2) == 0x40, "PortState2 ma miec 0x40 bajtow");

constexpr std::uint32_t kAttributePcm = 0;
constexpr std::uint32_t kAttributeGain = 1;
constexpr std::uint32_t kFormatI16Stereo = 1u | (2u << 8);
constexpr std::uint32_t kBlockingAsync = 0;

void note(const char *what, int code)
{
    klog_printf("[probe] %s = 0x%08x\n", what, static_cast<unsigned>(code));
}

void log_legacy_state(const char *type_name, const char *when, int handle)
{
    LegacyPortState state{};
    const int code = sceAudioOutGetPortState(handle, &state);
    klog_printf("[probe] legacy %s PortState(%s) = 0x%08x: outputs=0x%04x channels=%u "
                "volume=%d reroute=%u\n",
                type_name, when, static_cast<unsigned>(code), state.output, state.channel,
                state.volume, state.reroute_counter);
}

void log_ao2_state(const char *type_name, const char *when, std::uint64_t port)
{
    PortState2 state{};
    const int code = sceAudioOut2PortGetState(port, &state);
    klog_printf("[probe] AudioOut2 %s PortState(%s) = 0x%08x: outputs=0x%04x channels=%u "
                "volume=%d flags=0x%08x\n",
                type_name, when, static_cast<unsigned>(code), state.output, state.num_channels,
                state.volume, state.flags);
}

// A user that has a pad: the kit's pad code makes this call, and the SDK
// publishes it, so the number it returns is the one the speaker port wants.
int pad_user()
{
    const int init = sceUserServiceInitialize(nullptr);
    note("sceUserServiceInitialize", init);
    if (init != 0)
        return -1;

    int user = -1;
    note("sceUserServiceGetInitialUser", sceUserServiceGetInitialUser(&user));
    klog_printf("[probe] uzytkownik z padem = 0x%08x\n", static_cast<unsigned>(user));
    return user;
}

void legacy_type(int user_with_pad, const LegacyType &candidate)
{
    const unsigned user = candidate.needs_pad_user && user_with_pad >= 0
                              ? static_cast<unsigned>(user_with_pad)
                              : kUserSystem;
    klog_printf("[probe] --- legacy %s (typ %d, user 0x%02x) ---\n", candidate.name,
                candidate.type, user);

    const int handle =
        sceAudioOutOpen(user, candidate.type, 0, kLegacyGrainFrames, kRate, kFormatS16Stereo);
    klog_printf("[probe] legacy %s sceAudioOutOpen = 0x%08x\n", candidate.name,
                static_cast<unsigned>(handle));
    if (handle < 0)
        return;

    const int volumes[2] = {kVolumeFull, kVolumeFull};
    note("sceAudioOutSetVolume", sceAudioOutSetVolume(handle, kVolumeFlags, volumes));
    log_legacy_state(candidate.name, "przed graniem", handle);

    std::array<std::int16_t, kLegacyGrainFrames * 2> block{};
    Tone tone;
    const unsigned long long grains_per_second = kRate / kLegacyGrainFrames;
    const unsigned long long total =
        static_cast<unsigned long long>(kLegacySecondsPerType) * grains_per_second;

    for (unsigned long long grain = 0; grain < total; ++grain)
    {
        tone.render(block, kLegacyGrainFrames);
        const int code = sceAudioOutOutput(handle, block.data());
        if (grain == 0)
            note("pierwszy blok sceAudioOutOutput", code);
        if (grain % (grains_per_second * 5) == 0)
            log_legacy_state(candidate.name, "w trakcie grania", handle);
    }

    log_legacy_state(candidate.name, "po graniu", handle);
    note("sceAudioOutClose", sceAudioOutClose(handle));
}

void ao2_type(std::uint64_t context, int user_with_pad, const Ao2Type &candidate)
{
    const std::uint32_t user_id = candidate.needs_pad_user && user_with_pad >= 0
                                      ? static_cast<std::uint32_t>(user_with_pad)
                                      : kUserSystem;
    klog_printf("[probe] --- AudioOut2 %s (typ %u, user 0x%02x) ---\n", candidate.name,
                candidate.type, user_id);

    std::size_t user = 0;
    note("sceAudioOut2UserCreate", sceAudioOut2UserCreate(user_id, &user));
    if (user == 0)
        return;

    PortParam port_params{};
    port_params.port_type = candidate.type;
    port_params.data_format = kFormatI16Stereo;
    port_params.sampling_freq = kRate;
    port_params.user_handle = user;

    std::uint64_t port = 0;
    const int created = sceAudioOut2PortCreate(context, &port_params, &port);
    klog_printf("[probe] AudioOut2 %s sceAudioOut2PortCreate = 0x%08x\n", candidate.name,
                static_cast<unsigned>(created));
    if (created != 0 || port == 0)
    {
        sceAudioOut2UserDestroy(user);
        return;
    }

    log_ao2_state(candidate.name, "przed graniem", port);

    // Gain: one float per channel, the shape the emulator implementations check.
    float gain[2] = {1.0f, 1.0f};
    Attribute gain_attribute{kAttributeGain, 0, gain, sizeof(gain)};
    note("sceAudioOut2PortSetAttributes(Gain)",
         sceAudioOut2PortSetAttributes(port, &gain_attribute, 1));

    std::vector<std::int16_t> buffer(static_cast<std::size_t>(kAo2NumGrains) * 2, 0);
    Tone tone;
    for (int frame = 0; frame < kAo2GrainsPerType; ++frame)
    {
        tone.render(buffer, kAo2NumGrains);
        Pcm pcm{buffer.data()};
        Attribute attribute{kAttributePcm, 0, &pcm, sizeof(pcm)};

        const int set = sceAudioOut2PortSetAttributes(port, &attribute, 1);
        const int advance = sceAudioOut2ContextAdvance(context);
        const int push = sceAudioOut2ContextPush(context, kBlockingAsync);

        if (frame == 0)
        {
            klog_printf("[probe] AudioOut2 %s pierwsza klatka: set=0x%08x advance=0x%08x "
                        "push=0x%08x\n",
                        candidate.name, static_cast<unsigned>(set), static_cast<unsigned>(advance),
                        static_cast<unsigned>(push));
        }
        if (frame % (93 * 5) == 0)
        {
            std::uint32_t queued = 0;
            std::uint32_t available = 0;
            const int queue = sceAudioOut2ContextGetQueueLevel(context, &queued, &available);
            klog_printf("[probe] AudioOut2 %s klatka %d: kolejka=0x%08x czeka=%u wolne=%u\n",
                        candidate.name, frame, static_cast<unsigned>(queue), queued, available);
            log_ao2_state(candidate.name, "w trakcie grania", port);
        }
    }

    log_ao2_state(candidate.name, "po graniu", port);
    note("sceAudioOut2PortDestroy", sceAudioOut2PortDestroy(port));
    note("sceAudioOut2UserDestroy", sceAudioOut2UserDestroy(user));
}
} // namespace

int main()
{
    klog_printf("[probe] start: typy portow nieprzetestowane w poprzednich przebiegach\n");

    note("sceAudioOutInit", sceAudioOutInit());
    const int user_with_pad = pad_user();

    for (const LegacyType &candidate : kLegacyTypes)
        legacy_type(user_with_pad, candidate);

    note("sceAudioOut2Initialize", sceAudioOut2Initialize());

    ContextParam params{};
    params.max_ports = 256;
    params.max_object_ports = 256;
    params.guarantee_object_ports = 0;
    params.queue_depth = 4;
    params.num_grains = kAo2NumGrains;
    params.flags = 1;
    note("sceAudioOut2ContextResetParam", sceAudioOut2ContextResetParam(&params));

    std::size_t memory = 0;
    note("sceAudioOut2ContextQueryMemory", sceAudioOut2ContextQueryMemory(&params, &memory));

    std::uint64_t context = 0;
    if (memory != 0 && memory <= (16u << 20))
    {
        std::vector<std::uint8_t> buffer(memory, 0);
        note("sceAudioOut2ContextCreate",
             sceAudioOut2ContextCreate(&params, buffer.data(), buffer.size(), &context));
    }

    if (context != 0)
    {
        for (const Ao2Type &candidate : kAo2Types)
            ao2_type(context, user_with_pad, candidate);
        note("sceAudioOut2ContextDestroy", sceAudioOut2ContextDestroy(context));
    }
    else
    {
        klog_printf("[probe] brak kontekstu AudioOut2, typy 3 i 4 pominiete\n");
    }

    klog_printf("[probe] koniec\n");
    return 0;
}
