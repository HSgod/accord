/*
 * Accord - T2.3d-2, kroki 1 i 2: poprawiony AudioOut2 oraz proba z elewacja authid.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Probe z T2.3c zmierzyl, ze port payloadu ma `wyjscia = 0x0000` (zadne wyjscie go
 * nie niesie), a kolejka AudioOut2 nigdy nie schodzi. Ten plik robi dwie rzeczy,
 * ktore maja zrodlo w dokumentacji i w implementacjach emulatorow:
 *
 *   KROK 1 (przebieg A, bez elewacji): poprawiony cykl AudioOut2 wedlug Kyty
 *   (`libAudio2.cpp`):
 *     - `Gain` to TABLICA floatow na kanaly (stereo = 2 floaty), nie jeden float,
 *     - `Pcm` to `{ const void* data; }`, a rozmiar ramki to `num_grains` KONTEKSTU
 *       (512) x kanaly x 2 B, nie 256 ramek,
 *     - push asynchroniczny (`Async = 0`), a co klatke odczyt `GetQueueLevel`
 *       i `PortState`.
 *
 *   KROK 2 (przebieg B): elewacja authid dokladnie wedlug HearBridge-PS5
 *   (`src/creds.c:9-25`): zapis authid i 16-bajtowej maski uprawnien, ustawienie
 *   `0x4801000000000013` i uprawnien `0xff` x 16, potem te same dwa wyjscia
 *   (klasyczny BGM i poprawiony AudioOut2), na koncu przywrocenie zapisanych
 *   wartosci. Elewacja dotyczy WYLACZNIE tego procesu.
 *
 * ZAKAZ z decyzji Opusa: `sceAudioOutAttachToApplicationByPid`, `Mbus*`,
 * `PortRegister`/`Unregister`, `EnableChat`/`DisableChat` - brak zrodla, zero wywolan.
 *
 * Warunek zaliczenia jest konsolowy: syrena slyszalna w tych samych sluchawkach przy
 * cudzej grze na pierwszym planie, plus `wyjscia != 0`, jesli krok 0 potwierdzil
 * znaczenie tego pola. Sam odczyt bez dzwieku nie zalicza.
 *
 * Plik jest poza daemon/ i platform/: test nie zmienia kodu produktu. Syrena to
 * daemon/src/tone.cpp skompilowany bez zmian.
 */

#include <ps5/kernel.h>
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

