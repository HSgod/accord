/*
 * Accord - The little HTTP surface of the background daemon payload.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Three endpoints matter to the experiment: GET /status reports the audio
 * counters and which port variant is open, POST /variant?n=N asks the audio
 * thread for another one, and POST /quit stops the daemon without touching the
 * console. Request parsing and rendering are pure, so the host tests cover them.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace accord::daemon
{
enum class Method
{
    get,
    post,
    unknown,
};

struct RequestLine
{
    Method method = Method::unknown;
    std::string path;  // the target without its query string
    std::string query; // everything after the first '?'
};

// Parses one request line such as "GET /status HTTP/1.1". Anything that is not
// a GET or a POST comes back as Method::unknown.
RequestLine parse_request_line(std::string_view line);

// The value of `key` in a query string such as "n=2", or an empty view when the
// key is absent. No percent-decoding: the only query this daemon reads is the
// number of a port variant.
std::string_view query_value(std::string_view query, std::string_view key) noexcept;

// What GET /status reports. The codes travel as the console prints them, so
// 0x80260011 reads the same here as it does in the log.
struct Status
{
    std::uint64_t uptime_seconds = 0;
    std::uint64_t audio_frames = 0;
    bool audio_open = false;
    bool tone_playing = false;
    std::uint16_t port = 0;
    std::uint64_t audio_variant_index = 0; // into the grid of daemon/src/variant.hpp
    std::string_view audio_variant;        // the name of that variant
    int audio_type = 0;                    // the type the open call asked for
    bool audio_volume_called = false;      // whether sceAudioOutSetVolume was called
    int audio_open_code = 0;               // the handle, or the open's error code
    int audio_volume_code = 0;             // the volume call's code, 0 when not called
    std::string_view last_error;           // empty when nothing has failed
    std::string_view unwind_selftest;      // "ok" or "failed", see T6.0-3
};

std::string render_status_json(const Status &status);

struct Response
{
    int code = 200;
    std::string_view reason = "OK";
    std::string_view content_type = "text/plain; charset=utf-8";
    std::string body;
};

std::string render_response(const Response &response);
} // namespace accord::daemon
