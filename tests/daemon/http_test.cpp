/*
 * Accord - Tests for the daemon's HTTP surface.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tests/micro_test.hpp"

#include <string>

#include "daemon/src/http.hpp"

using namespace accord::daemon;

MICRO_TEST(request_line_parses_the_method_and_the_path)
{
    const RequestLine get = parse_request_line("GET /status HTTP/1.1");
    MICRO_CHECK(get.method == Method::get);
    MICRO_CHECK_EQ(get.path, std::string{"/status"});
    MICRO_CHECK(get.query.empty());

    const RequestLine post = parse_request_line("POST /quit HTTP/1.1");
    MICRO_CHECK(post.method == Method::post);
    MICRO_CHECK_EQ(post.path, std::string{"/quit"});
}

MICRO_TEST(request_line_keeps_the_query_apart_from_the_path)
{
    const RequestLine request = parse_request_line("GET /status?verbose=1 HTTP/1.1");
    MICRO_CHECK(request.method == Method::get);
    MICRO_CHECK_EQ(request.path, std::string{"/status"});
    MICRO_CHECK_EQ(request.query, std::string{"verbose=1"});
}

MICRO_TEST(request_line_rejects_anything_else)
{
    MICRO_CHECK(parse_request_line("").method == Method::unknown);
    MICRO_CHECK(parse_request_line("GET").method == Method::unknown);
    MICRO_CHECK(parse_request_line("DELETE /status HTTP/1.1").method == Method::unknown);
}

MICRO_TEST(status_json_lists_every_counter)
{
    Status status;
    status.uptime_seconds = 12;
    status.audio_frames = 3456;
    status.audio_open = true;
    status.tone_playing = false;
    status.port = 8280;
    status.audio_variant_index = 2;
    status.audio_variant = "type 1, no volume";
    status.audio_type = 1;
    status.audio_volume_flags = 3;
    status.audio_open_code = 0x20000000;
    status.audio_volume_code = 0;
    status.last_error = "";
    status.unwind_selftest = "ok";

    MICRO_CHECK_EQ(render_status_json(status),
                   std::string{"{\"uptime_seconds\":12,\"audio_frames\":3456,\"audio_open\":true,"
                               "\"tone_playing\":false,\"port\":8280,\"audio_variant_index\":2,"
                               "\"audio_variant\":\"type 1, no volume\",\"audio_type\":1,"
                               "\"audio_volume_flags\":3,\"audio_open_code\":\"0x20000000\","
                               "\"audio_volume_code\":\"0x00000000\",\"last_error\":\"\","
                               "\"unwind_selftest\":\"ok\"}\n"});
}

MICRO_TEST(status_json_prints_the_codes_the_way_the_console_does)
{
    Status status;
    status.audio_open_code = static_cast<int>(0x80260011u);
    status.audio_volume_code = -1;

    const std::string json = render_status_json(status);
    MICRO_CHECK(json.find("\"audio_open_code\":\"0x80260011\"") != std::string::npos);
    MICRO_CHECK(json.find("\"audio_volume_code\":\"0xffffffff\"") != std::string::npos);
}

MICRO_TEST(query_value_reads_the_key_it_was_asked_for)
{
    MICRO_CHECK_EQ(std::string{query_value("n=2", "n")}, std::string{"2"});
    MICRO_CHECK_EQ(std::string{query_value("x=1&n=5", "n")}, std::string{"5"});
    MICRO_CHECK_EQ(std::string{query_value("n=3&x=1", "n")}, std::string{"3"});
    MICRO_CHECK_EQ(std::string{query_value("n=", "n")}, std::string{});
    MICRO_CHECK_EQ(std::string{query_value("", "n")}, std::string{});
    MICRO_CHECK_EQ(std::string{query_value("m=2", "n")}, std::string{});
    // A key that merely ends with the name is not the name.
    MICRO_CHECK_EQ(std::string{query_value("on=2", "n")}, std::string{});
}

MICRO_TEST(status_json_carries_the_unwind_selftest_verdict)
{
    Status status;
    status.unwind_selftest = "ok";
    MICRO_CHECK(render_status_json(status).find("\"unwind_selftest\":\"ok\"") != std::string::npos);

    status.unwind_selftest = "failed";
    MICRO_CHECK(render_status_json(status).find("\"unwind_selftest\":\"failed\"") !=
                std::string::npos);
}

MICRO_TEST(status_json_escapes_the_error_text)
{
    Status status;
    status.last_error = "bad \"port\"\n";

    const std::string json = render_status_json(status);
    MICRO_CHECK(json.find("\"bad \\\"port\\\"\\n\"") != std::string::npos);
}

MICRO_TEST(response_carries_a_length_and_a_blank_line_before_the_body)
{
    Response response;
    response.body = "hello\n";

    const std::string text = render_response(response);
    MICRO_CHECK(text.starts_with("HTTP/1.1 200 OK\r\n"));
    MICRO_CHECK(text.find("Content-Length: 6\r\n") != std::string::npos);
    MICRO_CHECK(text.ends_with("\r\n\r\nhello\n"));
}

MICRO_TEST(response_can_carry_an_error_status)
{
    Response response;
    response.code = 404;
    response.reason = "Not Found";
    response.body = "not found\n";

    MICRO_CHECK(render_response(response).starts_with("HTTP/1.1 404 Not Found\r\n"));
}