namespace
{
using accord::daemon::Tone;

constexpr unsigned kUserSystem = 0xffu;
constexpr unsigned kLegacyGrainFrames = 256;
constexpr unsigned kRate = 48000;
constexpr unsigned kFormatS16Stereo = 1;
constexpr int kLegacyBgmType = 1;
constexpr int kVolumeFlags = 3;
constexpr int kVolumeFull = 0x8000;

// Cykl AudioOut2 wedlug Kyty: num_grains kontekstu to zarazem liczba ramek jednego
// pushu, a `max_object_ports` musi byc niezerowe, inaczej ContextCreate otwiera
// dodatkowy port obiektowy (Kyty: AudioOut2ContextCreate).
constexpr std::uint32_t kAo2NumGrains = 512;

// Ahid i uprawnienia dokladnie jak HearBridge-PS5 (`src/creds.h:10`, `src/creds.c:21-23`).
constexpr std::uint64_t kAuthidShellcore = 0x4801000000000013ull;

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

// Atrybut Pcm to struktura ze wskaznikiem na probki (Kyty: AudioOut2Pcm).
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

void log_legacy_state(const char *pass, const char *when, int handle)
{
    LegacyPortState state{};
    const int code = sceAudioOutGetPortState(handle, &state);
    klog_printf("[probe] %s klasyczny PortState(%s) = 0x%08x: wyjscia=0x%04x kanaly=%u "
                "glosnosc=%d reroute=%u\n",
                pass, when, static_cast<unsigned>(code), state.output, state.channel, state.volume,
                state.reroute_counter);
}

void log_ao2_state(const char *pass, const char *when, std::uint64_t port)
{
    PortState2 state{};
    const int code = sceAudioOut2PortGetState(port, &state);
    klog_printf("[probe] %s AudioOut2 PortState(%s) = 0x%08x: wyjscia=0x%04x kanaly=%u "
                "glosnosc=%d flagi=0x%08x\n",
                pass, when, static_cast<unsigned>(code), state.output, state.num_channels,
                state.volume, state.flags);
}

// Klasyczny port BGM: otwarcie, glosnosc, odczyt stanu i granie syreny przez `seconds`.
// Zwraca uchwyt albo -1.
int legacy_run(const char *pass, int seconds)
{
    note("sceAudioOutInit", sceAudioOutInit());

    const int handle =
        sceAudioOutOpen(kUserSystem, kLegacyBgmType, 0, kLegacyGrainFrames, kRate,
                        kFormatS16Stereo);
    klog_printf("[probe] %s sceAudioOutOpen(bgm) = 0x%08x\n", pass,
                static_cast<unsigned>(handle));
    if (handle < 0)
        return -1;

    const int volumes[2] = {kVolumeFull, kVolumeFull};
    note("sceAudioOutSetVolume", sceAudioOutSetVolume(handle, kVolumeFlags, volumes));
    log_legacy_state(pass, "przed graniem", handle);

    std::array<std::int16_t, kLegacyGrainFrames * 2> block{};
    Tone tone;
    const unsigned long long grains_per_second = kRate / kLegacyGrainFrames;
    const unsigned long long total = static_cast<unsigned long long>(seconds) * grains_per_second;

    for (unsigned long long grain = 0; grain < total; ++grain)
    {
        tone.render(block, kLegacyGrainFrames);
        const int code = sceAudioOutOutput(handle, block.data());
        if (grain == 0)
            note("pierwszy blok sceAudioOutOutput", code);
        if (grain % (grains_per_second * 10) == 0)
        {
            klog_printf("[probe] %s klasyczny BGM gra: ziaren %llu\n", pass, grain);
            log_legacy_state(pass, "w trakcie grania", handle);
        }
    }

    log_legacy_state(pass, "po graniu", handle);
    return handle;
}

// Poprawiony cykl AudioOut2 wedlug Kyty. Zwraca uchwyt portu albo 0.
std::uint64_t ao2_bring_up(const char *pass, std::uint64_t &context_out)
{
    context_out = 0;

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
    if (memory == 0 || memory > (16u << 20))
    {
        klog_printf("[probe] %s rozmiar kontekstu nieprawdopodobny (%llu B)\n", pass,
                    static_cast<unsigned long long>(memory));
        return 0;
    }

    std::vector<std::uint8_t> buffer(memory, 0);
    std::uint64_t context = 0;
    note("sceAudioOut2ContextCreate",
         sceAudioOut2ContextCreate(&params, buffer.data(), buffer.size(), &context));
    if (context == 0)
        return 0;

    std::size_t user = 0;
    note("sceAudioOut2UserCreate(0xff)", sceAudioOut2UserCreate(kUserSystem, &user));
    if (user == 0)
    {
        sceAudioOut2ContextDestroy(context);
        return 0;
    }

    PortParam port_params{};
    port_params.port_type = 1; // BGM
    port_params.data_format = kFormatI16Stereo;
    port_params.sampling_freq = kRate;
    port_params.user_handle = user;

    std::uint64_t port = 0;
    note("sceAudioOut2PortCreate(bgm)", sceAudioOut2PortCreate(context, &port_params, &port));
    if (port == 0)
    {
        sceAudioOut2ContextDestroy(context);
        return 0;
    }

    context_out = context;
    return port;
}

void ao2_probe(const char *pass, int frames)
{
    std::uint64_t context = 0;
    const std::uint64_t port = ao2_bring_up(pass, context);
    if (port == 0)
    {
        klog_printf("[probe] %s AudioOut2 nie dal portu\n", pass);
        return;
    }

    log_ao2_state(pass, "przed graniem", port);

    // Gain: tablica floatow na kanaly (stereo = 2 floaty). Kyty waliduje
    // `value_size == kanaly * sizeof(float)`, wiec jeden float dawal INVALID_PARAM.
    float gain[2] = {1.0f, 1.0f};
    Attribute gain_attribute{kAttributeGain, 0, gain, sizeof(gain)};
    note("sceAudioOut2PortSetAttributes(Gain 2x float)",
         sceAudioOut2PortSetAttributes(port, &gain_attribute, 1));

    // Bufor ma rozmiar num_grains kontekstu (512) x 2 kanaly x 2 B.
    const std::size_t sample_count = static_cast<std::size_t>(kAo2NumGrains) * 2;
    std::vector<std::int16_t> buffer(sample_count, 0);
    Tone tone;

    for (int frame = 0; frame < frames; ++frame)
    {
        tone.render(buffer, kAo2NumGrains);
        Pcm pcm{buffer.data()};
        Attribute attribute{kAttributePcm, 0, &pcm, sizeof(pcm)};

        const int set = sceAudioOut2PortSetAttributes(port, &attribute, 1);
        const int advance = sceAudioOut2ContextAdvance(context);
        const int push = sceAudioOut2ContextPush(context, kBlockingAsync);

        std::uint32_t queued = 0;
        std::uint32_t available = 0;
        const int queue = sceAudioOut2ContextGetQueueLevel(context, &queued, &available);

        PortState2 state{};
        const int state_code = sceAudioOut2PortGetState(port, &state);

        klog_printf("[probe] %s AudioOut2 klatka %d: set=0x%08x advance=0x%08x push=0x%08x "
                    "kolejka=0x%08x czeka=%u wolne=%u stan=0x%08x wyjscia=0x%04x\n",
                    pass, frame + 1, static_cast<unsigned>(set), static_cast<unsigned>(advance),
                    static_cast<unsigned>(push), static_cast<unsigned>(queue), queued, available,
                    static_cast<unsigned>(state_code), state.output);
    }

    log_ao2_state(pass, "po klatkach", port);
    sceAudioOut2ContextDestroy(context);
}

// Elewacja authid dokladnie wedlug HearBridge-PS5 (`src/creds.c:9-25`).
struct Creds
{
    std::uint64_t authid = 0;
    std::uint8_t caps[16] = {};
    bool have_caps = false;
    bool saved = false;
};

void elevate(const char *tag, Creds &saved)
{
    const std::uint64_t before = kernel_get_ucred_authid(-1);
    saved.authid = before;
    saved.have_caps = kernel_get_ucred_caps(-1, saved.caps) == 0;
    saved.saved = true;

    std::uint8_t caps[16];
    for (std::size_t index = 0; index < sizeof(caps); ++index)
        caps[index] = 0xff;

    kernel_set_ucred_authid(-1, kAuthidShellcore);
    kernel_set_ucred_caps(-1, caps);
    const std::uint64_t after = kernel_get_ucred_authid(-1);
    klog_printf("[probe] %s: authid 0x%llx -> 0x%llx (chciane 0x%llx), uprawnienia 0xff x16\n", tag,
                static_cast<unsigned long long>(before),
                static_cast<unsigned long long>(after),
                static_cast<unsigned long long>(kAuthidShellcore));
}

void restore(const char *tag, const Creds &saved)
{
    if (!saved.saved)
        return;
    kernel_set_ucred_authid(-1, saved.authid);
    if (saved.have_caps)
        kernel_set_ucred_caps(-1, saved.caps);
    klog_printf("[probe] %s: authid przywrocony na 0x%llx%s\n", tag,
                static_cast<unsigned long long>(kernel_get_ucred_authid(-1)),
                saved.have_caps ? "" : " (uprawnien nie przywrocono: nie udalo sie ich odczytac)");
}
} // namespace

int main()
{
    klog_printf("[probe] start: T2.3d-2 kroki 1-2 (przebieg A bez elewacji, przebieg B z elewacja)\n");

    klog_printf("[probe] === przebieg A: bez elewacji ===\n");
    const int legacy_a = legacy_run("A", 20);
    ao2_probe("A", 100);
    if (legacy_a >= 0)
        sceAudioOutClose(legacy_a);

    klog_printf("[probe] === przebieg B: elewacja authid ===\n");
    Creds saved;
    elevate("probe", saved);
    const int legacy_b = legacy_run("B", 300);
    ao2_probe("B", 100);
    if (legacy_b >= 0)
        sceAudioOutClose(legacy_b);
    restore("probe", saved);

    klog_printf("[probe] koniec obu przebiegow\n");
    return 0;
}
