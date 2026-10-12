/*
 * Accord - Entry point of the background daemon payload.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The daemon plays a test tone, answers GET /status, POST /variant and
 * POST /quit on a local port, and appends every line to a log file. It never
 * talks to Discord, never opens TLS and never contacts the title: the one
 * question it answers is whether a payload keeps running, and keeps its audio,
 * while the console is busy with something else. At start it runs one unwind
 * self-test and then builds one MLS key package through the DAVE facade, which
 * proves that libdave, mlspp and OpenSSL are linked into the payload and that
 * their exceptions stop at the facade. Both answers go to the log, and the
 * self-test also to /status.
 *
 * The port itself is one of the variants in daemon/src/variant.hpp, and
 * POST /variant?n=N switches between them without a restart: the first console
 * run opened its port and stayed silent, and the sweep is what separates the
 * port type from the volume call the kit never makes.
 */

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>

#include "daemon/src/audio_ps5.hpp"
#include "daemon/src/config.hpp"
#include "daemon/src/http.hpp"
#include "daemon/src/log.hpp"
#include "daemon/src/server.hpp"
#include "daemon/src/tone.hpp"
#include "platform/ps5/dave/dave_facade.hpp"

namespace accord::daemon
{
namespace
{
using accord::core::LogLevel;

struct Options
{
    std::uint16_t port = kDefaultPort;
    // The payload manager starts a payload without arguments, so the default is
    // what the console sees: variant 0, the type and the volume call the kit's
    // own port uses. --audio=voice selects the chat-type variant, and
    // POST /variant?n=N walks the rest without a restart.
    std::size_t variant = 0;
    // False when --audio named something this grid cannot honour, which is worth
    // a line in the log: a payload manager's stderr goes nowhere.
    bool audio_name_known = true;
    std::string log_path = kLogPath;
};

std::uint16_t read_port(const char *text, std::uint16_t fallback) noexcept
{
    char *end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0 || value > 65535ul)
        return fallback;
    return static_cast<std::uint16_t>(value);
}

// Maps --audio=main|voice onto the variant that carries that type. An unknown
// name keeps the variant the daemon already had.
std::size_t read_variant(std::string_view name, std::size_t fallback, bool &known) noexcept
{
    const std::size_t index = audio_variant_from_name(name, known);
    return known ? index : fallback;
}

// Accepts --port=N / --port N, --audio=voice|main and --log=PATH. Anything else
// is ignored, because a payload manager may pass arguments of its own.
Options read_options(int argc, char **argv) noexcept
{
    Options options;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        const bool has_more = index + 1 < argc;

        if (argument == "--port" && has_more)
            options.port = read_port(argv[++index], options.port);
        else if (argument.starts_with("--port="))
            options.port = read_port(std::string{argument.substr(7)}.c_str(), options.port);
        else if (argument == "--audio" && has_more)
            options.variant = read_variant(argv[++index], options.variant, options.audio_name_known);
        else if (argument.starts_with("--audio="))
            options.variant =
                read_variant(argument.substr(8), options.variant, options.audio_name_known);
        else if (argument == "--log" && has_more)
            options.log_path = argv[++index];
        else if (argument.starts_with("--log="))
            options.log_path = std::string{argument.substr(6)};
    }
    return options;
}

// The generator and the port belong to this thread alone: the HTTP side only
// reads the frame counter and asks for another variant through `requested`,
// which is why both are atomic and the tone is not shared. A switch is a close
// and a fresh open, so every variant is measured on a port of its own rather
// than on one the variant before it left behind.
void play_tone(AudioOut &audio, std::atomic<bool> &running, std::atomic<std::uint64_t> &frames,
               std::atomic<bool> &idle, std::atomic<std::size_t> &requested,
               std::atomic<std::size_t> &active)
{
    Tone tone;
    std::array<std::int16_t, kGrainFrames * 2> block{};
    std::size_t current = requested.load(std::memory_order_relaxed);
    bool first_block = true;

    const auto open_current = [&]() {
        const AudioVariant &variant = audio_variant_at(current);
        const bool opened = audio.open_variant(variant);
        logf(opened ? LogLevel::info : LogLevel::error,
             "audio variant %zu (%s): user 0x%08x, type %d, volume flags %d, open code 0x%08x, "
             "volume code 0x%08x",
             current, std::string{variant.name}.c_str(), variant.user, variant.type,
             variant.volume_flags, static_cast<unsigned>(audio.open_code()),
             static_cast<unsigned>(audio.volume_code()));
        active.store(current, std::memory_order_relaxed);
    };

    open_current();

    while (running.load(std::memory_order_relaxed))
    {
        const std::size_t wanted = requested.load(std::memory_order_relaxed);
        if (wanted != current)
        {
            current = wanted;
            open_current();
        }

        if (!audio.is_open())
        {
            // A variant whose open failed has nothing to block on, so the loop
            // waits here instead of spinning while the console is asked for the
            // next one.
            ::usleep(20000);
            continue;
        }

        tone.render(block, kGrainFrames);
        const int code = audio.submit(block);
        if (first_block)
        {
            // The first console run read this answer as a failure and reported it
            // for its whole life. 256 is the grain the call has just taken, and
            // the kit's own port checks for < 0 for the same reason.
            logf(LogLevel::info, "audio: first block answered 0x%08x", static_cast<unsigned>(code));
            first_block = false;
        }
        frames.store(tone.frames_written(), std::memory_order_relaxed);
    }

    idle.store(true, std::memory_order_relaxed);
}
} // namespace
} // namespace accord::daemon

