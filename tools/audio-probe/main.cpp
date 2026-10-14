/*
 * Accord - T2.3c krok 2, droga A: diagnostyka wyjscia audio z payloadu.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Trzy poprzednie przebiegi ustalily, ze port da sie otworzyc (klasyczny BGM i port
 * AudioOut2 po sceAudioOut2UserCreate), ale nic nie jest slychac. Zamiast zgadywac
 * kolejne kroki, ten probe CZYTA stan portu: dokumentacja biblioteki podaje
 * sceAudioOutGetPortState / sceAudioOut2PortGetState, gdzie pole Channel rowne zero
 * znaczy "nic nie niesie tego portu", a Output mowi, ktore wyjscia go niosa
 * (Primary, Headphone, External, ControllerSpeaker).
 *
 * Dodatkowo, dla AudioOut2, probe:
 *   - buduje atrybut Pcm tak, jak wymaga biblioteka: SceAudioOut2Pcm to STRUKTURA
 *     ZE WSKAZNIKIEM na probki, a nie same probki (poprzednia wersja podawala probki
 *     wprost, wiec atrybut byl zbudowany zle),
 *   - pushuje asynchronicznie (AudioOut2Blocking.Async = 0; Sync = 1 czeka i to
 *     wlasnie zablokowalo poprzednia wersje),
 *   - czyta ContextGetQueueLevel, zeby zobaczyc, czy kolejka jest oprozniana przez
 *     urzadzenie, czy stoi,
 *   - czyta SpeakerInfo, zeby wiedziec, na czym konsola w ogole gra.
 *
 * Poprawnosc tych sygnatur i struktur pochodzi z opisanej dokumentacji biblioteki
 * (SharpProspero: AudioOut.cs, AudioOut2.cs) i z implementacji emulatorow; to nie sa
 * juz zgadywane ABI. PortRegister celowo NIE jest wolany: nie ma go w dokumentacji,
 * a proba z jedna zgadywana sygnatura skonczyla sie SIGSEGV.
 *
 * Plik jest poza daemon/ i platform/ i nie zmienia zadnego pliku produktu.
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
extern "C" int sceAudioOutGetSystemState(void *state);
extern "C" int sceAudioOutClose(int handle);

extern "C" int sceAudioOut2Initialize();
extern "C" int sceAudioOut2EnableChat();
extern "C" int sceAudioOut2DisableChat();
extern "C" int sceAudioOut2ContextResetParam(void *param);
extern "C" int sceAudioOut2ContextQueryMemory(const void *param, std::size_t *memory_size);
extern "C" int sceAudioOut2ContextCreate(const void *param, void *buffer, std::size_t buffer_size,
                                         std::uint64_t *context);
extern "C" int sceAudioOut2ContextAdvance(std::uint64_t context);
extern "C" int sceAudioOut2ContextPush(std::uint64_t context, std::uint32_t blocking);
extern "C" int sceAudioOut2ContextGetQueueLevel(std::uint64_t context, std::uint32_t *queue_level,
                                                std::uint32_t *available);
extern "C" int sceAudioOut2PortCreate(std::uint64_t context, const void *param,
                                      std::uint64_t *port);
extern "C" int sceAudioOut2PortSetAttributes(std::uint64_t port, const void *attributes,
                                             std::uint32_t count);
extern "C" int sceAudioOut2PortGetState(std::uint64_t port, void *state);
extern "C" int sceAudioOut2UserCreate(std::uint32_t user_id, std::size_t *handle);
extern "C" int sceAudioOut2GetSpeakerInfo(void *info, std::uint32_t flags);

namespace
{
using accord::daemon::Tone;

constexpr unsigned kUserSystem = 0xffu;
constexpr unsigned kGrainFrames = 256;
constexpr unsigned kRate = 48000;
constexpr unsigned kFormatS16Stereo = 1;
constexpr int kVolumeFlags = 3;
constexpr int kVolumeFull = 0x8000;
constexpr int kBgmType = 1;

// Klasyczny PortState: 32 bajty (dokumentacja AudioOut.cs).
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

struct LegacySystemState
{
    float loudness;
    std::uint8_t reserved8[4];
    std::uint64_t reserved64[3];
};
static_assert(sizeof(LegacySystemState) == 32, "LegacySystemState ma miec 32 bajty");

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

// Atrybut Pcm to struktura ze wskaznikiem na probki, nie same probki.
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

struct SpeakerInfo
{
    std::uint8_t type;
    std::uint8_t pad1;
    std::uint16_t pad2;
    std::uint32_t available_bits;
    std::uint32_t flags;
    std::uint32_t pad3;
    std::int16_t speaker_angle[32];
};
static_assert(sizeof(SpeakerInfo) == 0x50, "SpeakerInfo ma miec 0x50 bajtow");

constexpr std::uint32_t kAttributePcm = 0;
constexpr std::uint32_t kFormatI16Stereo = 1u | (2u << 8);
constexpr std::uint32_t kBlockingAsync = 0;

constexpr unsigned long long kRunSeconds = 15ull * 60ull;
constexpr unsigned long long kGrainsPerSecond = kRate / kGrainFrames;

void note(const char *what, int code)
{
    klog_printf("[probe] %s = 0x%08x\n", what, static_cast<unsigned>(code));
}

void log_legacy_port_state(const char *when, int handle)
{
    LegacyPortState state{};
    const int code = sceAudioOutGetPortState(handle, &state);
    klog_printf("[probe] PortState(%s) = 0x%08x: wyjscia=0x%04x kanaly=%u glosnosc=%d "
                "reroute=%u\n",
                when, static_cast<unsigned>(code), state.output, state.channel, state.volume,
                state.reroute_counter);
}

void log_audio_out2_port_state(const char *when, std::uint64_t port)
{
    PortState2 state{};
    const int code = sceAudioOut2PortGetState(port, &state);
    klog_printf("[probe] AudioOut2 PortState(%s) = 0x%08x: wyjscia=0x%04x kanaly=%u "
                "glosnosc=%d flagi=0x%08x\n",
                when, static_cast<unsigned>(code), state.output, state.num_channels, state.volume,
                state.flags);
}

void log_system_state()
{
    LegacySystemState state{};
    const int code = sceAudioOutGetSystemState(&state);
    klog_printf("[probe] SystemState = 0x%08x: glosnosc=%.2f LKFS\n", static_cast<unsigned>(code),
                state.loudness);
}

void log_speaker_info()
{
    SpeakerInfo info{};
    const int code = sceAudioOut2GetSpeakerInfo(&info, 0);
    klog_printf("[probe] SpeakerInfo = 0x%08x: typ=%u flagi=0x%08x bity=0x%08x\n",
                static_cast<unsigned>(code), info.type, info.flags, info.available_bits);
}
} // namespace

int main()
{
    klog_printf("[probe] start: diagnostyka wyjscia (stan portu, nie sluch)\n");

    note("sceAudioOutInit", sceAudioOutInit());

    const int handle = sceAudioOutOpen(kUserSystem, kBgmType, 0, kGrainFrames, kRate,
                                       kFormatS16Stereo);
    klog_printf("[probe] sceAudioOutOpen(bgm) = 0x%08x\n", static_cast<unsigned>(handle));
    if (handle < 0)
    {
        klog_printf("[probe] port klasyczny sie nie otworzyl, koncze\n");
        return 0;
    }

    const int volumes[2] = {kVolumeFull, kVolumeFull};
    note("sceAudioOutSetVolume", sceAudioOutSetVolume(handle, kVolumeFlags, volumes));

    std::array<std::int16_t, kGrainFrames * 2> block{};
    Tone tone;
    tone.render(block, kGrainFrames);

    // Stan portu przed jakimkolwiek zapisem i po pierwszych blokach: jesli
    // urzadzenie naprawde bierze ten port, pole wyjsc i kanalow powinno byc niezerowe.
    log_legacy_port_state("przed graniem", handle);
    log_system_state();
    log_speaker_info();

    for (int index = 0; index < 20; ++index)
        sceAudioOutOutput(handle, block.data());
    log_legacy_port_state("po 20 blokach", handle);

    // AudioOut2: wlasciwy atrybut Pcm i push asynchroniczny, z odczytem kolejki.
    klog_printf("[probe] --- AudioOut2 ---\n");
    note("sceAudioOut2Initialize", sceAudioOut2Initialize());

    ContextParam params{};
    params.max_ports = 256;
    params.max_object_ports = 256;
    params.queue_depth = 4;
    params.num_grains = 512;
    params.flags = 1;
    note("sceAudioOut2ContextResetParam", sceAudioOut2ContextResetParam(&params));

    std::size_t memory = 0;
    note("sceAudioOut2ContextQueryMemory", sceAudioOut2ContextQueryMemory(&params, &memory));

    std::vector<std::uint8_t> buffer(memory, 0);
    std::uint64_t context = 0;
    note("sceAudioOut2ContextCreate",
         sceAudioOut2ContextCreate(&params, buffer.data(), buffer.size(), &context));

    std::size_t user = 0;
    note("sceAudioOut2UserCreate(0xff)", sceAudioOut2UserCreate(kUserSystem, &user));

    PortParam port_params{};
    port_params.port_type = 1; // BGM
    port_params.data_format = kFormatI16Stereo;
    port_params.sampling_freq = kRate;
    port_params.user_handle = user;

    std::uint64_t port = 0;
    note("sceAudioOut2PortCreate(bgm)", sceAudioOut2PortCreate(context, &port_params, &port));

    if (port != 0)
    {
        log_audio_out2_port_state("przed graniem", port);

        for (int index = 0; index < 100; ++index)
        {
            tone.render(block, kGrainFrames);
            Pcm pcm{block.data()};
            Attribute attribute{kAttributePcm, 0, &pcm, sizeof(pcm)};
            const int set = sceAudioOut2PortSetAttributes(port, &attribute, 1);
            const int advance = sceAudioOut2ContextAdvance(context);
            const int push = sceAudioOut2ContextPush(context, kBlockingAsync);

            if (index < 5 || index == 99)
            {
                std::uint32_t level = 0;
                std::uint32_t available = 0;
                const int queue = sceAudioOut2ContextGetQueueLevel(context, &level, &available);
                klog_printf("[probe] klatka %d: set=0x%08x advance=0x%08x push=0x%08x "
                            "kolejka=0x%08x czeka=%u wolne=%u\n",
                            index + 1, static_cast<unsigned>(set), static_cast<unsigned>(advance),
                            static_cast<unsigned>(push), static_cast<unsigned>(queue), level,
                            available);
            }
        }

        log_audio_out2_port_state("po 100 klatkach", port);
    }

    note("sceAudioOut2EnableChat", sceAudioOut2EnableChat());
    note("sceAudioOut2DisableChat", sceAudioOut2DisableChat());

    // Na koniec gra syrena klasycznym portem BGM, z okresowym odczytem stanu:
    // jesli przy grze na pierwszym planie stan portu sie zmieni, zobaczymy to w logu.
    klog_printf("[probe] gram syrene klasycznym portem BGM przez %llu s\n", kRunSeconds);
    unsigned long long grains = 0;
    while (grains < kRunSeconds * kGrainsPerSecond)
    {
        tone.render(block, kGrainFrames);
        sceAudioOutOutput(handle, block.data());
        ++grains;
        if (grains % (kGrainsPerSecond * 30) == 0)
        {
            klog_printf("[probe] gram dalej: ziaren %llu\n", grains);
            log_legacy_port_state("w trakcie grania", handle);
        }
    }

    sceAudioOutClose(handle);
    klog_printf("[probe] koniec\n");
    return 0;
}