int main(int argc, char **argv)
{
    using namespace accord::daemon;
    using accord::core::LogLevel;

    const Options options = read_options(argc, argv);

    if (!open_log(options.log_path))
        std::fprintf(stderr, "daemon: cannot open the log at %s\n", options.log_path.c_str());

    logf(LogLevel::info, "daemon up: pid %d, http port %u, audio variant %zu (%s)",
         static_cast<int>(::getpid()), static_cast<unsigned>(options.port), options.variant,
         std::string{audio_variant_at(options.variant).name}.c_str());

    if (!options.audio_name_known)
        logf(LogLevel::warn,
             "audio: the name on the command line selects no variant of this grid, variant %zu "
             "stays",
             options.variant);

    // T6.0-3: first check that an exception raised inside the libraries comes back
    // to us at all, because the facade's catch (...) is worth nothing without it.
    // The answer goes to the log and into /status, so the console start of T2.3
    // settles the one question the host build cannot: libc++/libunwind there.
    const accord::ps5::dave::UnwindSelfTest unwind =
        accord::ps5::dave::run_unwind_self_test();
    logf(unwind.ok() ? LogLevel::info : LogLevel::error,
         "unwind selftest: %s (runtime_error=%s, mlspp=%s, tls_parse=%s)",
         accord::ps5::dave::to_string(unwind), unwind.runtime_error_caught ? "caught" : "missed",
         unwind.mlspp_error_caught ? "caught" : "missed",
         unwind.tls_parse_error_caught ? "caught" : "missed");

    // T6.0-2: the payload is linked with DAVE, so the daemon asks the library for
    // a key package before it touches audio or the network. The facade hands back
    // a status code rather than an exception, so a broken library cannot take the
    // daemon down here.
    const accord::ps5::dave::KeyPackageProbe dave =
        accord::ps5::dave::probe_key_package(kDaveProbeGroupId, kDaveProbeUserId);
    if (dave.status == accord::ps5::dave::Status::ok)
        logf(LogLevel::info, "dave: key package %llu bytes, protocol version %u",
             static_cast<unsigned long long>(dave.bytes),
             static_cast<unsigned>(dave.protocol_version));
    else
        logf(LogLevel::error, "dave: no key package (status %s, protocol version %u)",
             accord::ps5::dave::to_string(dave.status),
             static_cast<unsigned>(dave.protocol_version));

    // The library's own failure callback also reports what it recovers from, so
    // this line shows up even when the key package above came out fine.
    if (dave.library_reported_failure)
        logf(LogLevel::warn, "dave: library reported '%s' while probing", dave.reported_reason);

    // The port belongs to the audio thread, which opens the first variant of the
    // sweep before it plays a block. A variant whose open fails does not stop the
    // daemon: /status keeps answering and POST /variant asks for the next one.
    AudioOut audio;

    Server server;
    if (server.listen(options.port))
        logf(LogLevel::info, "listening on 0.0.0.0:%u", static_cast<unsigned>(server.port()));
    else
        logf(LogLevel::error, "listen failed: %s", std::string{server.last_error()}.c_str());

    std::atomic<std::uint64_t> frames{0};
    std::atomic<bool> running{true};
    std::atomic<bool> idle{false};
    std::atomic<std::size_t> requested{options.variant};
    std::atomic<std::size_t> active{0};
    std::thread audio_thread{play_tone,     std::ref(audio),  std::ref(running),
                             std::ref(frames), std::ref(idle), std::ref(requested),
                             std::ref(active)};

    const std::time_t started = std::time(nullptr);
    std::uint64_t requests = 0;
    bool quitting = false;

    const RequestHandler handler = [&](const RequestLine &request) -> Response {
        ++requests;

        if (request.method == Method::get && request.path == "/status")
        {
            const std::uint64_t written = frames.load(std::memory_order_relaxed);
            // Composed here rather than stored: the audio thread owns the
            // record, this thread only reads it. The view below points into
            // this string, which outlives the render call.
            const std::string audio_error = audio.last_error();

            Status status;
            status.uptime_seconds = static_cast<std::uint64_t>(std::time(nullptr) - started);
            status.audio_frames = written;
            status.audio_open = audio.is_open();
            status.tone_playing = tone_playing_at(written);
            status.port = options.port;
            // The variant the audio thread last opened, not the one that was
            // asked for: a request only takes effect once the port behind it is
            // open, and that is what the sweep is comparing.
            status.audio_variant_index = active.load(std::memory_order_relaxed);
            status.audio_variant = audio_variant_at(status.audio_variant_index).name;
            status.audio_type = audio_variant_at(status.audio_variant_index).type;
            status.audio_volume_flags = audio.volume_flags();
            status.audio_open_code = audio.open_code();
            status.audio_volume_code = audio.volume_code();
            status.last_error = audio_error.empty() ? server.last_error() : audio_error;
            status.unwind_selftest = accord::ps5::dave::to_string(unwind); // a literal, so it outlives this

            Response response;
            response.content_type = "application/json";
            response.body = render_status_json(status);
            return response;
        }

        if (request.method == Method::get && request.path == "/variant")
        {
            // The grid, so the console run does not depend on anyone remembering
            // what the names in /status mean.
            std::string body = "{\"variants\":[";
            for (std::size_t index = 0; index < kAudioVariantCount; ++index)
            {
                const AudioVariant &variant = audio_variant_at(index);
                if (index != 0)
                    body += ',';
                char user[16];
                std::snprintf(user, sizeof(user), "0x%08x", variant.user);
                body += "{\"index\":" + std::to_string(index) + ",\"user\":\"" + user +
                        "\",\"type\":" + std::to_string(variant.type) + ",\"volume_flags\":" +
                        std::to_string(variant.volume_flags) + ",\"name\":\"" +
                        std::string{variant.name} + "\"}";
            }
            body += "]}\n";

            Response response;
            response.content_type = "application/json";
            response.body = body;
            return response;
        }

        if (request.method == Method::post && request.path == "/variant")
        {
            // POST /variant?n=2 is the sweep: it says which port to open next and
            // answers at once, because the audio thread picks the request up on
            // its next block. /status is what confirms the port behind it.
            bool ok = false;
            const std::size_t index =
                audio_variant_from_text(query_value(request.query, "n"), ok);

            Response response;
            response.content_type = "application/json";
            if (!ok)
            {
                response.code = 400;
                response.reason = "Bad Request";
                response.body = "variant: use POST /variant?n=0..5, or GET /variant\n";
                return response;
            }

            requested.store(index, std::memory_order_relaxed);
            response.body = "{\"requested\":" + std::to_string(index) + ",\"name\":\"" +
                            std::string{audio_variant_at(index).name} + "\"}\n";
            return response;
        }

        if (request.method == Method::post && request.path == "/quit")
        {
            quitting = true;

            Response response;
            response.content_type = "application/json";
            response.body = "{\"quitting\":true}\n";
            return response;
        }

        Response response;
        response.code = 404;
        response.reason = "Not Found";
        response.body = "not found: try GET /status, GET /variant or POST /quit\n";
        return response;
    };

    while (!quitting)
    {
        server.poll(handler);
        ::usleep(2000);
    }

    logf(LogLevel::info, "stopping after %llu request(s)",
         static_cast<unsigned long long>(requests));

    running.store(false, std::memory_order_relaxed);

    if (audio_thread.joinable())
    {
        // A port that a game has taken over can block the audio thread inside
        // the port forever. The exit must not depend on it, so wait a second
        // and leave it behind if it is stuck.
        for (int waited = 0; waited < 100 && !idle.load(std::memory_order_relaxed); ++waited)
            ::usleep(10000);

        if (idle.load(std::memory_order_relaxed))
        {
            audio_thread.join();
            audio.close();
        }
        else
        {
            logf(LogLevel::warn, "the audio thread is stuck in the port, exiting anyway");
            std::_Exit(0);
        }
    }

    server.close();
    logf(LogLevel::info, "daemon stopped cleanly");
    close_log();
    return 0;
}
